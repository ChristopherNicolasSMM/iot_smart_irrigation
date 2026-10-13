import { localParts,resolveWindow } from './time.js';
import { scheduleSlots } from '../public/schedules.js';
import { randomUUID } from 'node:crypto';
const kinds = ['soil_moisture','soil_temperature','irrigation_actuator'];
export class ValidationError extends Error {}
const fail = message => { throw new ValidationError(message); };
const finite = (n,min,max,label) => { if(typeof n !== 'number'||!Number.isFinite(n)||n<min||n>max) fail(label); };
const id = s => typeof s === 'string' && !['__proto__','constructor','prototype'].includes(s) && /^[a-zA-Z0-9_-]{1,64}$/.test(s);
const clock = s => typeof s==='string' && /^([01]\d|2[0-3]):[0-5]\d$/.test(s);
export function validateConfig(c) {
 if(!c || typeof c!=='object'|| !Array.isArray(c.components)||!Array.isArray(c.zones)||!Array.isArray(c.schedules)) fail('Configuração incompleta');
 if(typeof c.name!=='string'||!c.name.trim()||c.name.length>100||typeof c.area!=='string'||c.area.length>100) fail('Nome/área inválidos');
 if(typeof c.timezone!=='string'||!c.timezone||c.timezone.length>64)fail('Fuso inválido');
 try { new Intl.DateTimeFormat('pt-BR',{timeZone:c.timezone}); } catch { fail('Fuso inválido'); }
 finite(c.max_simultaneous,1,4,'Simultaneidade entre 1 e 4'); if(!Number.isInteger(c.max_simultaneous)) fail('Simultaneidade inteira');
 const ids=new Set();
 for(const x of c.components){
  if(!id(x.id)||ids.has(x.id)||!kinds.includes(x.kind)||typeof x.name!=='string'||!x.name.trim()||x.name.length>100||typeof x.enabled!=='boolean')fail('Componente inválido ou identificador duplicado');
  ids.add(x.id);
 }
 for(const k of kinds)if(c.components.filter(x=>x.kind===k).length>4)fail('Máximo de quatro componentes por categoria');
 if(c.zones.length>4)fail('Máximo de quatro conjuntos por dispositivo');
 const zs=new Set(), valves=new Set();
 for(const z of c.zones){
  if(!id(z.id)||zs.has(z.id)||typeof z.name!=='string'||!z.name.trim()||z.name.length>100||!['static','dynamic','hybrid'].includes(z.mode)) fail('Setor inválido');zs.add(z.id);
  if(!c.components.find(x=>x.id===z.actuator_id&&x.kind==='irrigation_actuator')||valves.has(z.actuator_id))fail('Válvula ausente ou vinculada a dois setores');valves.add(z.actuator_id);
  if(z.sensor_id&&!c.components.find(x=>x.id===z.sensor_id&&x.kind==='soil_moisture'))fail('Sensor de umidade do conjunto inválido');
  if(z.temperature_sensor_id&&!c.components.find(x=>x.id===z.temperature_sensor_id&&x.kind==='soil_temperature'))fail('Sensor de temperatura do conjunto inválido');
  finite(z.max_run_s,1,86400,'Duração máxima entre 1 e 86400 s');finite(z.min_pause_s,0,86400,'Pausa inválida');
  if(z.mode!=='static'){
   if(!c.components.find(x=>x.id===z.sensor_id&&x.kind==='soil_moisture'))fail('Modo por umidade exige sensor associado');
   finite(z.start_below,0,100,'Limite inicial inválido');finite(z.target,0,100,'Alvo inválido');if(z.start_below>=z.target)fail('Limite inicial deve ser menor que alvo');
  }
 }
 const validateTime=s=>{
  if(!s||typeof s!=='object'||Array.isArray(s))fail('Horário inválido');
  if(!clock(s.start_time))fail('Hora inicial inválida');
  if(s.termination==='duration'){finite(s.duration_s,1,86400,'Duração de agenda inválida');if(s.end_time)fail('Use duração ou hora final');}
  else if(s.termination==='end_time'){if(!clock(s.end_time)||s.duration_s!=null||s.end_time===s.start_time||(!s.crosses_midnight&&s.end_time<s.start_time)||(s.crosses_midnight&&s.end_time>s.start_time))fail('Hora final/virada de dia inválida');}else fail('Término inválido');
 };
 if(c.schedules.length>32)fail('Máximo de 32 agendas por dispositivo');
 const ss=new Set();for(const s of c.schedules){
  if(!id(s.id)||ss.has(s.id)||!zs.has(s.zone_id)||typeof s.enabled!=='boolean')fail('Agenda inválida');ss.add(s.id);
  if(s.calendar==='weekly'){
   if(s.day_times!==undefined){
    if(!Array.isArray(s.day_times)||!s.day_times.length||s.day_times.length>7)fail('Marque ao menos um dia da semana');const days=new Set();
    for(const day of s.day_times){if(!day||typeof day!=='object'||!Number.isInteger(day.weekday)||day.weekday<1||day.weekday>7||days.has(day.weekday)||!Array.isArray(day.times)||!day.times.length||day.times.length>8)fail('Dia/horários inválidos ou repetidos');days.add(day.weekday);const slots=new Set();for(const time of day.times){if(!time||typeof time!=='object'||!id(time.id)||slots.has(time.id))fail('Identificador de horário inválido ou repetido');slots.add(time.id);validateTime(time);}}
   }else{if(!Array.isArray(s.weekdays)||!s.weekdays.length||s.weekdays.some(d=>!Number.isInteger(d)||d<1||d>7))fail('Selecione dias da semana');validateTime(s);}
  }else if(s.calendar==='date'){if(!/^\d{4}-\d{2}-\d{2}$/.test(s.date)||Number.isNaN(Date.parse(s.date))||new Date(s.date).toISOString().slice(0,10)!==s.date)fail('Data inválida');validateTime(s);}else fail('Calendário inválido');
 }

 return structuredClone(c);
}
export function demoConfig(mode='hybrid') {return {name:'ESP32 · Horta',area:'Horta principal',timezone:'America/Sao_Paulo',max_simultaneous:1,components:[{id:'soil_1',name:'Umidade canteiro',kind:'soil_moisture',enabled:true},{id:'temp_1',name:'Temperatura solo',kind:'soil_temperature',enabled:true},{id:'valve_1',name:'Solenoide canteiro',kind:'irrigation_actuator',enabled:true}],zones:[{id:'zone_1',name:'Canteiro 1',actuator_id:'valve_1',sensor_id:'soil_1',temperature_sensor_id:'temp_1',mode,start_below:30,target:45,max_run_s:600,min_pause_s:1800}],schedules:[{id:'morning',zone_id:'zone_1',enabled:true,calendar:'weekly',weekdays:[1,2,3,4,5,6,7],start_time:'06:00',termination:'duration',duration_s:1800,crosses_midnight:false}]};}
export class Engine {
 constructor(state,emit=()=>{}){this.s=state;this.emit=emit;this.ids=0;}
 static fresh(){return {now:Date.parse('2026-10-12T08:59:50Z')/1000,seed:42,event_seq:0,uptime:0,paused:true,speed:1,devices:[]};}
 event(d,kind,reason,extra={}){const group=d.config.zones.find(z=>z.id===extra.zone_id);const e={event_id:`sim-${this.s.seed}-${++this.s.event_seq}`,device_guid:d.guid,device_name:d.config.name,property_id:d.property_id,area_id:d.config.area_id||d.area_id,area:group?.area_name||d.config.area,revision:d.revision,recorded_at:new Date(this.s.now*1000).toISOString(),kind,reason,time_quality:d.clock_valid?'synchronized':'invalid',simulated:true,...(group?{group_id:group.id,group_name:group.name,actuator_id:group.actuator_id,sensor_id:group.sensor_id||null,temperature_sensor_id:group.temperature_sensor_id||null,area_id:group.area_id||d.area_id}:{}),...extra};this.emit(e);return e;}
 add(config,guid=randomUUID(),metadata={}){validateConfig(config);const d={guid,property_id:metadata.property_id,area_id:config.area_id,config:structuredClone(config),revision:1,desired_revision:1,desired_config:structuredClone(config),config_status:'applied',config_error:null,online:true,clock_valid:true,boot:1,powered:true,sensors:{},active:{},last_stop:{},last_stop_uptime:{},occurrences:{},decisions:{},commands:{},retired_ids:[],next_sample:this.s.now+900,next_sample_uptime:(this.s.uptime||0)+900};for(const x of config.components)if(x.kind!=='irrigation_actuator')d.sensors[x.id]={value:x.kind==='soil_moisture'?25:24,quality:'valid',updated_at:this.s.now,updated_uptime:this.s.uptime||0};this.s.devices.push(d);this.event(d,'configured','device_created');return d;}
 configure(d,c,revision=d.revision+1){validateConfig(c);if(Object.keys(d.active).length)fail('Pare as saídas antes de alterar a configuração');if(c.components.some(x=>d.config.components.some(old=>old.id===x.id&&old.kind!==x.kind)))fail('Categoria de identificador existente não pode mudar');if(c.components.some(x=>d.retired_ids.includes(x.id)))fail('Identificador arquivado não pode ser reutilizado');for(const x of d.config.components)if(!c.components.some(y=>y.id===x.id))d.retired_ids.push(x.id);for(const x of c.components)if(x.kind!=='irrigation_actuator'&&!d.sensors[x.id])d.sensors[x.id]={value:x.kind==='soil_moisture'?25:24,quality:'valid',updated_at:this.s.now,updated_uptime:this.s.uptime||0};d.config=structuredClone(c);d.area_id=c.area_id||d.area_id;d.revision=revision;d.decisions={};this.event(d,'configured','configuration_applied');}
 publish(d,c){validateConfig(c);d.desired_revision=(d.desired_revision??d.revision)+1;d.desired_config=structuredClone(c);d.config_status='pending';d.config_error=null;this.event(d,'configuration_published','awaiting_device',{desired_revision:d.desired_revision});return {desired_revision:d.desired_revision,applied_revision:d.revision,status:'pending'};}
 applyPending(d){if(d.config_status!=='pending'||!d.online||Object.keys(d.active).length)return;try{this.configure(d,d.desired_config,d.desired_revision);d.config_status='applied';d.config_error=null;}catch(e){d.config_status='rejected';d.config_error=e.message;this.event(d,'configuration_rejected','invalid_configuration');}}
 decide(d,z,key,reason){if(d.decisions[key]!==reason){d.decisions[key]=reason;this.event(d,'omitted',reason,{zone_id:z.id,occurrence_id:key});}}
 valid(d,z){const x=d.sensors[z.sensor_id],component=d.config.components.find(x=>x.id===z.sensor_id);return component?.enabled&&x?.quality==='valid'&&Number.isFinite(x.value)&&(x.updated_uptime!==undefined?(this.s.uptime||0)-x.updated_uptime:this.s.now-x.updated_at)<=60;}
 start(d,z,source,duration,extra={}){const valve=d.config.components.find(x=>x.id===z.actuator_id);let reason=!valve?.enabled?'actuator_disabled':d.active[z.id]?'conflict':Object.keys(d.active).length>=d.config.max_simultaneous?'simultaneous_limit':null;if(reason){this.event(d,'rejected',reason,{zone_id:z.id,source,...extra});return false;}d.active[z.id]={source,started_at:this.s.now,deadline:this.s.now+Math.min(duration,z.max_run_s),deadline_uptime:(this.s.uptime||0)+Math.min(duration,z.max_run_s),...extra};this.event(d,'started','output_on',{zone_id:z.id,actuator_id:z.actuator_id,source,...extra});return true;}
 stop(d,z,reason='requested_stop'){const a=d.active[z.id];if(a){delete d.active[z.id];d.last_stop[z.id]=this.s.now;d.last_stop_uptime??={};d.last_stop_uptime[z.id]=this.s.uptime||0;if(a.command_id)d.commands[a.command_id].status=reason==='duration_elapsed'?'completed':'interrupted';this.event(d,reason==='duration_elapsed'||reason==='target_reached'?'completed':'interrupted',reason,{zone_id:z.id,actuator_id:z.actuator_id,...a});}return a;}
 command(d,{command_id,zone_id,action='start',source='test',duration_s=5,expires_at,origin='web',actor='admin'}){
  if(typeof command_id!=='string'||['__proto__','constructor','prototype'].includes(command_id)||command_id.length>100||!command_id)fail('ID de comando inválido');if(d.commands[command_id])return d.commands[command_id];
  const z=d.config.zones.find(z=>z.id===zone_id);if(!z)fail('Setor não encontrado');if(!['start','stop'].includes(action)||!['test','manual'].includes(source)||!['web','local'].includes(origin))fail('Comando inválido');
  const result={command_id,status:'requested'};d.commands[command_id]=result;this.event(d,'requested',action,{zone_id,command_id,source,origin,actor});
  const reject=reason=>{result.status=reason==='expired'?'expired':'rejected';result.reason=reason;this.event(d,result.status,reason,{zone_id,command_id,origin,actor});return result;};
  if(d.powered===false)return reject('device_powered_off');if(origin==='web'&&!d.online)return reject('device_offline');if(action==='stop'){this.stop(d,z);result.status='completed';this.event(d,'completed','stop_ack',{zone_id,command_id,origin,actor});return result;}
  if(!d.clock_valid)return reject('clock_invalid');if(expires_at!=null&&(!Number.isFinite(expires_at)||expires_at<=this.s.now))return reject('expired');
  if(!Number.isFinite(duration_s)||duration_s<1||duration_s>Math.min(z.max_run_s,source==='test'?30:86400))return reject('duration_invalid');
  this.event(d,'received','command_received',{command_id,zone_id,origin,actor});result.status=this.start(d,z,source,duration_s,{command_id,origin,actor})?'started':'rejected';return result;
 }
 windows(d,at=this.s.now){const p=localParts(at,d.config.timezone),today=Date.parse(p.date)/1000,out=[];for(const s of d.config.schedules.filter(s=>s.enabled).flatMap(scheduleSlots))for(const day of [today-86400,today]){const date=new Date(day*1000).toISOString().slice(0,10),weekday=new Date(day*1000).getUTCDay()||7;if(s.calendar==='date'?s.date!==date:!s.weekdays.includes(weekday))continue;const times=resolveWindow(date,s,d.config.timezone);out.push({schedule:s,key:`${s.id}${s.slot_key?':'+s.slot_key:''}:r${d.revision}:${date}`,inside:!times.invalid&&at>=times.start&&at<times.end,opening:at===times.start,remaining:times.end-at,start:times.start,invalid:times.invalid});}return out;}
 missed(d,from,to,reason){if(to-from>7*86400)this.event(d,'omitted','missed_scan_truncated',{from,to,checked_from:to-7*86400});const windows=new Map();from=Math.max(from,to-7*86400);for(let time=from;time<=to;time+=43200)for(const w of this.windows(d,time))windows.set(w.key,w);for(const w of this.windows(d,to))windows.set(w.key,w);for(const w of windows.values()){const z=d.config.zones.find(z=>z.id===w.schedule.zone_id);if(z.mode!=='dynamic'&&w.start>from&&w.start<=to&&!d.occurrences[w.key]){d.occurrences[w.key]=true;this.decide(d,z,w.key,reason);}}}
 setClock(now){if(!Number.isInteger(now)||now<946684800||now>4102444800||Math.abs(now-this.s.now)>7*86400)fail('Ajuste de relógio deve ser de até sete dias (2000–2100)');const previous=this.s.now;this.s.now=now;for(const d of this.s.devices.filter(d=>d.transport!=='http')){if(now>previous)this.missed(d,previous,now,'clock_adjustment_missed');for(const z of d.config.zones){const a=d.active[z.id];if(a){if(['static','dynamic','hybrid'].includes(a.source))this.stop(d,z,'clock_adjusted');else a.deadline=now+Math.max(0,a.deadline_uptime-(this.s.uptime||0));}}this.event(d,'clock_adjusted','clock_adjusted',{previous_time:previous,new_time:now});}}
 clockValid(d,valid){if(typeof valid!=='boolean')fail('Estado do relógio inválido');d.clock_valid=valid;if(!valid)for(const z of d.config.zones)if(['static','dynamic','hybrid'].includes(d.active[z.id]?.source))this.stop(d,z,'clock_invalid');this.event(d,'clock_quality',valid?'clock_restored':'clock_invalid');}
 power(d,on){if(typeof on!=='boolean')fail('Estado de energia inválido');if(d.powered===on)return;if(!on){for(const z of d.config.zones)this.stop(d,z,'power_loss');d.power_off_at=this.s.now;d.powered=false;}else{this.missed(d,d.power_off_at??this.s.now,this.s.now,'power_off_missed');d.powered=true;d.boot++;d.next_sample_uptime=(this.s.uptime||0)+900;}this.event(d,on?'powered_on':'powered_off',on?'power_restored':'power_loss');}

