import http from 'node:http';
import { DatabaseSync } from 'node:sqlite';
import { randomBytes, randomUUID, scryptSync, timingSafeEqual } from 'node:crypto';
import { mkdirSync, readFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Engine, ValidationError, demoConfig } from './engine.js';
import { Outbox, normalizeProfile, envelope, mappedPayload, defaultProfile } from './integration.js';
import { initIngestion, tokenHash, ingest } from './ingestion.js';
import { DeviceProtocol } from './protocol.js';
import { Properties, AccessError } from './properties.js';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
export function createApp({dbPath='data/pilot.sqlite',adminPassword=process.env.ADMIN_PASSWORD||'admin-demo-2026',operatorPassword=process.env.OPERATOR_PASSWORD||'operador-demo-2026',allowLocalIntegrations=process.env.ALLOW_LOCAL_INTEGRATIONS==='1',transport}={}){
 if(dbPath!==':memory:')mkdirSync(dirname(resolve(dbPath)),{recursive:true});const db=new DatabaseSync(dbPath);
 db.exec('PRAGMA journal_mode=WAL; CREATE TABLE IF NOT EXISTS state (id INTEGER PRIMARY KEY,body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS events (id INTEGER PRIMARY KEY AUTOINCREMENT,device TEXT,body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS samples (id INTEGER PRIMARY KEY AUTOINCREMENT,device TEXT,body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY,role TEXT,salt TEXT,hash TEXT); CREATE INDEX IF NOT EXISTS events_device ON events(device,id); CREATE INDEX IF NOT EXISTS samples_device ON samples(device,id);');
 db.exec('CREATE TABLE IF NOT EXISTS configuration_versions(device TEXT,revision INTEGER,status TEXT,body TEXT,published_at TEXT,applied_at TEXT,error TEXT,PRIMARY KEY(device,revision));');
 initIngestion(db);
 const sessions=new Map(),attempts=new Map();for(const [username,role,password] of [['admin','admin',adminPassword],['operador','operator',operatorPassword]])if(!db.prepare('SELECT username FROM users WHERE username=?').get(username)){const salt=randomBytes(16).toString('hex');db.prepare('INSERT INTO users(username,role,salt,hash) VALUES (?,?,?,?)').run(username,role,salt,scryptSync(password,salt,32).toString('hex'));}
 let state=JSON.parse(db.prepare('SELECT body FROM state WHERE id=1').get()?.body||JSON.stringify(Engine.fresh()));state.event_seq??=0;state.uptime??=0;
 state.integration=normalizeProfile(state.integration||{url:'',method:'POST',body:{schema_version:1,message_id:'{{message.message_id}}',device_guid:'{{device.guid}}',recorded_at:'{{message.recorded_at}}',readings:'{{message.readings}}',events:'{{message.events}}',outputs:'{{message.outputs}}',simulated:true},accepted_status:[200,201],ack_path:'accepted'});
 for(const d of state.devices){d.next_sample_uptime??=state.uptime+Math.max(0,(d.next_sample||state.now+900)-state.now);d.last_stop_uptime??=Object.fromEntries(Object.entries(d.last_stop||{}).map(([id,time])=>[id,state.uptime-Math.max(0,state.now-time)]));d.desired_revision??=d.revision;d.desired_config??=structuredClone(d.config);d.config_status??='applied';d.integration??=structuredClone(state.integration);}
 const properties=new Properties(db,state);
 let outbox;
 const engine=new Engine(state,event=>{
  event.message_id??=randomUUID();
  if(event.reason==='configuration_applied')db.prepare("UPDATE configuration_versions SET status='applied',applied_at=?,error=NULL WHERE device=? AND revision=?").run(event.recorded_at,event.device_guid,event.revision);
  if(event.kind==='configuration_rejected'){const d=state.devices.find(d=>d.guid===event.device_guid);db.prepare("UPDATE configuration_versions SET status='rejected',error=? WHERE device=? AND revision=?").run(d.config_error,d.guid,d.desired_revision);}
  db.prepare(event.sample?'INSERT INTO samples(device,body) VALUES (?,?)':'INSERT INTO events(device,body) VALUES (?,?)').run(event.device_guid,JSON.stringify(event));
  const target=state.devices.find(d=>d.guid===event.device_guid);const profile=target?.integration||state.integration;
  if(outbox&&profile.enabled&&event.source!=='integration'){
   const d=target,message=envelope(d,event,state.now);
   try{outbox.enqueue(d.guid,mappedPayload(profile,d,message),profile);}catch(e){integrationOutcome({device:d.guid,status:'dropped',message_id:message.message_id,attempts:0,reason:'template_invalid'});}
  }
 });
 function integrationOutcome(e){db.prepare('INSERT INTO events(device,body) VALUES (?,?)').run(e.device,JSON.stringify({event_id:randomUUID(),device_guid:e.device,recorded_at:new Date(state.now*1000).toISOString(),received_at:new Date().toISOString(),kind:'integration_'+e.status,reason:e.reason,message_id:e.message_id,attempts:e.attempts,source:'integration',simulated:true}));}
 outbox=new Outbox(db,{transport,allowLocal:allowLocalIntegrations,online:guid=>state.devices.find(d=>d.guid===guid)?.online,onOutcome:integrationOutcome});
 const protocol=new DeviceProtocol(db,engine);
 const save=()=>db.prepare('INSERT INTO state(id,body) VALUES(1,?) ON CONFLICT(id) DO UPDATE SET body=excluded.body').run(JSON.stringify(state));
 function mutate(fn){const previous=structuredClone(state);db.exec('BEGIN');try{const result=fn();save();db.exec('COMMIT');return result;}catch(e){db.exec('ROLLBACK');state=previous;engine.s=state;throw e;}}
 if(!db.prepare('SELECT body FROM state WHERE id=1').get())mutate(()=>{const config=properties.bind(demoConfig(),'property-default'),d=engine.add(config,'97e157ec-93ab-4d4d-93c9-2ec7077746ec',{property_id:'property-default'});d.property_id='property-default';d.area_id=config.area_id;});
 // An application restart must not resume an energized output from persisted state.
 if(state.devices.some(d=>d.transport!=='http'&&Object.keys(d.active).length))mutate(()=>{for(const d of state.devices)if(d.transport!=='http'&&Object.keys(d.active).length)engine.reboot(d);});
 for(const d of state.devices)if(d.transport==='http')d.online=false;
 for(const d of state.devices)db.prepare("INSERT OR IGNORE INTO configuration_versions VALUES(?,?,'applied',?,?,?,NULL)").run(d.guid,d.revision,JSON.stringify(d.config),new Date(state.now*1000).toISOString(),new Date(state.now*1000).toISOString());
 const publicFiles={'/':'index.html','/local':'index.html','/app.js':'app.js','/schedules.js':'schedules.js','/style.css':'style.css'};
 const handler=async(req,res)=>{
  const send=(status,body)=>{res.writeHead(status,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'});res.end(JSON.stringify(body));};
  try{
   const url=new URL(req.url,'http://localhost');const p=url.pathname;
   res.setHeader('X-Content-Type-Options','nosniff');res.setHeader('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'self'; form-action 'self'");
   if(req.method==='GET'&&publicFiles[p]){res.setHeader('Content-Type',p.endsWith('.js')?'text/javascript; charset=utf-8':p.endsWith('.css')?'text/css; charset=utf-8':'text/html; charset=utf-8');res.end(readFileSync(resolve(root,'public',publicFiles[p])));return;}
   if(req.method==='GET'&&p==='/health'){send(200,{ok:true,version:'0.5.0',simulated:true});return;}
   if(req.method!=='GET'){
    if(req.headers.origin){const origin=new URL(req.headers.origin);if(origin.host!==req.headers.host) {send(403,{error:'Origem não autorizada'});return;}}
    if(req.headers['content-type']?.split(';')[0]!=='application/json'){send(415,{error:'Envie application/json'});return;}
   }
   let body={};if(req.method!=='GET'){let chunks=[],bytes=0;for await(const chunk of req){bytes+=chunk.length;if(bytes>131072){send(413,{error:'Corpo excede 128 KB'});return;}chunks.push(chunk);}try{body=JSON.parse(Buffer.concat(chunks).toString('utf8')||'{}');}catch{send(400,{error:'JSON inválido'});return;}}
   if(p.startsWith('/iot/v1/')){
    const token=req.headers.authorization?.match(/^Bearer ([A-Za-z0-9_-]{20,200})$/)?.[1];const credential=token?db.prepare('SELECT device FROM device_credentials WHERE hash=?').get(tokenHash(token)):null;const d=credential&&state.devices.find(d=>d.guid===credential.device);if(!d){send(401,{error:'Credencial de dispositivo inválida'});return;}
    let result;
    if(req.method==='POST'&&p==='/iot/v1/telemetry')result=mutate(()=>ingest(db,state,d,body));
    else if(req.method==='GET'&&p==='/iot/v1/configuration')result=mutate(()=>protocol.configuration(d));
    else if(req.method==='POST'&&p==='/iot/v1/configuration/ack')result=mutate(()=>protocol.acknowledgeConfiguration(d,body));
    else if(req.method==='GET'&&p==='/iot/v1/commands')result=mutate(()=>protocol.commands(d));
    else if(req.method==='POST'&&p.match(/^\/iot\/v1\/commands\/[A-Za-z0-9_-]+\/events$/))result=mutate(()=>protocol.commandEvent(d,p.split('/')[4],body));
    else if(req.method==='POST'&&p==='/iot/v1/runtime')result=mutate(()=>protocol.runtime(d,body));
    else {send(404,{error:'Rota inexistente'});return;}send(200,result);return;
   }
   if(req.method==='POST'&&p==='/api/login'){
    const ip=req.socket.remoteAddress,entry=attempts.get(ip)||{count:0,until:Date.now()+60000};if(entry.until<Date.now()){entry.count=0;entry.until=Date.now()+60000;}attempts.set(ip,entry);if(entry.count>=10){send(429,{error:'Muitas tentativas; aguarde um minuto'});return;}
    const user=db.prepare('SELECT * FROM users WHERE username=?').get(typeof body.username==='string'?body.username:'');const salt=user?.salt||'invalid';const h=scryptSync(typeof body.password==='string'?body.password:'',salt,32);if(!user||!timingSafeEqual(h,Buffer.from(user.hash,'hex'))){entry.count++;send(401,{error:'Usuário ou senha inválidos'});return;}
    const token=randomBytes(32).toString('hex');sessions.set(token,{username:user.username,role:user.role,until:Date.now()+8*3600000});res.setHeader('Set-Cookie',`session=${token}; HttpOnly; SameSite=Strict; Path=/; Max-Age=28800`);send(200,{username:user.username,role:user.role});return;
   }
   const token=req.headers.cookie?.split(';').map(s=>s.trim()).find(s=>s.startsWith('session='))?.slice(8);const session=sessions.get(token);if(!session||session.until<Date.now()){send(401,{error:'Faça login'});return;}
   if(req.method==='POST'&&p==='/api/logout'){sessions.delete(token);res.setHeader('Set-Cookie','session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0');send(200,{ok:true});return;}
   const scope=properties.scope(session),authorized=()=>state.devices.filter(d=>d.property_id===scope.property.id);
   const deviceByGuid=guid=>{const d=authorized().find(d=>d.guid===guid);if(!d)throw new AccessError();return d;};
   const device=()=>deviceByGuid(body.guid);
   if(req.method==='GET'&&p==='/api/state'){send(200,{now:state.now,server_time:new Date().toISOString(),seed:state.seed,paused:state.paused,speed:state.speed,properties:properties.list(session.username),property_id:scope.property.id,areas:properties.areaList(scope.property.id),devices:authorized().map(d=>({...d,integration:{...(d.integration||state.integration),header_names:Object.keys(outbox.getHeaders(d.guid)),local_destinations_allowed:allowLocalIntegrations}})),integration:{enabled:false,url:'',header_names:[]},user:{username:session.username,role:scope.role,system_admin:scope.system_admin},simulated:true});return;}
   if(req.method==='GET'&&p==='/api/properties'){send(200,properties.list(session.username));return;}
   if(req.method==='GET'&&p==='/api/members'){if(!scope.system_admin)throw new AccessError('Somente administrador do sistema',403);send(200,properties.members(scope.property.id));return;}
   if(req.method==='GET'&&['/api/events','/api/history'].includes(p)){const table=p==='/api/events'?'events':'samples',guid=url.searchParams.get('guid'),guids=guid?[deviceByGuid(guid).guid]:authorized().map(d=>d.guid);const rows=guids.length?db.prepare(`SELECT body FROM ${table} WHERE device IN (${guids.map(()=>'?').join(',')}) ORDER BY id DESC LIMIT 500`).all(...guids):[];send(200,rows.map(r=>JSON.parse(r.body)));return;}
   if(req.method==='GET'&&p==='/api/configurations'){const d=deviceByGuid(url.searchParams.get('guid'));send(200,db.prepare('SELECT revision,status,published_at,applied_at,error FROM configuration_versions WHERE device=? ORDER BY revision DESC LIMIT 100').all(d.guid));return;}
   if(req.method==='GET'&&p==='/api/commands'){send(200,protocol.statuses(deviceByGuid(url.searchParams.get('guid'))));return;}
   if(req.method==='GET'&&p==='/api/outbox'){const guid=url.searchParams.get('guid');send(200,guid?outbox.list(deviceByGuid(guid).guid):authorized().flatMap(d=>outbox.list(d.guid)).sort((a,b)=>b.created_at-a.created_at).slice(0,100));return;}
   if(req.method!=='POST'){send(404,{error:'Rota inexistente'});return;}
   const adminOnly=['/api/config','/api/device','/api/simulation','/api/sensor','/api/network','/api/reboot','/api/integration','/api/integration/test','/api/device/credential','/api/area','/api/clock','/api/clock-valid','/api/power','/api/device/transport'];
   if(adminOnly.includes(p)&&scope.role!=='admin'){const d=authorized()[0];if(d)mutate(()=>engine.event(d,'rejected','permission_denied',{actor:session.username,route:p}));throw new AccessError('Somente administrador da propriedade',403);}
   if(['/api/simulation','/api/clock'].includes(p)&&!scope.system_admin)throw new AccessError('Relógio global: somente administrador do sistema',403);
   if(['/api/sensor','/api/network','/api/reboot','/api/clock-valid','/api/power'].includes(p)&&device().transport==='http')throw new ValidationError('Ação disponível somente no simulador interno');
   let result;
   if(p==='/api/device')result=mutate(()=>{const config=properties.bind(body.config,scope.property.id),d=engine.add(config,randomUUID(),{property_id:scope.property.id});d.property_id=scope.property.id;d.area_id=config.area_id;d.integration=defaultProfile();db.prepare("INSERT INTO configuration_versions VALUES(?,?,'applied',?,?,?,NULL)").run(d.guid,d.revision,JSON.stringify(d.config),new Date(state.now*1000).toISOString(),new Date(state.now*1000).toISOString());return d;});
   else if(p==='/api/config')result=mutate(()=>{const d=device(),result=engine.publish(d,properties.bind(body.config,scope.property.id)),time=new Date(state.now*1000).toISOString();db.prepare("UPDATE configuration_versions SET status='superseded' WHERE device=? AND status='pending'").run(d.guid);db.prepare("INSERT INTO configuration_versions VALUES(?,?,'pending',?,?,NULL,NULL)").run(d.guid,result.desired_revision,JSON.stringify(d.desired_config),time);return result;});
   else if(p==='/api/command')result=mutate(()=>{const d=device();return d.transport==='http'?protocol.queue(d,body,session.username):engine.command(d,{...body,actor:session.username});});
   else if(p==='/api/simulation')result=mutate(()=>{if(body.seconds!=null)engine.advance(body.seconds);if(body.paused!=null){if(typeof body.paused!=='boolean')throw new ValidationError('Pausa inválida');state.paused=body.paused;}if(body.speed!=null){if(![1,10,60].includes(body.speed))throw new ValidationError('Velocidade inválida');state.speed=body.speed;}return {ok:true,now:state.now};});
   else if(p==='/api/sensor')result=mutate(()=>{const d=device(),s=d.sensors[body.component_id];if(!s||!d.config.components.some(c=>c.id===body.component_id&&c.kind!=='irrigation_actuator'))throw new ValidationError('Sensor não encontrado');if(!['valid','invalid','stale','missing'].includes(body.quality)||typeof body.value!=='number'||!Number.isFinite(body.value))throw new ValidationError('Leitura inválida');const c=d.config.components.find(c=>c.id===body.component_id);if(c.kind==='soil_moisture'&&(body.value<0||body.value>100))throw new ValidationError('Umidade entre 0 e 100');Object.assign(s,{value:body.value,quality:body.quality,updated_at:state.now,updated_uptime:state.uptime||0});engine.event(d,'sensor_changed','simulation_input',{component_id:body.component_id,actor:session.username});return {ok:true};});
   else if(p==='/api/network')result=mutate(()=>{const d=device();if(typeof body.online!=='boolean')throw new ValidationError('Estado inválido');d.online=body.online;engine.event(d,body.online?'online':'offline','simulation_network');return {ok:true};});
   else if(p==='/api/reboot')result=mutate(()=>{engine.reboot(device());return {ok:true};});
   else if(p==='/api/integration')result=mutate(()=>{const profile=normalizeProfile(body.integration);if(profile.url.startsWith('http:')&&!allowLocalIntegrations)throw new ValidationError('HTTP local requer ALLOW_LOCAL_INTEGRATIONS=1');const d=body.guid?device():authorized()[0];if(!d)throw new ValidationError('Cadastre um dispositivo');if(body.headers!==undefined)outbox.setHeaders(body.headers,d.guid);d.integration=profile;return {ok:true};});
   else if(p==='/api/integration/preview'){const d=device(),profile=d.integration||state.integration,message=envelope(d,{sample:true},state.now);message.message_id='preview-message';result={simulated:true,request:{url:profile.url,method:profile.method,body:mappedPayload(profile,d,message)},note:'Prévia apenas. Nenhuma requisição externa foi enviada.'};}
   else if(p==='/api/integration/test')result=mutate(()=>{const d=device(),profile=d.integration||state.integration;if(!profile.url)throw new ValidationError('Configure uma URL');const message=envelope(d,{sample:true},state.now),payload=mappedPayload(profile,d,message);const queued=outbox.enqueue(d.guid,payload,profile);return {message_id:message.message_id,status:queued?'pending':'dropped'};});
   else if(p==='/api/device/credential')result=mutate(()=>{const d=device(),token=randomBytes(32).toString('base64url');db.prepare('INSERT INTO device_credentials VALUES(?,?) ON CONFLICT(device) DO UPDATE SET hash=excluded.hash').run(d.guid,tokenHash(token));engine.event(d,'credential_rotated','previous_token_revoked',{actor:session.username});return {guid:d.guid,token,note:'Exibido uma vez. O token anterior foi revogado.'};});
   else if(p==='/api/clock')result=mutate(()=>{engine.setClock(body.now);return {ok:true,now:state.now};});
   else if(p==='/api/clock-valid')result=mutate(()=>{engine.clockValid(device(),body.valid);return {ok:true};});
   else if(p==='/api/power')result=mutate(()=>{engine.power(device(),body.powered);return {ok:true};});
   else if(p==='/api/device/transport')result=mutate(()=>{const d=device();if(!['internal','http'].includes(body.transport))throw new ValidationError('Transporte inválido');if(Object.keys(d.active).length)throw new ValidationError('Pare o controlador antes de trocar transporte');d.transport=body.transport;d.online=body.transport==='internal';d.last_seen=null;const result=engine.publish(d,d.config);db.prepare("INSERT INTO configuration_versions VALUES(?,?,'pending',?,?,NULL,NULL)").run(d.guid,result.desired_revision,JSON.stringify(d.desired_config),new Date().toISOString());return {ok:true,...result};});
   else if(p==='/api/property')result=mutate(()=>properties.saveProperty(session,body));
   else if(p==='/api/area')result=mutate(()=>properties.saveArea(scope.property.id,body));
   else if(p==='/api/user')result=mutate(()=>properties.createUser(session,{...body,property_id:scope.property.id}));
   else if(p==='/api/member')result=mutate(()=>properties.saveMember(session,{...body,property_id:scope.property.id}));
   else if(p==='/api/property/select'){if(!properties.list(session.username).some(p=>p.id===body.property_id))throw new AccessError();session.property_id=body.property_id;result={ok:true};}
   else {send(404,{error:'Rota inexistente'});return;}
   send(200,result);
  }catch(e){if(e instanceof AccessError)send(e.status,{error:e.message});else if(e instanceof ValidationError)send(400,{error:e.message});else{console.error(e);send(500,{error:'Erro interno; nenhuma confirmação de gravação foi emitida'});}}
 };
 const server=http.createServer(handler);const timer=setInterval(()=>{if(!state.paused)try{mutate(()=>engine.advance(state.speed));}catch(e){state.paused=true;console.error(e);}},1000);timer.unref();
 const watcher=setInterval(()=>{try{for(const d of state.devices.filter(d=>d.transport==='http')){const now=Date.now();if(d.online&&(!d.last_seen||now-Date.parse(d.last_seen)>90000))mutate(()=>{d.online=false;engine.event(d,'offline','heartbeat_timeout',{recorded_at:new Date().toISOString(),source:'protocol'});});mutate(()=>protocol.expire(d));}}catch(e){console.error('protocol_watcher_error');}},1000);watcher.unref();
 const sender=setInterval(()=>outbox.flush().catch(e=>console.error('integration_worker_error',e.code||'internal')),500);sender.unref();
 return {server,engine,db,outbox,protocol,close:()=>{clearInterval(timer);clearInterval(sender);clearInterval(watcher);outbox.close();server.close();db.close();}};

}
if(process.argv[1]===fileURLToPath(import.meta.url)){const app=createApp({dbPath:process.env.DB_PATH||'data/pilot.sqlite'});const port=Number(process.env.PORT||8080),host=process.env.HOST||'127.0.0.1';app.server.listen(port,host,()=>console.log(`IoT Smart Irrigation: http://${host}:${port} | portal local: /local | SIMULAÇÃO | Node 24+`));process.on('SIGINT',()=>{app.close();process.exit(0);});}
