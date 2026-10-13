import http from 'node:http';
import { DatabaseSync } from 'node:sqlite';
import { randomBytes, scryptSync, timingSafeEqual } from 'node:crypto';
import { mkdirSync, readFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Engine, ValidationError, demoConfig } from './engine.js';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
export function createApp({dbPath='data/pilot.sqlite',adminPassword=process.env.ADMIN_PASSWORD||'admin-demo-2026',operatorPassword=process.env.OPERATOR_PASSWORD||'operador-demo-2026'}={}){
 if(dbPath!==':memory:')mkdirSync(dirname(resolve(dbPath)),{recursive:true});const db=new DatabaseSync(dbPath);
 db.exec('PRAGMA journal_mode=WAL; CREATE TABLE IF NOT EXISTS state (id INTEGER PRIMARY KEY,body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS events (id INTEGER PRIMARY KEY AUTOINCREMENT,device TEXT,body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS samples (id INTEGER PRIMARY KEY AUTOINCREMENT,device TEXT,body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY,role TEXT,salt TEXT,hash TEXT); CREATE INDEX IF NOT EXISTS events_device ON events(device,id); CREATE INDEX IF NOT EXISTS samples_device ON samples(device,id);');
 const sessions=new Map(),attempts=new Map();for(const [username,role,password] of [['admin','admin',adminPassword],['operador','operator',operatorPassword]])if(!db.prepare('SELECT username FROM users WHERE username=?').get(username)){const salt=randomBytes(16).toString('hex');db.prepare('INSERT INTO users VALUES (?,?,?,?)').run(username,role,salt,scryptSync(password,salt,32).toString('hex'));}
 let state=JSON.parse(db.prepare('SELECT body FROM state WHERE id=1').get()?.body||JSON.stringify(Engine.fresh()));state.event_seq??=0;state.integration??={url:'',method:'POST',body:{device_guid:'{{device.guid}}',message_id:'{{message.id}}'},accepted_status:[200,201],ack_path:'accepted'};
 const engine=new Engine(state,event=>db.prepare(event.sample?'INSERT INTO samples(device,body) VALUES (?,?)':'INSERT INTO events(device,body) VALUES (?,?)').run(event.device_guid,JSON.stringify(event)));
 const save=()=>db.prepare('INSERT INTO state(id,body) VALUES(1,?) ON CONFLICT(id) DO UPDATE SET body=excluded.body').run(JSON.stringify(state));
 function mutate(fn){const previous=structuredClone(state);db.exec('BEGIN');try{const result=fn();save();db.exec('COMMIT');return result;}catch(e){db.exec('ROLLBACK');state=previous;engine.s=state;throw e;}}
 if(!db.prepare('SELECT body FROM state WHERE id=1').get())mutate(()=>engine.add(demoConfig(),'97e157ec-93ab-4d4d-93c9-2ec7077746ec'));
 // An application restart must not resume an energized output from persisted state.
 if(state.devices.some(d=>Object.keys(d.active).length))mutate(()=>{for(const d of state.devices)if(Object.keys(d.active).length)engine.reboot(d);});
 const publicFiles={'/':'index.html','/local':'index.html','/app.js':'app.js','/style.css':'style.css'};
 const handler=async(req,res)=>{
  const send=(status,body)=>{res.writeHead(status,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'});res.end(JSON.stringify(body));};
  try{
   const url=new URL(req.url,'http://localhost');const p=url.pathname;
   res.setHeader('X-Content-Type-Options','nosniff');res.setHeader('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'self'; form-action 'self'");
   if(req.method==='GET'&&publicFiles[p]){res.setHeader('Content-Type',p.endsWith('.js')?'text/javascript; charset=utf-8':p.endsWith('.css')?'text/css; charset=utf-8':'text/html; charset=utf-8');res.end(readFileSync(resolve(root,'public',publicFiles[p])));return;}
   if(req.method==='GET'&&p==='/health'){send(200,{ok:true,version:'0.2.0',simulated:true});return;}
   if(req.method!=='GET'){
    if(req.headers.origin){const origin=new URL(req.headers.origin);if(origin.host!==req.headers.host) {send(403,{error:'Origem não autorizada'});return;}}
    if(req.headers['content-type']?.split(';')[0]!=='application/json'){send(415,{error:'Envie application/json'});return;}
   }
   let body={};if(req.method!=='GET'){let text='',bytes=0;for await(const chunk of req){bytes+=chunk.length;if(bytes>131072){send(413,{error:'Corpo excede 128 KB'});return;}text+=chunk;}try{body=JSON.parse(text||'{}');}catch{send(400,{error:'JSON inválido'});return;}}
   if(req.method==='POST'&&p==='/api/login'){
    const ip=req.socket.remoteAddress,entry=attempts.get(ip)||{count:0,until:Date.now()+60000};if(entry.until<Date.now()){entry.count=0;entry.until=Date.now()+60000;}attempts.set(ip,entry);if(entry.count>=10){send(429,{error:'Muitas tentativas; aguarde um minuto'});return;}
    const user=db.prepare('SELECT * FROM users WHERE username=?').get(typeof body.username==='string'?body.username:'');const salt=user?.salt||'invalid';const h=scryptSync(typeof body.password==='string'?body.password:'',salt,32);if(!user||!timingSafeEqual(h,Buffer.from(user.hash,'hex'))){entry.count++;send(401,{error:'Usuário ou senha inválidos'});return;}
    const token=randomBytes(32).toString('hex');sessions.set(token,{username:user.username,role:user.role,until:Date.now()+8*3600000});res.setHeader('Set-Cookie',`session=${token}; HttpOnly; SameSite=Strict; Path=/; Max-Age=28800`);send(200,{username:user.username,role:user.role});return;
   }
   const token=req.headers.cookie?.split(';').map(s=>s.trim()).find(s=>s.startsWith('session='))?.slice(8);const session=sessions.get(token);if(!session||session.until<Date.now()){send(401,{error:'Faça login'});return;}
   if(req.method==='POST'&&p==='/api/logout'){sessions.delete(token);res.setHeader('Set-Cookie','session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0');send(200,{ok:true});return;}
   if(req.method==='GET'&&p==='/api/state'){send(200,{...state,user:{username:session.username,role:session.role},simulated:true});return;}
   if(req.method==='GET'&&['/api/events','/api/history'].includes(p)){const table=p==='/api/events'?'events':'samples',guid=url.searchParams.get('guid');const rows=guid?db.prepare(`SELECT body FROM ${table} WHERE device=? ORDER BY id DESC LIMIT 500`).all(guid):db.prepare(`SELECT body FROM ${table} ORDER BY id DESC LIMIT 500`).all();send(200,rows.map(r=>JSON.parse(r.body)));return;}
   if(req.method!=='POST'){send(404,{error:'Rota inexistente'});return;}
   const adminOnly=['/api/config','/api/device','/api/simulation','/api/sensor','/api/network','/api/reboot','/api/integration'];
   if(adminOnly.includes(p)&&session.role!=='admin'){const d=state.devices[0];if(d)mutate(()=>engine.event(d,'rejected','permission_denied',{actor:session.username,route:p}));send(403,{error:'Somente administrador'});return;}
   const device=()=>{const d=state.devices.find(d=>d.guid===body.guid);if(!d)throw new ValidationError('Dispositivo não encontrado');return d;};
   let result;
   if(p==='/api/device')result=mutate(()=>engine.add(body.config));
   else if(p==='/api/config')result=mutate(()=>{engine.configure(device(),body.config);return {ok:true};});
   else if(p==='/api/command')result=mutate(()=>engine.command(device(),{...body,actor:session.username}));
   else if(p==='/api/simulation')result=mutate(()=>{if(body.seconds!=null)engine.advance(body.seconds);if(body.paused!=null){if(typeof body.paused!=='boolean')throw new ValidationError('Pausa inválida');state.paused=body.paused;}if(body.speed!=null){if(![1,10,60].includes(body.speed))throw new ValidationError('Velocidade inválida');state.speed=body.speed;}return {ok:true,now:state.now};});
   else if(p==='/api/sensor')result=mutate(()=>{const d=device(),s=d.sensors[body.component_id];if(!s||!d.config.components.some(c=>c.id===body.component_id&&c.kind!=='irrigation_actuator'))throw new ValidationError('Sensor não encontrado');if(!['valid','invalid','stale','missing'].includes(body.quality)||typeof body.value!=='number'||!Number.isFinite(body.value))throw new ValidationError('Leitura inválida');const c=d.config.components.find(c=>c.id===body.component_id);if(c.kind==='soil_moisture'&&(body.value<0||body.value>100))throw new ValidationError('Umidade entre 0 e 100');Object.assign(s,{value:body.value,quality:body.quality,updated_at:state.now});engine.event(d,'sensor_changed','simulation_input',{component_id:body.component_id,actor:session.username});return {ok:true};});
   else if(p==='/api/network')result=mutate(()=>{const d=device();if(typeof body.online!=='boolean')throw new ValidationError('Estado inválido');d.online=body.online;engine.event(d,body.online?'online':'offline','simulation_network');return {ok:true};});
   else if(p==='/api/reboot')result=mutate(()=>{engine.reboot(device());return {ok:true};});
   else if(p==='/api/integration')result=mutate(()=>{const i=body.integration;if(!i||typeof i.url!=='string'||(i.url&&!i.url.startsWith('https://'))||!['POST','PUT'].includes(i.method)||!i.body||typeof i.body!=='object'||!Array.isArray(i.accepted_status)||i.accepted_status.some(n=>!Number.isInteger(n)||n<100||n>599)||typeof i.ack_path!=='string'||!/^[a-zA-Z0-9_.]+$/.test(i.ack_path))throw new ValidationError('Perfil de integração inválido');state.integration=i;return {ok:true};});
   else if(p==='/api/integration/preview'){const d=device();let template=JSON.stringify(state.integration.body);const replacements={'device.guid':d.guid,'message.id':'preview-message','message.recorded_at':new Date(state.now*1000).toISOString()};template=template.replace(/\{\{([a-zA-Z0-9_.]+)\}\}/g,(_,key)=>{if(!(key in replacements))throw new ValidationError('Variável desconhecida: '+key);return replacements[key];});result={simulated:true,request:{url:state.integration.url,method:state.integration.method,body:JSON.parse(template)},note:'Prévia apenas. Nenhuma requisição externa foi enviada.'};}
   else {send(404,{error:'Rota inexistente'});return;}
   send(200,result);
  }catch(e){if(e instanceof ValidationError)send(400,{error:e.message});else{console.error(e);send(500,{error:'Erro interno; nenhuma confirmação de gravação foi emitida'});}}
 };
 const server=http.createServer(handler);const timer=setInterval(()=>{if(!state.paused)try{mutate(()=>engine.advance(state.speed));}catch(e){state.paused=true;console.error(e);}},1000);timer.unref();
 return {server,engine,db,close:()=>{clearInterval(timer);server.close();db.close();}};
}
if(process.argv[1]===fileURLToPath(import.meta.url)){const app=createApp();const port=Number(process.env.PORT||8080),host=process.env.HOST||'127.0.0.1';app.server.listen(port,host,()=>console.log(`IoT Smart Irrigation: http://${host}:${port} | portal local: /local | SIMULAÇÃO | Node 24+`));process.on('SIGINT',()=>{app.close();process.exit(0);});}