 tick(){this.s.now++;this.s.uptime=(this.s.uptime||0)+1;for(const d of this.s.devices){if(d.transport==='http'||d.powered===false)continue;d.next_sample_uptime??=this.s.uptime+Math.max(0,d.next_sample-this.s.now);this.applyPending(d);
  // Synthetic readings are refreshed locally even while cloud communication is offline.
  for(const x of d.config.components.filter(x=>x.kind!=='irrigation_actuator')){const s=d.sensors[x.id];if(s?.quality==='valid'&&x.enabled){const watered=Object.keys(d.active).some(zid=>d.config.zones.find(z=>z.id===zid)?.sensor_id===x.id);if(x.kind==='soil_moisture')s.value=Math.max(0,Math.min(100,s.value+(watered?0.05:-0.0005)));s.updated_at=this.s.now;s.updated_uptime=this.s.uptime;}}
  for(const z of d.config.zones){const a=d.active[z.id];if(a){if(this.s.uptime>=(a.deadline_uptime??Infinity))this.stop(d,z,'duration_elapsed');else if(['dynamic','hybrid'].includes(a.source)&&!this.valid(d,z))this.stop(d,z,'sensor_invalid');else if(['dynamic','hybrid'].includes(a.source)&&d.sensors[z.sensor_id].value>=z.target)this.stop(d,z,'target_reached');}}
  const windows=this.windows(d);for(const w of windows)if(w.invalid&&w.opening&&!d.occurrences[w.key]){d.occurrences[w.key]=true;this.decide(d,d.config.zones.find(z=>z.id===w.schedule.zone_id),w.key,'nonexistent_local_time');}
  for(const z of d.config.zones){const ws=windows.filter(w=>w.schedule.zone_id===z.id);if(d.active[z.id]){for(const w of ws)if(w.opening&&!d.occurrences[w.key]){d.occurrences[w.key]=true;this.decide(d,z,w.key,'conflict');}continue;}if(z.mode==='static'){
    for(const w of ws)if(w.opening&&!w.invalid&&!d.occurrences[w.key]){d.occurrences[w.key]=true;if(!d.clock_valid)this.decide(d,z,w.key,'clock_invalid');else this.start(d,z,'static',w.remaining,{occurrence_id:w.key});}
   }else{const eligible=z.mode==='dynamic'?[{key:`${z.id}:dynamic`,remaining:z.max_run_s}]:ws.filter(w=>w.inside&&!d.occurrences[w.key]);for(const w of eligible){if(!d.clock_valid){this.decide(d,z,w.key,'clock_invalid');continue;}if(!this.valid(d,z)){this.decide(d,z,w.key,'sensor_invalid');continue;}if(d.sensors[z.sensor_id].value>=z.start_below){this.decide(d,z,w.key,'moisture_sufficient');continue;}if(this.s.uptime-(d.last_stop_uptime?.[z.id]??-Infinity)<z.min_pause_s){this.decide(d,z,w.key,'cooldown');continue;}if(Object.keys(d.active).length>=d.config.max_simultaneous){this.decide(d,z,w.key,'simultaneous_limit');continue;}if(!d.config.components.find(c=>c.id===z.actuator_id)?.enabled){this.decide(d,z,w.key,'actuator_disabled');continue;}if(this.start(d,z,z.mode,Math.min(w.remaining,z.max_run_s),{occurrence_id:w.key})){if(z.mode==='hybrid')d.occurrences[w.key]=true;break;}}}
  }
  for(const [zid,a] of Object.entries(d.active))if(a.source==='hybrid'&&!windows.some(w=>w.key===a.occurrence_id&&w.inside))this.stop(d,d.config.zones.find(z=>z.id===zid),'window_closed');
  if(this.s.uptime>=d.next_sample_uptime){this.emit({sample:true,device_guid:d.guid,recorded_at:new Date(this.s.now*1000).toISOString(),sensors:structuredClone(d.sensors),outputs:structuredClone(d.active),online:d.online,time_quality:d.clock_valid?'synchronized':'invalid',simulated:true});d.next_sample_uptime+=900;d.next_sample=this.s.now+900;}
 }}
 advance(seconds){if(!Number.isInteger(seconds)||seconds<1||seconds>86400)fail('Avanço entre 1 e 86400 segundos');for(let i=0;i<seconds;i++)this.tick();}
 reboot(d){for(const z of d.config.zones)this.stop(d,z,'reboot');d.boot++;this.event(d,'interrupted','device_reboot');}
}
