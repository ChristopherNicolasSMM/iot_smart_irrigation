import {historyContext} from './history-context.js';
import { createHash } from 'node:crypto';
import { ValidationError } from './engine.js';
const fail=message=>{throw new ValidationError(message);};
export function initIngestion(db){db.exec(`CREATE TABLE IF NOT EXISTS device_credentials(device TEXT PRIMARY KEY,hash TEXT NOT NULL); CREATE TABLE IF NOT EXISTS inbox(device TEXT,message_id TEXT,hash TEXT NOT NULL,received_at TEXT NOT NULL,PRIMARY KEY(device,message_id)); CREATE TABLE IF NOT EXISTS incoming_events(device TEXT,event_id TEXT,PRIMARY KEY(device,event_id));`);}
export const tokenHash=token=>createHash('sha256').update(token).digest('hex');
export function ingest(db,state,d,payload){
 const validID=x=>typeof x==='string'&&x.length>=1&&x.length<=100;
 const validTime=x=>typeof x==='string'&&/^\d{4}-\d{2}-\d{2}T/.test(x)&&Number.isFinite(Date.parse(x));
 if(!payload||payload.schema_version!==1||payload.device_guid!==d.guid||!validID(payload.message_id)||!validTime(payload.recorded_at)||!Array.isArray(payload.readings)||payload.readings.length>8||!Array.isArray(payload.events)||payload.events.length>50)fail('Envelope de telemetria inválido');
 if(payload.configuration_revision!==undefined&&(!Number.isSafeInteger(payload.configuration_revision)||payload.configuration_revision<1))fail('Revisão de telemetria inválida');
 const revision=payload.configuration_revision,version=revision?db.prepare("SELECT body FROM configuration_versions WHERE device=? AND revision=? AND status='applied'").get(d.guid,revision):null,context=version?historyContext(JSON.parse(version.body)):{};
 const readings=new Set();for(const r of payload.readings){const c=d.config.components.find(c=>c.id===r.component_id&&c.kind!=='irrigation_actuator');if(!c||readings.has(r.component_id)||!['valid','invalid','missing','stale'].includes(r.quality)||!validTime(r.recorded_at)||r.quality==='valid'&&(!Number.isFinite(r.value)||c.kind==='soil_moisture'&&(r.value<0||r.value>100))||r.quality!=='valid'&&r.value!==null)fail('Leitura inválida');readings.add(r.component_id);}
 for(const e of payload.events)if(!validID(e.event_id)||!validTime(e.recorded_at)||typeof e.kind!=='string'||typeof e.reason!=='string'||e.device_guid!==d.guid)fail('Evento inválido');
 const canonical=x=>Array.isArray(x)?x.map(canonical):x&&typeof x==='object'?Object.fromEntries(Object.keys(x).sort().map(k=>[k,canonical(x[k])])):x;
 const hash=createHash('sha256').update(JSON.stringify(canonical(payload))).digest('hex'),previous=db.prepare('SELECT * FROM inbox WHERE device=? AND message_id=?').get(d.guid,payload.message_id);
 if(previous){if(previous.hash!==hash)fail('message_id reutilizado com conteúdo diferente');return {accepted:true,message_id:payload.message_id,received_at:previous.received_at,duplicate:true};}
 const received_at=new Date().toISOString();db.prepare('INSERT INTO inbox VALUES(?,?,?,?)').run(d.guid,payload.message_id,hash,received_at);
 if(payload.readings.length&&!db.prepare("SELECT id FROM samples WHERE device=? AND json_extract(body,'$.message_id')=? LIMIT 1").get(d.guid,payload.message_id)){const sensors=Object.fromEntries(payload.readings.map(r=>[r.component_id,{value:r.value,quality:r.quality,updated_at:Date.parse(r.recorded_at)/1000}]));db.prepare('INSERT INTO samples(device,body) VALUES (?,?)').run(d.guid,JSON.stringify({sample:true,configuration_revision:revision??null,...context,message_id:payload.message_id,device_guid:d.guid,recorded_at:payload.recorded_at,received_at,sensors,outputs:payload.outputs||[],simulated:payload.simulated===true,transport:'http_ingest',online:true}));}
 for(const e of payload.events){const result=db.prepare('INSERT OR IGNORE INTO incoming_events VALUES(?,?)').run(d.guid,e.event_id);if(result.changes&&!db.prepare("SELECT id FROM events WHERE device=? AND json_extract(body,'$.event_id')=? LIMIT 1").get(d.guid,e.event_id))db.prepare('INSERT INTO events(device,body) VALUES (?,?)').run(d.guid,JSON.stringify({...e,received_at,transport:'http_ingest',simulated:payload.simulated===true}));}
 d.last_received??={};for(const r of payload.readings){const old=d.last_received[r.component_id];if(!old||Date.parse(r.recorded_at)>Date.parse(old.recorded_at))d.last_received[r.component_id]={...r,received_at};}
 return {accepted:true,message_id:payload.message_id,received_at,duplicate:false};
}
