import { scheduleSlots } from '../public/schedules.js';
import { localParts,resolveWindow } from './time.js';
const shift=(date,days)=>new Date(Date.parse(date)+days*86400000).toISOString().slice(0,10);
// Advisory only: calendar windows, not predicted physical valve operation.
export function scheduleWarnings(config,now){
 const first=localParts(now,config.timezone).date,dates=new Set();
 for(let day=-1;day<=7;day++)dates.add(shift(first,day));
 for(const agenda of config.schedules)if(agenda.enabled&&agenda.calendar==='date')for(let day=-1;day<=1;day++)dates.add(shift(agenda.date,day));
 const zones=new Map(config.zones.filter(z=>config.components.some(c=>c.id===z.actuator_id&&c.enabled)).map(z=>[z.id,z]));
 const windows=[],warnings=[],seen=new Set();let truncated=false;
 const add=(key,warning)=>{if(seen.has(key))return;seen.add(key);if(warnings.length<50)warnings.push(warning);else truncated=true;};
 for(const agenda of config.schedules){const zone=zones.get(agenda.zone_id);if(!agenda.enabled||!zone||zone.mode==='dynamic')continue;
  for(const slot of scheduleSlots(agenda))for(const date of dates){const weekday=new Date(date+'T00:00:00Z').getUTCDay()||7;if(agenda.calendar==='date'?date!==agenda.date:!slot.weekdays.includes(weekday))continue;
   const window=resolveWindow(date,slot,config.timezone);if(window.invalid||!Number.isFinite(window.start)||!Number.isFinite(window.end))continue;
   windows.push({...window,date,zone_id:zone.id,group_name:zone.name,schedule_id:agenda.id,slot_key:slot.slot_key,start_time:slot.start_time});
  }
 }
 windows.sort((a,b)=>a.start-b.start||a.end-b.end);
 let active=[];
 for(const window of windows){active=active.filter(w=>w.end>window.start);
  for(const other of active)if(other.zone_id===window.zone_id){const refs=[other,window].map(w=>`${w.schedule_id}/${w.slot_key}`).sort();add('same:'+refs.join('|'),{code:'same_solenoid_overlap',message:`Agendas sobrepostas no conjunto ${window.group_name}.`,groups:[window.zone_id],schedules:[other.schedule_id,window.schedule_id],example_date:window.date,start_time:window.start_time});}
  active.push(window);const groups=[...new Set(active.map(w=>w.zone_id))].sort();if(groups.length>config.max_simultaneous)add('capacity:'+groups.join('|'),{code:'simultaneous_limit',message:`Janelas de ${groups.length} conjuntos ultrapassam o limite de ${config.max_simultaneous} saídas simultâneas.`,groups,example_date:window.date,start_time:window.start_time});
 }
 const dynamic=[...zones.values()].filter(z=>z.mode==='dynamic');
 if(dynamic.length&&zones.size>config.max_simultaneous)add('dynamic',{code:'dynamic_capacity',message:'Acionamentos por umidade podem disputar capacidade com outros conjuntos; o limite será aplicado durante a execução.',groups:[...zones.keys()]});
 return {warnings,truncated,scope:{timezone:config.timezone,weekly_from:first,weekly_through:shift(first,7),specific_dates:true},note:'Avisos não impedem publicação. Janelas híbridas indicam possibilidade; execução depende da umidade. Capacidade hidráulica e acionamentos manuais não são calculados.'};
}
