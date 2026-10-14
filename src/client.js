import {DatabaseSync} from 'node:sqlite';import {randomUUID} from 'node:crypto';import {mkdirSync} from 'node:fs';import {dirname} from 'node:path';import {fileURLToPath} from 'node:url';
import {Engine} from './engine.js';import {Outbox,normalizeProfile,envelope} from './integration.js';
import {controllerStatus} from './diagnostics.js';
import {openVault,fieldContext} from './secrets.js';
export function createDeviceClient({url,guid,token,dbPath=`data/client-${guid}.sqlite`,log=console.log,keyFile=process.env.VAULT_KEY_FILE,vaultRequired=process.env.VAULT_REQUIRED==='1'}={}){
 if(!url||!guid||!token)throw Error('Defina DEVICE_URL, DEVICE_GUID e DEVICE_TOKEN');const base=new URL(url);if(!['http:','https:'].includes(base.protocol)||base.username||base.password||base.search||base.hash)throw Error('URL inválida');const endpoint=base.origin;
 if(dbPath!==':memory:')mkdirSync(dirname(dbPath),{recursive:true});const db=new DatabaseSync(dbPath);let vault;try{vault=openVault(db,{keyFile,required:vaultRequired});}catch(e){db.close();throw e;}db.exec('CREATE TABLE IF NOT EXISTS controller(id INTEGER PRIMARY KEY,body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS command_acks(seq INTEGER PRIMARY KEY AUTOINCREMENT,id TEXT,status TEXT,body TEXT,UNIQUE(id,status));');
 let state=JSON.parse(vault.decode(db.prepare('SELECT body FROM controller WHERE id=1').get()?.body||vault.encode(JSON.stringify(Engine.fresh()),fieldContext('controller',1)),fieldContext('controller',1)));state.paused=!!state.recovery_hold;let closed=false,busy=false,closeStarted=false,polling,engine,outbox;
 if(state.devices.length>1||state.devices.some(d=>d.guid!==guid)){db.close();throw Error('GUID não corresponde ao banco do cliente');}
 const save=()=>db.prepare('INSERT INTO controller VALUES(1,?) ON CONFLICT(id) DO UPDATE SET body=excluded.body').run(vault.encode(JSON.stringify(engine.s),fieldContext('controller',1)));
 const profile=normalizeProfile({url:endpoint+'/iot/v1/telemetry',method:'POST',body:{message_id:'{{message.message_id}}'},accepted_status:[200],ack_path:'accepted',queue_limit:300,max_attempts:5});
 const api=async(path,body)=>{const response=await fetch(endpoint+path,{method:body===undefined?'GET':'POST',headers:{Authorization:'Bearer '+token,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(5000)});const data=await response.json();if(!response.ok)throw Error(data.error||'HTTP '+response.status);return data;};
 function ack(id,status,reason,executed_at){db.prepare('INSERT OR IGNORE INTO command_acks(id,status,body) VALUES(?,?,?)').run(id,status,JSON.stringify({status,reason,executed_at}));}
 engine=new Engine(state,event=>{event.event_id=randomUUID();const d=engine.s.devices[0];if(!d||!outbox)return;outbox.enqueue(guid,envelope(d,event,engine.s.now),profile);if(event.command_id&&['started','completed','interrupted','rejected','expired'].includes(event.kind))ack(event.command_id,event.kind,event.reason,event.recorded_at);});
 outbox=new Outbox(db,{vault,allowLocal:base.hostname==='127.0.0.1'||base.hostname==='localhost'||base.hostname==='[::1]'||process.env.ALLOW_LOCAL_INTEGRATIONS==='1',onOutcome:e=>log('telemetria: '+e.status+' / '+e.reason)});outbox.setHeaders({Authorization:'Bearer '+token},guid);
 if(state.devices.length&&!state.recovery_hold){const d=state.devices[0];engine.reboot(d);const previous=state.now;state.now=Math.floor(Date.now()/1000);if(state.now>previous)engine.missed(d,previous,state.now,'restart_missed');save();}
 async function flushAcks(){if(state.recovery_hold)return;for(const row of db.prepare('SELECT * FROM command_acks ORDER BY seq LIMIT 20').all()){const r=await api('/iot/v1/commands/'+row.id+'/events',JSON.parse(row.body));if(r.accepted||r.status==='expired')db.prepare('DELETE FROM command_acks WHERE seq=?').run(row.seq);}}
 function poll(){
  if(closed)return Promise.resolve();
  // Explicit callers wait for contention, then run their requested fresh cycle.
  // The periodic worker skips busy cycles instead of building an unbounded queue.
  if(busy)return polling.then(()=>poll());
  busy=true;polling=performPoll();return polling;
 }
 async function performPoll(){try{
  const config=await api('/iot/v1/configuration');if(closed)return;if(config.device_guid!==guid)throw Error('GUID não corresponde à credencial');let d=engine.s.devices[0];
  if(state.recovery_hold){
   if(config.configuration.components.some(c=>c.kind==='irrigation_actuator'&&c.enabled)||config.configuration.schedules.some(s=>s.enabled))throw Error('recovery_requires_disabled_configuration');
   if(!d){state.now=Math.floor(Date.now()/1000);d=engine.add(config.configuration,guid);}else engine.configure(d,config.configuration,config.revision);d.desired_revision=config.revision;d.desired_config=structuredClone(config.configuration);d.config_status='applied';save();
   await api('/iot/v1/configuration/ack',{revision:d.revision,configuration_hash:config.configuration_hash,status:'applied',idle:true});
   for(const [id,command] of Object.entries(d.commands))if(command.status==='started'){command.status='interrupted';ack(id,'interrupted','restore_quarantine',new Date().toISOString());}
   d.archived=false;d.online=true;state.recovery_hold=false;state.paused=false;save();
   await flushAcks();await api('/iot/v1/runtime',{controller_status:controllerStatus(engine,d),active_groups:[],runs:[]});return;
  }
  if(!d){state.now=Math.floor(Date.now()/1000);d=engine.add(config.configuration,guid);d.revision=config.revision;d.desired_revision=config.revision;d.desired_config=config.configuration;outbox.enqueue(guid,envelope(d,{sample:true},state.now),profile);save();}
  if(d.revision!==config.revision&&!Object.keys(d.active).length){try{engine.configure(d,config.configuration,config.revision);save();}catch(e){await api('/iot/v1/configuration/ack',{revision:config.revision,configuration_hash:config.configuration_hash,status:'rejected',reason:e.message});throw e;}}
  if(d.revision===config.revision&&!Object.keys(d.active).length)await api('/iot/v1/configuration/ack',{revision:d.revision,configuration_hash:config.configuration_hash,status:'applied',idle:true});
  await flushAcks();const response=await api('/iot/v1/commands');
  for(const command of response.commands){if(closed)break;if(command.reconcile_only){const old=d.commands[command.command_id];ack(command.command_id,old?.status==='started'?'started':old?.status||'expired','reconciliation',old?.status==='started'&&d.active[command.zone_id]?new Date(d.active[command.zone_id].started_at*1000).toISOString():undefined);continue;}const received=await api('/iot/v1/commands/'+command.command_id+'/events',{status:'received'});if(!received.accepted)continue;if(command.configuration_revision!==d.revision){ack(command.command_id,'rejected','configuration_revision_mismatch');continue;}const remaining=command.expires_at-Math.floor(Date.now()/1000);engine.command(d,{...command,origin:'local',expires_at:engine.s.now+remaining});save();const old=d.commands[command.command_id];if(old&&['completed','interrupted','rejected','expired'].includes(old.status))ack(command.command_id,old.status,'replay_state');}
  await flushAcks();await api('/iot/v1/runtime',{controller_status:controllerStatus(engine,d),active_groups:Object.keys(d.active),runs:Object.entries(d.active).map(([group_id,run])=>({group_id,...run}))});await outbox.flush();
 }catch(e){if(!closed)log('cliente: '+e.message);}finally{busy=false;}}
 const tick=setInterval(()=>{if(closed||state.recovery_hold||!state.devices.length)return;try{engine.advance(1);save();}catch(e){log('controle: '+e.message);}},1000),network=setInterval(()=>{if(!busy)poll();},1000);tick.unref();network.unref();
 return {engine,db,outbox,poll,advance:seconds=>{if(state.recovery_hold)throw Error('recovery_hold');engine.advance(seconds);save();},close:async()=>{if(closeStarted)return;closeStarted=true;closed=true;clearInterval(tick);clearInterval(network);if(polling)await polling;for(const z of state.recovery_hold?[]:state.devices[0]?.config.zones||[])engine.stop(state.devices[0],z,'client_shutdown');save();try{await flushAcks();if(!state.recovery_hold)await outbox.flush();}catch{}closed=true;outbox.close();db.close();}};
}
if(process.argv[1]===fileURLToPath(import.meta.url)){const client=createDeviceClient({url:process.env.DEVICE_URL,guid:process.env.DEVICE_GUID,token:process.env.DEVICE_TOKEN,dbPath:process.env.CLIENT_DB_PATH||`data/client-${process.env.DEVICE_GUID}.sqlite`});console.log('Cliente ESP32 simulado independente iniciado. Nenhum hardware é acionado.');const keepAlive=setInterval(()=>{},60000);client.poll();process.on('SIGINT',async()=>{clearInterval(keepAlive);await client.close();process.exit(0);});}
