const formatters=new Map(),cache=new Map();
export function localParts(t,tz){
 if(!formatters.has(tz))formatters.set(tz,new Intl.DateTimeFormat('en-CA',{timeZone:tz,year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit',hourCycle:'h23'}));
 const p=Object.fromEntries(formatters.get(tz).formatToParts(new Date(t*1000)).filter(p=>p.type!=='literal').map(p=>[p.type,p.value]));
 return {date:`${p.year}-${p.month}-${p.day}`,minute:+p.hour*60+(+p.minute),second:+p.second};
}
const addDay=date=>new Date(Date.parse(date)+86400000).toISOString().slice(0,10);
function instants(date,time,tz){
 const target=Date.parse(date+'T'+time+':00Z')/1000,offsets=new Set();
 for(let h=-36;h<=36;h+=6){const epoch=target+h*3600,p=localParts(epoch,tz);offsets.add(Date.parse(p.date+'T00:00:00Z')/1000+p.minute*60+p.second-epoch);}
 return [...offsets].map(offset=>target-offset).filter(epoch=>{const p=localParts(epoch,tz);return p.date===date&&p.minute===Number(time.slice(0,2))*60+Number(time.slice(3))&&p.second===0;}).sort((a,b)=>a-b);
}
function omissionInstant(date,time,tz){
 // Find the first valid civil minute after a gap, including a skipped calendar
 // date. This instant records the omission; invalid windows never authorize starts.
 const civil=Date.parse(date+'T'+time+':00Z');
 for(let n=1;n<=2880;n++){
  const candidate=new Date(civil+n*60000).toISOString(),epoch=instants(candidate.slice(0,10),candidate.slice(11,16),tz)[0];
  if(epoch!==undefined)return epoch;
 }
 return undefined;
}
export function resolveWindow(date,slot,tz){
 const key=JSON.stringify([date,slot.start_time,slot.termination,slot.duration_s,slot.end_time,slot.crosses_midnight,tz]);
 if(cache.has(key))return cache.get(key);
 let start=instants(date,slot.start_time,tz)[0],end,invalid=false;
 if(start===undefined){invalid=true;start=omissionInstant(date,slot.start_time,tz);}
 if(slot.termination==='duration')end=start+slot.duration_s;
 else{const values=instants(slot.crosses_midnight?addDay(date):date,slot.end_time,tz);end=values.find(t=>t>start);if(end===undefined)invalid=true;}
 const result={start,end,invalid};if(cache.size>=2048)cache.delete(cache.keys().next().value);cache.set(key,result);return result;
}
