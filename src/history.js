import {ValidationError} from './engine.js';
const fail=message=>{throw new ValidationError(message);};
const qualities=['valid','invalid','stale','missing','disabled'];
export function historyFilters(params){
 const f={};for(const key of ['from','to'])if(params.get(key)){const value=params.get(key);if(!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?(?:Z|[+-]\d{2}:\d{2})$/.test(value)||!Number.isFinite(Date.parse(value))||new Date(Date.parse(value.slice(0,19)+'Z')).toISOString().slice(0,19)!==value.slice(0,19))fail('Período exige datas ISO com fuso explícito');f[key]=new Date(value).toISOString();}
 if(f.from&&f.to&&f.from>=f.to)fail('Fim do período deve ser posterior ao início');
 for(const key of ['component_id','group_id','kind','quality'])if(params.get(key)){const value=params.get(key);if(value.length>100||!value.trim())fail('Filtro inválido');f[key]=value;}
 if(f.quality&&!qualities.includes(f.quality))fail('Qualidade inválida');
 for(const key of ['limit','before_id','snapshot_id'])if(params.has(key)){const value=params.get(key);if(!/^[1-9]\d*$/.test(value)||!Number.isSafeInteger(Number(value))||key==='limit'&&Number(value)>500)fail('Paginação inválida');f[key]=Number(value);}
 f.limit??=200;return f;
}
const effectiveQuality="CASE WHEN EXISTS(SELECT 1 FROM json_each(q.body,'$.components') c WHERE json_extract(c.value,'$.id')=r.key AND json_extract(c.value,'$.enabled')=0) THEN 'disabled' ELSE COALESCE(json_extract(r.value,'$.quality'),'missing') END";
export function historyCatalog(db,d){
 const components=new Map(),groups=new Map();
 const configs=db.prepare('SELECT body FROM configuration_versions WHERE device=? ORDER BY revision').all(d.guid).map(r=>JSON.parse(r.body));configs.push(d.config);
 for(const c of configs){for(const x of c.components)if(x.kind!=='irrigation_actuator')components.set(x.id,{id:x.id,name:x.name,kind:x.kind});for(const z of c.zones)groups.set(z.id,{id:z.id,name:z.name});}
 for(const r of db.prepare("SELECT DISTINCT s.key AS id FROM samples q,json_each(q.body,'$.sensors') s WHERE q.device=?").all(d.guid))if(!components.has(r.id))components.set(r.id,{id:r.id,name:r.id,kind:'unknown'});
 return {components:[...components.values()],groups:[...groups.values()],event_kinds:db.prepare("SELECT DISTINCT json_extract(body,'$.kind') AS kind FROM events WHERE device=? ORDER BY kind").all(d.guid).map(r=>r.kind).filter(k=>typeof k==='string')};
}
function sqlFilter(type,f){const clauses=['q.device=?'],args=[];
 for(const [key,op] of [['from','>='],['to','<']])if(f[key]){clauses.push(`julianday(json_extract(q.body,'$.recorded_at')) ${op} julianday(?)`);args.push(f[key]);}
 if(f.before_id){clauses.push('q.id<?');args.push(f.before_id);}if(f.snapshot_id){clauses.push('q.id<=?');args.push(f.snapshot_id);}
 if(type==='readings'){
  const sensor=['1=1'];if(f.component_id){sensor.push('r.key=?');args.push(f.component_id);}
  if(f.group_id){sensor.push("EXISTS(SELECT 1 FROM json_each(q.body,'$.groups') g WHERE json_extract(g.value,'$.id')=? AND r.key IN (json_extract(g.value,'$.sensor_id'),json_extract(g.value,'$.temperature_sensor_id')))");args.push(f.group_id);}
  if(f.quality){sensor.push(effectiveQuality+'=?');args.push(f.quality);}
  clauses.push(`EXISTS(SELECT 1 FROM json_each(q.body,'$.sensors') r WHERE ${sensor.join(' AND ')})`);
 }else{
  if(f.group_id){clauses.push("COALESCE(json_extract(q.body,'$.group_id'),json_extract(q.body,'$.zone_id'))=?");args.push(f.group_id);}
  if(f.component_id){clauses.push("? IN (json_extract(q.body,'$.component_id'),json_extract(q.body,'$.sensor_id'),json_extract(q.body,'$.temperature_sensor_id'),json_extract(q.body,'$.actuator_id'))");args.push(f.component_id);}
  if(f.kind){clauses.push("json_extract(q.body,'$.kind')=?");args.push(f.kind);}
 }
 return {where:clauses.join(' AND '),args};
}
export function historyItems(type,row,filters={}){
 const b=JSON.parse(row.body),recorded=Number.isFinite(Date.parse(b.recorded_at))?new Date(b.recorded_at).toISOString():null,common={record_id:row.id,device_guid:row.device,recorded_at:recorded,received_at:b.received_at||null,configuration_revision:b.revision??b.configuration_revision??null,simulated:b.simulated===true};
 if(type==='events')return [{...common,event_id:b.event_id||null,kind:b.kind,reason:b.reason,group_id:b.group_id||b.zone_id||null,group_name:b.group_name||null,component_id:b.component_id||b.actuator_id||null,source:b.origin||b.source||'system',actor:b.actor||null,command_id:b.command_id||null,occurrence_id:b.occurrence_id||null,area_id:b.area_id||null,area_name:b.area||null}];
 return Object.entries(b.sensors||{}).flatMap(([id,s])=>{
  const c=b.components?.find(c=>c.id===id),groups=(b.groups||[]).filter(g=>g.sensor_id===id||g.temperature_sensor_id===id),quality=c?.enabled===false?'disabled':s.quality||'missing';
  if(filters.component_id&&filters.component_id!==id||filters.group_id&&!groups.some(g=>g.id===filters.group_id)||filters.quality&&filters.quality!==quality)return [];
  return [{...common,component_id:id,component_name:c?.name||id,metric:c?.kind||'unknown',unit:c?.kind==='soil_temperature'?'°C':c?.kind==='soil_moisture'?'% relativo':null,value:Number.isFinite(s.value)?s.value:null,quality,reading_at:Number.isFinite(s.updated_at)?new Date(s.updated_at*1000).toISOString():recorded,group_ids:groups.map(g=>g.id),group_names:groups.map(g=>g.name),context_known:Array.isArray(b.groups),online:typeof b.online==='boolean'?b.online:null}];
 });
}
export function queryHistory(db,d,type,params,{exporting=false}={}){
 if(!['readings','events'].includes(type))fail('Tipo de histórico inválido');
 if(exporting&&params.has('before_id'))fail('Exportação não aceita cursor de paginação. Use filtros de período.');
 const filters=historyFilters(params),table=type==='readings'?'samples':'events';
 if(!filters.snapshot_id)filters.snapshot_id=db.prepare(`SELECT MAX(id) AS id FROM ${table} WHERE device=?`).get(d.guid).id||null;
 const {where,args}=sqlFilter(type,filters),limit=exporting?5000:filters.limit;
 const rows=db.prepare(`SELECT q.id,q.device,q.body FROM ${table} q WHERE ${where} ORDER BY q.id DESC LIMIT ?`).all(d.guid,...args,limit+1);
 if(exporting&&rows.length>limit)fail('Exportação limitada a 5000 registros. Restrinja o período ou filtros.');
 const has_more=rows.length>limit,selected=rows.slice(0,limit),items=selected.flatMap(row=>historyItems(type,row,filters));
 if(exporting&&items.length>20000)fail('Exportação limitada a 20000 leituras. Restrinja o período ou filtros.');
 return {schema_version:1,device_guid:d.guid,type,filters,items,page:{records:selected.length,has_more,next_before_id:has_more?selected.at(-1).id:null,snapshot_id:filters.snapshot_id},catalog:historyCatalog(db,d),note:'Período pelo instante de coleta do registro; início inclusivo e fim exclusivo. Ordem de recebimento decrescente. Sem contexto histórico, associação a conjunto é desconhecida.'};
}
export function historyCSV(result){
 const columns=result.type==='events'?['record_id','device_guid','recorded_at','received_at','event_id','configuration_revision','kind','reason','group_id','group_name','component_id','source','actor','command_id','occurrence_id','area_id','area_name','simulated']:['record_id','device_guid','recorded_at','reading_at','received_at','configuration_revision','component_id','component_name','metric','unit','value','quality','group_ids','group_names','context_known','online','simulated'];
 const cell=value=>{let text=Array.isArray(value)?value.join(' | '):value===null||value===undefined?'':String(value);if(typeof value==='string'&&/^[\s]*[=+@-]/.test(text)||Array.isArray(value)&&/^[\s]*[=+@-]/.test(text))text="'"+text;return '"'+text.replaceAll('"','""')+'"';};
 return '\ufeff'+[columns.map(cell).join(';'),...result.items.map(item=>columns.map(k=>cell(item[k])).join(';'))].join('\r\n')+'\r\n';
}
