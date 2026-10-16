import express from 'express';
import {createServer} from 'node:http';
import {createHash, timingSafeEqual} from 'node:crypto';
import {WebSocketServer} from 'ws';
import {decodePcm} from './protocol.js';

export interface EchoOptions {
  deviceToken: string;
  sessionMs?: number;
  idleMs?: number;
  maxConnections?: number;
}

/** Connectivity probe only. It never imports or calls a model provider. */
export function createEchoServer(options: EchoOptions) {
  const {deviceToken, sessionMs = 30_000, idleMs = 10_000, maxConnections = 1} = options;
  if (!/^[\x21-\x7e]{32,256}$/.test(deviceToken)) throw new Error('DEVICE_TOKEN must be 32-256 printable characters');
  if (!Number.isInteger(maxConnections) || maxConnections < 1 || maxConnections > 4 ||
      !Number.isFinite(sessionMs) || sessionMs < 100 || sessionMs > 30_000 ||
      !Number.isFinite(idleMs) || idleMs < 100 || idleMs > 10_000)
    throw new Error('Invalid echo limits');
  const digest = (s: string) => createHash('sha256').update(s).digest();
  const expected = digest(deviceToken);
  const app = express();
  app.disable('x-powered-by');
  app.get('/healthz', (_req, res) => { res.json({status:'ok', mode:'echo'}); });
  app.get('/', (_req, res) => { res.type('text/plain').send('Stackchan connectivity probe. Gemini is not used.\n'); });
  const server = createServer({maxHeaderSize:8192}, app);
  server.headersTimeout = 5000;
  server.requestTimeout = 10_000;
  const wss = new WebSocketServer({noServer:true, maxPayload:1024, perMessageDeflate:false});
  server.on('upgrade', (request, socket, head) => {
    socket.on('error', () => {});
    const reject = (status: number, name: string) => {
      socket.end(`HTTP/1.1 ${status} ${name}\r\nConnection: close\r\nContent-Length: 0\r\n\r\n`);
    };
    if (request.url !== '/live') { reject(404, 'Not Found'); return; }
    const auth = request.headers.authorization;
    if (typeof auth !== 'string' || !auth.startsWith('Bearer ') || auth.length > 263 ||
        !timingSafeEqual(digest(auth.slice(7)), expected)) {
      reject(401, 'Unauthorized'); return;
    }
    if (wss.clients.size >= maxConnections) { reject(429, 'Too Many Requests'); return; }
    wss.handleUpgrade(request, socket, head, ws => { wss.emit('connection', ws); });
  });
  wss.on('connection', ws => {
    const start = Date.now();
    let activity = start, windowAt = start, received = 0;
    let terminating = false;
    const stop = (code: number, reason: string) => {
      if (terminating) return;
      terminating = true;
      ws.close(code, reason);
      const hardClose = setTimeout(() => ws.terminate(), 1000);
      hardClose.unref();
      ws.once('close', () => clearTimeout(hardClose));
    };
    const timer = setInterval(() => {
      const now = Date.now();
      if (now - start >= sessionMs || now - activity >= idleMs) stop(1000, 'Probe complete');
    }, Math.min(100, sessionMs, idleMs));
    timer.unref();
    ws.on('error', () => stop(1011, 'Transport error'));
    ws.on('close', () => clearInterval(timer));
    ws.on('message', (raw, binary) => {
      if (terminating) return;
      const packet = Array.isArray(raw) ? Buffer.concat(raw) : Buffer.isBuffer(raw) ? raw : Buffer.from(raw);
      const now = Date.now();
      if (now - start >= sessionMs) { stop(1000, 'Probe complete'); return; }
      activity = now;
      if (now - windowAt >= 1000) { windowAt = now; received = 0; }
      received += packet.length;
      if (received > 65536 || ws.bufferedAmount > 32768) { stop(1008, 'Probe capacity exceeded'); return; }
      if (!binary) {
        try {
          const data: unknown = JSON.parse(packet.toString());
          if (data && typeof data === 'object' && 'type' in data && data.type === 'stop') {
            stop(1000, 'Probe complete'); return;
          }
        } catch { /* Untrusted text is never reflected or logged. */ }
        stop(1003, 'Unsupported probe control'); return;
      }
      try { decodePcm(packet, 640); }
      catch { stop(1003, 'Invalid PCM frame'); return; }
      ws.send(packet, {binary:true}, error => { if (error) stop(1011, 'Transport error'); });
    });
    ws.send(JSON.stringify({type:'ready', protocolVersion:1, mode:'echo', maxPcmBytes:640, sessionMs}));
  });
  return {
    server,
    close: async () => {
      for (const ws of wss.clients) ws.terminate();
      await new Promise<void>(resolve => wss.close(() => resolve()));
      await new Promise<void>((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
    },
  };
}
