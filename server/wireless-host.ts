import {createInterface} from 'node:readline';
import {isIPv4} from 'node:net';
import {once} from 'node:events';
import {createGeminiServer} from './gemini-server.js';
import {loadGeminiConfig} from './private-config.js';
import {runIsolatedTurn} from './gemini-isolated.js';
import {failureDetails} from './gemini-turn.js';

// Only parent stdin supplies pairing credentials. Never write them to stdout.
const lines=createInterface({input:process.stdin});
const report=(event:string, more={})=>console.info(JSON.stringify({event,...more}));
let close:(()=>Promise<void>)|undefined;
const timer=setTimeout(()=>process.exit(1),10000);
try {
  const [raw]=await once(lines,'line'); clearTimeout(timer);
  const value=JSON.parse(raw) as {host:string;port:number;keyHex:string;fixture?:boolean};
  const ip=value.host?.split('.').map(Number);
  if(!isIPv4(value.host) || !(ip[0]===10 || (ip[0]===172 && ip[1]!>=16 && ip[1]!<=31) || (ip[0]===192 && ip[1]===168)) ||
     !Number.isInteger(value.port) || value.port<1024 || value.port>65535 || !/^[0-9a-f]{32}$/.test(value.keyHex)) throw new Error();
  const config=value.fixture ? undefined : await loadGeminiConfig();
  let lastHealth=0;
  const app=createGeminiServer({deviceToken:value.keyHex,tlsPsk:{identity:'stackchan-v1',keyHex:value.keyHex},
    onHealth:()=>{if(Date.now()-lastHealth>10000){lastHealth=Date.now();report('device-online');}},
    runTurn:async pcm=>{
      if(!config) return {pcm:Buffer.alloc(1922),sampleRate:24000,chunks:1,elapsedMs:0};
      try {const result=await runIsolatedTurn({config,input:{pcm}});report('turn-complete');return result;}
      catch (error) {report('turn-failed',failureDetails(error));throw new Error('TURN_FAILED');}
    }});
  close=app.close;
  app.server.listen(value.port,value.host);await once(app.server,'listening');report('ready');
  let stopping=false;
  const stop=async()=>{if(stopping)return;stopping=true;await app.close();process.exit(0);};
  lines.once('close',()=>void stop());process.once('SIGTERM',()=>void stop());process.once('SIGINT',()=>void stop());
} catch {clearTimeout(timer);report('failed');lines.close();if(close)await close().catch(()=>{});process.exitCode=1;}
