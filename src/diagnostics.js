import {scheduleSlots} from '../public/schedules.js';
import {localParts,resolveWindow} from './time.js';
const iso=t=>Number.isFinite(t)?new Date(t*1000).toISOString():null;
const epoch=t=>typeof t==='number'?t:typeof t==='string'?Date.parse(t)/1000:NaN;
const day=(date,n)=>new Date(Date.parse(date)+n*86400000).toISOString().slice(0,10);
export function nextSchedule(config,zone,now,revision,consumed={}){
 if(zone.mode==='dynamic')return null;
 const dates=new Set(),today=localParts(now,config.timezone).date;
 for(let n=-1;n<=14;n++)dates.add(day(today,n));
 for(const s of config.schedules)if(s.calendar==='date'&&s.date>=day(today,-1))dates.add(s.date);
 let best=null;
 for(const s of config.schedules.filter(s=>s.enabled&&s.zone_id===zone.id))for(const slot of scheduleSlots(s))for(const date of dates){
  const weekday=new Date(date+'T00:00:00Z').getUTCDay()||7;
  if(s.calendar==='date'?date!==s.date:!slot.weekdays.includes(weekday))continue;
  const w=resolveWindow(date,slot,config.timezone),key=`${s.id}${slot.slot_key?':'+slot.slot_key:''}:r${revision}:${date}`;
  if(w.invalid||!Number.isFinite(w.start)||!Number.isFinite(w.end)||w.end<=now||consumed[key]||zone.mode==='static'&&w.start<now)continue;
  const eligible=Math.max(now,w.start);if(best&&best.eligible_epoch<=eligible)continue;
  best={schedule_id:s.id,slot_key:slot.slot_key,occurrence_id:key,date,timezone:config.timezone,start_at:iso(w.start),end_at:iso(w.end),eligible_at:iso(eligible),eligible_epoch:eligible,window_open:w.start<=now,conditional:zone.mode==='hybrid',duration_limit_s:Math.min(zone.max_run_s,w.end-eligible)};
 }
 if(best)delete best.eligible_epoch;
 return best;
}
export function controllerStatus(engine,d){
 const consumed=engine.windows(d).filter(w=>w.inside&&d.occurrences[w.key]).map(w=>w.key);
 return {revision:d.revision,clock_valid:d.clock_valid,now:engine.s.now,uptime:engine.s.uptime||0,sensors:Object.fromEntries(d.config.components.filter(c=>c.kind!=='irrigation_actuator').map(c=>[c.id,structuredClone(d.sensors[c.id]||{value:null,quality:'missing'})])),last_stop_uptime:Object.fromEntries(d.config.zones.filter(z=>Number.isFinite(d.last_stop_uptime?.[z.id])).map(z=>[z.id,d.last_stop_uptime[z.id]])),consumed_occurrences:consumed};
}
export function deviceDiagnostics(state,d,realNow=Date.now()/1000){
 const http=d.transport==='http',now=http?realNow:state.now,report=http?d.controller_status:null;
 const received=epoch(d.controller_status_received_at),reportAge=Number.isFinite(received)?now-received:null;
 const authoritative=!http||!!(d.online&&report&&report.revision===d.revision&&reportAge>=0&&reportAge<=90);
 const controller=http?report:{now:state.now,uptime:state.uptime||0,clock_valid:d.clock_valid,sensors:d.sensors,last_stop_uptime:d.last_stop_uptime};
 const consumed=http?Object.fromEntries((authoritative?report.consumed_occurrences||[]:[]).map(k=>[k,true])):d.occurrences||{};
 const lastSeen=http?epoch(d.last_seen):epoch(d.last_communication_at);
 const communication={source:http?'http':'internal_simulator',online:http?!!(d.online&&Number.isFinite(lastSeen)&&now-lastSeen<=90&&now>=lastSeen):d.online&&d.powered!==false,last_seen_at:iso(lastSeen),age_s:Number.isFinite(lastSeen)?Math.max(0,now-lastSeen):null,clock_basis:http?'server_utc':'simulation',heartbeat_limit_s:http?90:null};
 const sensors=d.config.components.filter(c=>c.kind!=='irrigation_actuator').map(c=>{
  const current=authoritative?controller.sensors?.[c.id]:null,s=current||(http?d.last_received?.[c.id]:d.sensors?.[c.id]);
  const stamp=epoch(s?.recorded_at??s?.updated_at);let age=Number.isFinite(stamp)?now-stamp:null;
  if(current&&Number.isFinite(stamp))age=controller.now-stamp+(http?reportAge:0);
  if(current&&Number.isFinite(current.updated_uptime))age=controller.uptime-current.updated_uptime+(http?reportAge:0);
  const quality=!c.enabled?'disabled':!s?'missing':s.quality!=='valid'?s.quality:!Number.isFinite(s.value)?'invalid':age===null||age<0?'invalid_time':age>60?'stale':'valid';
  return {component_id:c.id,name:c.name,kind:c.kind,enabled:c.enabled,value:Number.isFinite(s?.value)?s.value:null,reported_quality:s?.quality||'missing',quality,recorded_at:iso(stamp),received_at:s?.received_at||null,age_s:age,control_age_limit_s:60,source:current?(http?'controller_report':'internal_simulator'):'telemetry_cache',current_control_known:authoritative};
 });
 const groups=d.config.zones.map(z=>{
  const reasons=[],active=d.active[z.id],moisture=sensors.find(s=>s.component_id===z.sensor_id),valve=d.config.components.find(c=>c.id===z.actuator_id);
  const scheduleNow=http&&authoritative?controller.now+reportAge:now;
  const next=nextSchedule(d.config,z,scheduleNow,d.revision,consumed);
  let cooldown=null;
  if(authoritative&&z.mode!=='static'){const last=controller.last_stop_uptime?.[z.id];cooldown=Number.isFinite(last)?Math.max(0,z.min_pause_s-(controller.uptime-last)-(http?reportAge:0)):0;}
  if(!http&&d.powered===false)reasons.push('device_powered_off');
  if(http&&!communication.online)reasons.push('device_offline');
  if(!authoritative)reasons.push('controller_status_unknown');
  if(authoritative&&!controller.clock_valid)reasons.push('clock_invalid');
  if(!valve?.enabled)reasons.push('actuator_disabled');
  if(active)reasons.push('already_running');
  if(Object.keys(d.active).length>=d.config.max_simultaneous&&!active)reasons.push('simultaneous_limit');
  if(z.mode!=='static'){
   if(authoritative&&moisture?.quality!=='valid')reasons.push('sensor_'+(moisture?.quality||'missing'));
   if(authoritative&&moisture?.quality==='valid'&&moisture.value>=z.start_below)reasons.push('moisture_sufficient');
   if(cooldown>0)reasons.push('cooldown');
   if(z.mode==='hybrid'&&!next?.window_open)reasons.push('outside_window');
  }else if(!next?.window_open)reasons.push(next?'waiting_schedule':'no_future_schedule');
  const known=authoritative&&(!http||communication.online),runtimeAge=now-epoch(d.last_runtime_at),outputKnown=!http||communication.online&&runtimeAge>=0&&runtimeAge<=90;
  return {group_id:z.id,name:z.name,mode:z.mode,actuator_id:z.actuator_id,moisture_sensor_id:z.sensor_id||null,temperature_sensor_id:z.temperature_sensor_id||null,observed_output:!outputKnown?'unknown':active?'on':'off',automation_state:!known?'unknown':active?'running':reasons.length?'waiting':'eligible',reasons,cooldown_remaining_s:cooldown,next_schedule:next,forecast_kind:z.mode==='dynamic'?'moisture_condition':z.mode==='hybrid'?'conditional_window':'scheduled_start'};
 });
 return {device_guid:d.guid,generated_at:iso(now),configuration_revision:d.revision,desired_revision:d.desired_revision,configuration_status:d.config_status,communication,controller:{known:authoritative,received_at:http?d.controller_status_received_at||null:null,age_s:http?reportAge:0,clock_valid:authoritative?controller.clock_valid:null,clock_offset_s:http&&report?report.now+Math.max(0,reportAge||0)-now:null},sensors,groups,forecast:{weekly_horizon_days:14,specific_future_dates:true,basis:'applied_configuration',clock_basis:http&&authoritative?'controller_calendar':http?'server_utc':'simulation',note:'Previsão de agenda, não confirmação de acionamento. Estado e motivos descrevem o instante observado; temperatura não controla irrigação.'}};
}
