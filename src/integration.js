import http from 'node:http';
import https from 'node:https';
import { lookup } from 'node:dns/promises';
import { isIP } from 'node:net';
import { randomUUID } from 'node:crypto';
import { ValidationError } from './engine.js';
const fail = message => { throw new ValidationError(message); };
const PATH = /^[a-zA-Z0-9_]+(?:\.[a-zA-Z0-9_]+)*$/;
const variable = /^\{\{([a-zA-Z0-9_.-]+)\}\}$/;
export function normalizeProfile(input) {
 const p={enabled:false,timeout_s:5,max_attempts:3,queue_limit:300,require_ack:true,message_id_path:'message_id',...input};
 if(typeof p.enabled!=='boolean'||typeof p.url!=='string'||!['POST','PUT'].includes(p.method)||!p.body||typeof p.body!=='object'||Array.isArray(p.body))fail('Perfil de integração inválido');
 if(p.url){let u;try{u=new URL(p.url);}catch{fail('URL inválida');}if(!['https:','http:'].includes(u.protocol)||u.username||u.password||u.hash)fail('URL deve usar HTTP(S), sem usuário/senha ou fragmento');}
 if(p.enabled&&!p.url)fail('Defina a URL antes de habilitar envio');
 for(const [k,min,max] of [['timeout_s',1,30],['max_attempts',1,5],['queue_limit',1,1000]])if(!Number.isInteger(p[k])||p[k]<min||p[k]>max)fail('Limite inválido: '+k);
 if(!Array.isArray(p.accepted_status)||!p.accepted_status.length||p.accepted_status.some(n=>!Number.isInteger(n)||n<200||n>299)||typeof p.require_ack!=='boolean'||!PATH.test(p.ack_path)||!PATH.test(p.message_id_path))fail('Confirmação/códigos inválidos');
 if(JSON.stringify(p.body).length>32768)fail('Modelo excede 32 KB');
 return {enabled:p.enabled,url:p.url,method:p.method,body:p.body,accepted_status:p.accepted_status,ack_path:p.ack_path,message_id_path:p.message_id_path,require_ack:p.require_ack,timeout_s:p.timeout_s,max_attempts:p.max_attempts,queue_limit:p.queue_limit};
}
export function validateHeaders(headers) {
 if(!headers||typeof headers!=='object'||Array.isArray(headers)||Object.keys(headers).length>12)fail('Cabeçalhos inválidos');
 const forbidden=new Set(['host','content-length','connection','transfer-encoding','cookie','set-cookie']);const out={};
 for(const [name,value] of Object.entries(headers)){if(!/^[A-Za-z0-9-]+$/.test(name)||forbidden.has(name.toLowerCase())||typeof value!=='string'||/[\r\n]/.test(value)||value.length>4096)fail('Cabeçalho inválido');out[name]=value;}
 return out;
}
export function renderTemplate(template,context){
 function get(path){let v=context;for(const key of path.split('.')){if(['__proto__','constructor','prototype'].includes(key)||v==null||!Object.hasOwn(v,key))fail('Variável desconhecida: '+path);v=v[key];}return v;}
 function render(value){if(typeof value==='string'){const m=value.match(variable);if(m)return structuredClone(get(m[1]));return value.replace(/\{\{([a-zA-Z0-9_.-]+)\}\}/g,(_,p)=>{const v=get(p);if(v!==null&&typeof v==='object')fail('Objeto precisa ocupar todo o campo: '+p);return String(v??'');});}if(Array.isArray(value))return value.map(render);if(value&&typeof value==='object'){const result={};for(const [k,v] of Object.entries(value)){if(['__proto__','constructor','prototype'].includes(k))fail('Chave não permitida');result[k]=render(v);}return result;}return value;}
 return render(template);
}
function privateAddress(address){
 const v=isIP(address);if(v===4){const a=address.split('.').map(Number);return a[0]===0||a[0]===10||a[0]===127||a[0]>=224||a[0]===169&&a[1]===254||a[0]===172&&a[1]>=16&&a[1]<=31||a[0]===192&&(a[1]===168||a[1]===0)||a[0]===100&&a[1]>=64&&a[1]<=127||a[0]===198&&(a[1]===18||a[1]===19);}
 // Only globally routable IPv6 2000::/3 is accepted without explicit local opt-in.
 return v!==6||!/^([23])[0-9a-f]{0,3}:/i.test(address);
}
export async function requestJSON(profile,payload,headers,{allowLocal=false}={}){
 const url=new URL(profile.url), hostname=url.hostname.replace(/^\[|\]$/g,'');
 if(url.protocol!=='https:'&&!allowLocal)throw Error('https_required');
 let dnsTimer;const addresses=isIP(hostname)?[{address:hostname,family:isIP(hostname)}]:await Promise.race([lookup(hostname,{all:true}),new Promise((_,reject)=>{dnsTimer=setTimeout(()=>reject(Error('timeout')),profile.timeout_s*1000);})]).finally(()=>clearTimeout(dnsTimer));
 if(!addresses.length||!allowLocal&&addresses.some(a=>privateAddress(a.address)))throw Error('private_destination_blocked');
 const pinned=addresses[0],data=Buffer.from(JSON.stringify(payload));if(data.length>65536)throw Error('payload_too_large');
 return await new Promise((resolve,reject)=>{
  let settled=false;const finish=(fn,value)=>{if(!settled){settled=true;clearTimeout(timer);fn(value);}};
  const req=(url.protocol==='https:'?https:http).request(url,{method:profile.method,headers:{...headers,'Content-Type':'application/json','Content-Length':data.length},lookup:(_host,opts,cb)=>opts?.all?cb(null,[pinned]):cb(null,pinned.address,pinned.family)},res=>{
   let size=0,parts=[];res.on('data',chunk=>{size+=chunk.length;if(size>65536){req.destroy();finish(reject,Error('response_too_large'));}else parts.push(chunk);});res.on('error',()=>finish(reject,Error('response_error')));res.on('end',()=>{let body=null;try{body=JSON.parse(Buffer.concat(parts).toString());}catch{}finish(resolve,{status:res.statusCode,body});});
  });
  const timer=setTimeout(()=>{req.destroy();finish(reject,Error('timeout'));},profile.timeout_s*1000);req.on('error',e=>finish(reject,Error(['timeout','response_too_large'].includes(e.message)?e.message:'network_error')));req.end(data);
 });
}
function valueAt(object,path){return path.split('.').reduce((v,k)=>v&&Object.hasOwn(v,k)?v[k]:undefined,object);}
export class Outbox {
 constructor(db,{transport=requestJSON,allowLocal=false,online=()=>true,onOutcome=()=>{},now=()=>Date.now()}={}){
  Object.assign(this,{db,transport,allowLocal,online,onOutcome,now,busy:false,closed:false});
  db.exec(`CREATE TABLE IF NOT EXISTS outbox(id TEXT PRIMARY KEY,device TEXT,body TEXT NOT NULL,profile TEXT NOT NULL,headers TEXT NOT NULL,status TEXT NOT NULL,attempts INTEGER NOT NULL DEFAULT 0,next_at INTEGER NOT NULL,error TEXT,created_at INTEGER NOT NULL,delivered_at INTEGER); CREATE INDEX IF NOT EXISTS outbox_ready ON outbox(status,next_at); CREATE TABLE IF NOT EXISTS integration_headers(device TEXT PRIMARY KEY,body TEXT NOT NULL);`);
  db.prepare("UPDATE outbox SET status='pending' WHERE status='sending'").run();
 }
 getHeaders(device){return JSON.parse(this.db.prepare('SELECT body FROM integration_headers WHERE device=?').get(device)?.body||'{}');}
 setHeaders(headers,device){this.db.prepare('INSERT INTO integration_headers VALUES(?,?) ON CONFLICT(device) DO UPDATE SET body=excluded.body').run(device,JSON.stringify(validateHeaders(headers)));}
 enqueue(device,payload,profile){
  const count=this.db.prepare("SELECT COUNT(*) AS n FROM outbox WHERE device=? AND status IN ('pending','sending')").get(device).n;
  if(count>=profile.queue_limit){this.onOutcome({device,message_id:payload.message_id,status:'dropped',attempts:0,reason:'queue_full'});return false;}
  this.db.prepare('INSERT INTO outbox(id,device,body,profile,headers,status,next_at,created_at) VALUES(?,?,?,?,?,?,?,?)').run(payload.message_id,device,JSON.stringify(payload),JSON.stringify(profile),JSON.stringify(this.getHeaders(device)),'pending',this.now(),this.now());return true;
 }
 list(guid){const rows=guid?this.db.prepare('SELECT id,device,status,attempts,next_at,error,created_at,delivered_at FROM outbox WHERE device=? ORDER BY created_at DESC LIMIT 100').all(guid):this.db.prepare('SELECT id,device,status,attempts,next_at,error,created_at,delivered_at FROM outbox ORDER BY created_at DESC LIMIT 100').all();return rows;}
 async flush(){if(this.busy||this.closed)return;this.busy=true;try{
  const rows=this.db.prepare("SELECT * FROM outbox WHERE status='pending' AND next_at<=? ORDER BY created_at ").all(this.now());
  const row=rows.find(r=>this.online(r.device));if(!row)return;
  const p=JSON.parse(row.profile),payload=JSON.parse(row.body),headers=JSON.parse(row.headers);let reason,status='pending';const attempts=row.attempts+1;
  if(row.attempts>=p.max_attempts){this.db.prepare("UPDATE outbox SET status='dead',error='attempt_budget_exhausted' WHERE id=?").run(row.id);this.onOutcome({device:row.device,message_id:row.id,status:'dead',attempts:row.attempts,reason:'attempt_budget_exhausted'});return;}
  this.db.prepare("UPDATE outbox SET status='sending',attempts=? WHERE id=?").run(attempts,row.id);
  try{const response=await this.transport(p,payload,headers,{allowLocal:this.allowLocal});
   if(!p.accepted_status.includes(response.status))reason='http_'+response.status;
   else if(p.require_ack&&valueAt(response.body,p.ack_path)!==true)reason='ack_invalid';
   else if(p.require_ack&&valueAt(response.body,p.message_id_path)!==row.id)reason='message_id_mismatch';
   else status='delivered';
  }catch(e){reason=['timeout','response_too_large','payload_too_large','https_required','private_destination_blocked'].includes(e.message)?e.message:'network_error';}
  if(this.closed)return;
  if(reason&&(attempts>=p.max_attempts||['http_401','http_403','https_required','private_destination_blocked','payload_too_large'].includes(reason)))status='dead';
  this.db.prepare('UPDATE outbox SET status=?,next_at=?,error=?,delivered_at=? WHERE id=?').run(status,this.now()+Math.min(60000,1000*2**attempts),reason||null,status==='delivered'?this.now():null,row.id);
  this.onOutcome({device:row.device,message_id:row.id,status,attempts,reason:reason||(p.require_ack?'application_ack':'http_status_only')});
 }finally{this.busy=false;}}
 close(){this.closed=true;}
}
export function envelope(d,event,now){const time=event.recorded_at||new Date(now*1000).toISOString();return {schema_version:1,message_id:event.message_id||randomUUID(),device_guid:d.guid,boot_id:`sim-boot-${d.boot}`,recorded_at:time,configuration_revision:d.revision,simulated:true,readings:event.sample?d.config.components.filter(c=>c.kind!=='irrigation_actuator').map(c=>({component_id:c.id,metric:c.kind,value:c.enabled&&d.sensors[c.id]?.quality==='valid'?d.sensors[c.id].value:null,quality:c.enabled?(d.sensors[c.id]?.quality||'missing'):'missing',unit:c.kind==='soil_moisture'?'%relative':'degC',recorded_at:time})):[],outputs:event.sample?d.config.components.filter(c=>c.kind==='irrigation_actuator').map(c=>({component_id:c.id,commanded_state:d.config.zones.some(z=>z.actuator_id===c.id&&d.active[z.id])?'on':'off',feedback_kind:'electrical_output'})):[],events:event.sample?[]:[event]};}
export function mappedPayload(profile,d,message){const sensors={};for(const c of d.config.components.filter(c=>c.kind!=='irrigation_actuator'))sensors[c.id]={value:c.enabled&&d.sensors[c.id]?.quality==='valid'?d.sensors[c.id].value:null,quality:c.enabled?d.sensors[c.id]?.quality:'missing'};const payload=renderTemplate(profile.body,{device:{guid:d.guid,name:d.config.name,area:d.config.area},message:{...message,id:message.message_id},sensors});if(payload.message_id!==message.message_id)fail('Modelo deve preservar message_id no nível principal');return payload;}

export function defaultProfile(){return normalizeProfile({url:'',method:'POST',body:{schema_version:1,message_id:'{{message.message_id}}',device_guid:'{{device.guid}}',recorded_at:'{{message.recorded_at}}',readings:'{{message.readings}}',events:'{{message.events}}',outputs:'{{message.outputs}}',simulated:true},accepted_status:[200,201],ack_path:'accepted'});}
