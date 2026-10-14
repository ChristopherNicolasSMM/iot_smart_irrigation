export function historySeries(items){
 const groups=new Map();for(const item of items){if(!groups.has(item.component_id))groups.set(item.component_id,[]);groups.get(item.component_id).push(item);}
 return [...groups.entries()].map(([id,rows])=>{
  rows.sort((a,b)=>Date.parse(a.recorded_at)-Date.parse(b.recorded_at)||a.record_id-b.record_id);
  const valid=rows.filter(r=>r.quality==='valid'&&Number.isFinite(r.value)),minValue=valid.length?Math.min(...valid.map(r=>r.value)):0,maxValue=valid.length?Math.max(...valid.map(r=>r.value)):1;
  const low=rows[0].metric==='soil_moisture'?0:minValue===maxValue?minValue-1:minValue,high=rows[0].metric==='soil_moisture'?100:minValue===maxValue?maxValue+1:maxValue;
  const first=Date.parse(rows[0].recorded_at),last=Date.parse(rows.at(-1).recorded_at);let previous=null;
  const points=rows.map(row=>{const time=Date.parse(row.recorded_at);if(row.quality!=='valid'||!Number.isFinite(row.value)||!Number.isFinite(time)){previous=null;return null;}const point={x:50+(time-first)*700/Math.max(1,last-first),y:170-(row.value-low)*140/Math.max(.00001,high-low),connect:previous!==null&&time>previous&&time-previous<=1350000};previous=time;return point;});
  return {id,name:rows.at(-1).component_name,unit:rows.at(-1).unit,points,low,high,first:rows[0].recorded_at,last:rows.at(-1).recorded_at,valid_count:valid.length};
 });
}
