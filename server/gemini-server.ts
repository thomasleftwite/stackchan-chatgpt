import {createServer} from 'node:http';
import {createHash, timingSafeEqual} from 'node:crypto';
import {WebSocketServer, WebSocket} from 'ws';
import {decodePcm, encodePcm} from './protocol.js';
import type {TurnResult} from './gemini-turn.js';

export interface GeminiServerOptions {
  deviceToken: string;
  runTurn: (pcm: Buffer) => Promise<TurnResult>;
  sessionMs?: number;
}

/** Initial half-duplex protocol v2: one bounded utterance/response per connection. */
export function createGeminiServer(options: GeminiServerOptions) {
  if (!/^[\x21-\x7e]{32,256}$/.test(options.deviceToken)) throw new Error('Invalid device token');
  const sessionMs = options.sessionMs ?? 90_000;
  if (!Number.isFinite(sessionMs) || sessionMs < 100 || sessionMs > 90_000) throw new Error('Invalid deadline');
  const digest = (value: string) => createHash('sha256').update(value).digest();
  const expected = digest(options.deviceToken);
  const server = createServer({maxHeaderSize:8192}, (req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.writeHead(req.url === '/healthz' ? 200 : 404);
    res.end(JSON.stringify(req.url === '/healthz' ? {status:'ok', mode:'gemini-live', protocolVersion:2} : {error:'not-found'}));
  });
  server.headersTimeout = 5000; server.requestTimeout = 10_000;
  const wss = new WebSocketServer({noServer:true, maxPayload:1024, perMessageDeflate:false});
  let busy = false;
  server.on('upgrade', (req, socket, head) => {
    socket.on('error', () => {});
    const reject = (code: number) => socket.end(`HTTP/1.1 ${code} Rejected\r\nConnection: close\r\nContent-Length: 0\r\n\r\n`);
    if (req.url !== '/live') { reject(404); return; }
    const auth = req.headers.authorization;
    if (typeof auth !== 'string' || !auth.startsWith('Bearer ') || auth.length > 263 ||
        !timingSafeEqual(digest(auth.slice(7)), expected)) { reject(401); return; }
    if (wss.clients.size || busy) { reject(429); return; }
    wss.handleUpgrade(req, socket, head, ws => wss.emit('connection', ws));
  });
  wss.on('connection', ws => {
    let phase: 'input' | 'generating' | 'output' | 'draining' | 'closed' = 'input';
    let generation = 0, inputSequence = 0, inputBytes = 0, outputSequence = 0, outputSamples = 0;
    let output: Buffer | undefined;
    const input: Buffer[] = [];
    let activity = Date.now(), windowStart = activity, windowBytes = 0;
    const started = activity;
    let hardClose: ReturnType<typeof setTimeout> | undefined;
    const clearAudio = () => { for (const pcm of input) pcm.fill(0); input.length = 0; output?.fill(0); output = undefined; };
    const stop = (code: number, reason: string) => {
      if (phase === 'closed') return;
      phase = 'closed'; clearAudio(); ws.close(code, reason);
      hardClose = setTimeout(() => ws.terminate(), 1000); hardClose.unref();
    };
    const json = (value: object) => { if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(value)); };
    const timer = setInterval(() => {
      if (Date.now() - started >= sessionMs || (phase === 'input' && Date.now() - activity > 10_000)) stop(1008, 'Session deadline');
    }, 100);
    timer.unref();
    ws.on('error', () => stop(1011, 'Transport failure'));
    ws.on('close', () => { phase = 'closed'; clearInterval(timer); clearTimeout(hardClose); clearAudio(); });
    ws.on('message', (raw, binary) => {
      if (phase === 'closed') return;
      const packet = Array.isArray(raw) ? Buffer.concat(raw) : Buffer.isBuffer(raw) ? raw : Buffer.from(raw);
      activity = Date.now();
      if (activity - windowStart >= 1000) { windowStart = activity; windowBytes = 0; }
      windowBytes += packet.length;
      if (windowBytes > 65_536 || ws.bufferedAmount > 32_768) { stop(1008, 'Capacity exceeded'); return; }
      try {
        if (binary) {
          if (phase !== 'input') throw new Error();
          const frame = decodePcm(packet, 640);
          if (inputSequence >= 250 || frame.sequence !== inputSequence || (generation && generation !== frame.generation) || inputBytes + frame.pcm.length > 160_000) throw new Error();
          generation = frame.generation; ++inputSequence; inputBytes += frame.pcm.length;
          input.push(Buffer.from(frame.pcm));
          json({type:'input_ack', generation, sequence:frame.sequence});
          return;
        }
        const control: unknown = JSON.parse(packet.toString('utf8'));
        if (!control || typeof control !== 'object' || !('type' in control)) throw new Error();
        if (control.type === 'stop') { stop(1000, 'Stopped'); return; }
        if (!('generation' in control) || control.generation !== generation) throw new Error();
        if (control.type === 'input_end' && phase === 'input' && inputBytes) {
          phase = 'generating'; busy = true;
          const pcm = Buffer.concat(input);
          for (const chunk of input) chunk.fill(0); input.length = 0;
          json({type:'thinking', generation});
          void Promise.resolve().then(() => options.runTurn(pcm)).then(result => {
            if (phase === 'closed') { result.pcm.fill(0); return; }
            if (result.sampleRate !== 24000 || !result.pcm.length || result.pcm.length % 2 || result.pcm.length > 480_000) {
              result.pcm.fill(0); stop(1011, 'Invalid provider audio'); return;
            }
            output = result.pcm; outputSamples = output.length / 2; phase = 'output';
            json({type:'output_start', generation, sampleRate:24000, samples:outputSamples, frames:Math.ceil(output.length / 960)});
          }).catch(() => { if (phase !== 'closed') { json({type:'error', code:'GEMINI_FAILED'}); stop(1011, 'Gemini failed'); } })
            .finally(() => { pcm.fill(0); busy = false; });
          return;
        }
        if (control.type === 'next' && phase === 'output' && 'sequence' in control && control.sequence === outputSequence && output) {
          const offset = outputSequence * 960;
          const frame = encodePcm(generation, outputSequence++, output.subarray(offset, offset + 960));
          output.fill(0, offset, Math.min(offset + 960, output.length));
          const last = outputSequence * 960 >= output.length;
          if (last) phase = 'draining';
          ws.send(frame, {binary:true}, error => {
            frame.fill(0);
            if (error) { stop(1011, 'Transport failure'); return; }
            if (last && phase !== 'closed') json({type:'output_end', generation, samples:outputSamples});
          });
          return;
        }
        if (control.type === 'played' && phase === 'draining' && 'samples' in control && control.samples === outputSamples) {
          json({type:'complete', generation}); stop(1000, 'Turn complete'); return;
        }
        throw new Error();
      } catch { stop(1003, 'Invalid protocol'); }
    });
    json({type:'ready', mode:'gemini-live', protocolVersion:2, inputRate:16000, outputRate:24000,
      maxInputSamples:80_000, maxOutputSamples:240_000});
  });
  return {server, close: async () => {
    for (const ws of wss.clients) ws.terminate();
    await new Promise<void>(resolve => wss.close(() => resolve()));
    await new Promise<void>(resolve => server.close(() => resolve()));
  }};
}
