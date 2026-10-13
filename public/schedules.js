export const WEEKDAYS = [[1,'Segunda-feira'],[2,'Terça-feira'],[3,'Quarta-feira'],[4,'Quinta-feira'],[5,'Sexta-feira'],[6,'Sábado'],[7,'Domingo']];
export function weeklyDays(schedule){
 if(Array.isArray(schedule.day_times))return structuredClone(schedule.day_times);
 return (schedule.weekdays||[]).map(weekday=>({weekday,times:[{id:'slot_1',start_time:schedule.start_time,termination:schedule.termination,...(schedule.termination==='end_time'?{end_time:schedule.end_time,crosses_midnight:!!schedule.crosses_midnight}:{duration_s:schedule.duration_s})}]}));
}
export function scheduleSlots(schedule){
 if(schedule.calendar==='weekly'&&Array.isArray(schedule.day_times))return schedule.day_times.flatMap(day=>day.times.map(time=>({...schedule,...time,id:schedule.id,weekdays:[day.weekday],slot_key:`d${day.weekday}:${time.id}`})));
 return [{...schedule,slot_key:''}];
}
