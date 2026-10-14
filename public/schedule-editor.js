import {WEEKDAYS,weeklyDays} from './schedules.js';
const esc=x=>String(x??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const options=(list,value)=>list.map(([v,name])=>`<option value="${esc(v)}" ${v===value?'selected':''}>${esc(name)}</option>`).join('');
const input=(key,value,type='text')=>`<input data-key="${key}" type="${type}" value="${esc(value)}">`;
const field=(name,html)=>`<label>${name}${html}</label>`;
const select=(key,list,value)=>`<select data-key="${key}">${options(list,value)}</select>`;
export function scheduleEditorHTML(s,i,model){
 const days=weeklyDays(s),first=days[0]?.times[0]||{},dateTime={start_time:s.start_time||first.start_time||'06:00',termination:s.termination||first.termination||'duration',duration_s:s.duration_s??first.duration_s??600,end_time:s.end_time||first.end_time||'06:30',crosses_midnight:s.crosses_midnight??first.crosses_midnight??false};
 const timeFields=(t,attr='data-time-key')=>{const f=(key,value,type='text')=>`<input ${attr}="${key}" type="${type}" value="${esc(value)}">`,sel=(key,list,value)=>`<select ${attr}="${key}">${options(list,value)}</select>`;return `<div class="form-grid">${field('Hora inicial',f('start_time',t.start_time||'06:00','time'))}${field('Término',sel('termination',[['duration','Duração'],['end_time','Hora final']],t.termination||'duration'))}${field('Duração (s)',f('duration_s',t.duration_s??600,'number'))}${field('Hora final',f('end_time',t.end_time||'06:30','time'))}${field('Hora final no dia seguinte',sel('crosses_midnight',[['false','Não'],['true','Sim']],String(t.crosses_midnight||false)))}</div>`;};
 return `<div class="editor" data-group="schedules" data-index="${i}"><div class="form-grid">${field('Identificador',input('id',s.id))}${field('Conjunto / solenoide',select('zone_id',model.zones.map(z=>[z.id,z.name+' · '+(model.components.find(c=>c.id===z.actuator_id)?.name||'sem solenoide')]),s.zone_id))}${field('Habilitada',select('enabled',[['true','Sim'],['false','Não']],String(s.enabled)))}${field('Calendário',select('calendar',[['weekly','Dias da semana'],['date','Data específica']],s.calendar))}</div><div class="weekly-block" ${s.calendar!=='weekly'?'hidden':''}>${WEEKDAYS.map(([weekday,name])=>{const day=days.find(d=>d.weekday===weekday),times=day?.times||[{id:'slot_1',start_time:'06:00',termination:'duration',duration_s:600}];return `<div class="day-editor" data-weekday="${weekday}"><label class="day-check"><input type="checkbox" class="weekday-check" ${day?'checked':''}>${name}</label><div class="day-times" ${!day?'hidden':''}>${times.map(t=>`<div class="time-row" data-slot-id="${esc(t.id)}">${timeFields(t)}<button type="button" class="secondary remove-slot">Remover horário</button></div>`).join('')}<button type="button" class="secondary add-slot">Adicionar horário neste dia</button></div></div>`;}).join('')}</div><div class="date-block" ${s.calendar!=='date'?'hidden':''}>${field('Data',input('date',s.date||'','date'))}${timeFields(dateTime,'data-key')}</div><button type="button" class="secondary remove">Remover agenda</button></div>`;

}

export function newIdentifier(prefix,used=[],retired=[],random=()=>crypto.randomUUID()){
 const unavailable=new Set([...used,...retired]);
 for(let attempt=0;attempt<32;attempt++){const candidate=prefix+'_'+random().slice(0,8);if(!unavailable.has(candidate))return candidate;}
 throw Error('Não foi possível gerar identificador novo; tente novamente.');
}
