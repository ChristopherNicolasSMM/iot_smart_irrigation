import assert from 'node:assert/strict';
import {mkdtempSync,rmSync,statSync,existsSync} from 'node:fs';
import {tmpdir,cpus} from 'node:os';import {join} from 'node:path';import {performance,monitorEventLoopDelay} from 'node:perf_hooks';
import {createApp} from '../src/server.js';import {createKeyFile} from '../src/secrets.js';import {defaultProfile} from '../src/integration.js';import {Engine,demoConfig} from '../src/engine.js';
// Reproducible bounded soak. Real wall time + accelerated controller time are
// reported separately. All databases, keys and payloads stay in temporary files.
const duration=Number(process.env.SOAK_SECONDS||120),maxMessages=Number(process.env.SOAK_MESSAGES||6000);
if(!Number.isInteger(duration)||duration<5||duration>3600||!Number.isInteger(maxMessages)||maxMessages<100||maxMessages>100000)throw Error('Limites: SOAK_SECONDS 5..3600 e SOAK_MESSAGES 100..100000');
const dir=mkdtempSync(join(tmpdir(),'irrigation-soak-')),dbPath=join(dir,'portal.sqlite'),keyFile=join(dir,'key.vault-key.json');createKeyFile(keyFile);
let app,url,successful=0,restarted=false,duplicates=0,deliveries=0,dropped=0,started=performance.now(),latencies=[],memory=[],keys=[],offline=false,lastPacket;
const transport=async(_p,body)=>{if(offline)throw Error('timeout');deliveries++;return {status:200,body:{accepted:true,message_id:body.message_id}};};
async function boot(){app=createApp({dbPath,keyFile,transport});await new Promise(r=>app.server.listen(0,'127.0.0.1',r));url=`http://127.0.0.1:${app.server.address().port}`;}
async function request(path,body,auth){const headers={'Content-Type':'application/json',...(auth?.cookie?{Cookie:auth.cookie}:{Authorization:'Bearer '+auth})};const response=await fetch(url+path,{method:body===undefined?'GET':'POST',headers,body:body===undefined?undefined:JSON.stringify(body)});const data=await response.json();assert.equal(response.status,200,JSON.stringify(data));return {data,cookie:response.headers.get('set-cookie')?.split(';')[0]};}
const lag=monitorEventLoopDelay({resolution:20});lag.enable();
try{
 await boot();const auth=await request('/api/login',{username:'admin',password:'admin-demo-2026'},{});const first=app.engine.s.devices[0];
 const config=demoConfig('static');config.schedules=[];
 for(let n=0;n<4;n++){
  const d=n===0?first:(await request('/api/device',{config:{...config,name:'Soak '+n}},auth)).data;
  await request('/api/config',{guid:d.guid,config},auth);await request('/api/simulation',{seconds:1},auth);await request('/api/device/transport',{guid:d.guid,transport:'http'},auth);
  const token=(await request('/api/device/credential',{guid:d.guid},auth)).data.token;
  await request('/api/integration',{guid:d.guid,integration:{...defaultProfile(),url:'https://soak-receiver.example',enabled:true,queue_limit:25,max_attempts:2}},auth);
  keys.push({guid:d.guid,token,revision:app.engine.s.devices.find(x=>x.guid===d.guid).revision});
 }
 started=performance.now();const until=started+duration*1000;
 while(performance.now()<until){
  const elapsed=(performance.now()-started)/(duration*1000);offline=elapsed>=0.15&&elapsed<0.4;
  if(!restarted&&elapsed>=0.5){app.close();await boot();restarted=true;assert.equal((await request('/iot/v1/configuration',undefined,keys[0].token)).data.device_guid,keys[0].guid);if(lastPacket){assert.equal((await request('/iot/v1/telemetry',lastPacket.body,lastPacket.token)).data.duplicate,true);duplicates++;}}
  if(successful<Math.min(maxMessages,Math.floor(elapsed*maxMessages)+10)){
   for(let n=0;n<10&&successful<maxMessages;n++){
    const d=keys[successful%keys.length],time=new Date().toISOString(),body={schema_version:1,message_id:'soak-'+successful,device_guid:d.guid,configuration_revision:d.revision,recorded_at:time,simulated:true,readings:[{component_id:'soil_1',value:successful%100,quality:'valid',recorded_at:time},{component_id:'temp_1',value:24,quality:'valid',recorded_at:time}],events:[],outputs:[]};
    const begin=performance.now(),r=await request('/iot/v1/telemetry',body,d.token);latencies.push(performance.now()-begin);if(r.data.forwarding.status==='dropped')dropped++;
    if(successful%20===0){assert.equal((await request('/iot/v1/telemetry',body,d.token)).data.duplicate,true);duplicates++;}
    if(successful%50===0)assert.equal((await request('/iot/v1/configuration',undefined,d.token)).data.device_guid,d.guid);
    lastPacket={body,token:d.token};successful++;
   }
  }
  await app.outbox.flush();
  if(memory.length===0||performance.now()-memory.at(-1).at>=1000){global.gc?.();memory.push({at:performance.now(),...process.memoryUsage()});}
  await new Promise(r=>setTimeout(r,50));
 }
 offline=false;app.db.prepare("UPDATE outbox SET next_at=0 WHERE status='pending'").run();for(let n=0;n<150;n++)await app.outbox.flush();
 assert.equal(app.db.prepare('SELECT COUNT(*) n FROM inbox').get().n,successful);assert.equal(app.db.prepare('SELECT COUNT(*) n FROM samples').get().n,successful);
 const queues=app.db.prepare('SELECT device,COUNT(*) n FROM outbox WHERE status IN (\'pending\',\'sending\') GROUP BY device').all();assert.ok(queues.every(x=>x.n<=25));assert.equal(app.db.prepare('PRAGMA integrity_check').get().integrity_check,'ok');assert.ok(restarted);
 const state=Engine.fresh();let samples=0,events=0;const engine=new Engine(state,event=>{if(event.sample)samples++;else events++;});for(let n=0;n<4;n++)engine.add(demoConfig('static'),'virtual-'+n);const virtualStart=performance.now();for(let day=0;day<7;day++)engine.advance(86400);assert.equal(samples,4*7*96);assert.ok(state.devices.every(d=>Object.keys(d.active).length===0));
 const p95=values=>{const sorted=[...values].sort((a,b)=>a-b);return sorted[Math.min(sorted.length-1,Math.floor(sorted.length*0.95))];};
 const size=suffix=>existsSync(dbPath+suffix)?statSync(dbPath+suffix).size:0;lag.disable();
 console.log(JSON.stringify({version:'0.12.0',node:process.version,cpu:cpus()[0]?.model,wall_seconds:+((performance.now()-started)/1000).toFixed(2),requested_soak_seconds:duration,devices:4,unique_messages:successful,duplicate_retries:duplicates,delivered_attempts:deliveries,dropped_messages:dropped,portal_restarts:1,sqlite_bytes:size(''),wal_bytes:size('-wal'),http_p95_ms:+p95(latencies).toFixed(3),heap_peak_mib:+(Math.max(...memory.map(x=>x.heapUsed))/1048576).toFixed(2),rss_peak_mib:+(Math.max(...memory.map(x=>x.rss))/1048576).toFixed(2),heap_last_mib:+(memory.at(-1).heapUsed/1048576).toFixed(2),event_loop_p95_ms:+(lag.percentile(95)/1e6).toFixed(3),virtual_days:7,virtual_devices:4,virtual_samples:samples,virtual_events:events,virtual_cpu_seconds:+((performance.now()-virtualStart)/1000).toFixed(2),terminal_counts:app.db.prepare('SELECT status,COUNT(*) count FROM outbox GROUP BY status').all(),integrity:'ok',bounded_pending_queues:true},null,2));
}finally{lag.disable();app?.close();rmSync(dir,{recursive:true,force:true});}
