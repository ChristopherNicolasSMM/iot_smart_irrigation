import {createHash,randomUUID} from 'node:crypto';
import {mappedPayload} from './integration.js';
import {ValidationError} from './engine.js';
// Runs inside the same transaction as ingestion. HTTP ACK confirms local storage,
// not delivery to a third party. Forwarded IDs are stable across retries/restarts.
export class TelemetryForwarder {
 constructor(db,state,outbox,onOutcome){this.db=db;this.state=state;this.outbox=outbox;this.onOutcome=onOutcome;state.gateway_id??=randomUUID();}
 forward(d,packet,result){
  if(result.duplicate)return {status:'duplicate'};
  const profile=d.integration||this.state.integration;if(!profile.enabled)return {status:'disabled'};
  const trace=packet.integration_trace||[];
  if(trace.includes(this.state.gateway_id)||trace.length>=4)return {status:'loop_suppressed'};
  if(this.db.prepare('SELECT id FROM outbox WHERE id=? AND device=?').get(packet.message_id,d.guid))return {status:'local_outbox_suppressed'};
  if(!packet.readings.length&&packet.events.length&&packet.events.every(e=>e.source==='integration'))return {status:'integration_events_suppressed'};
  const message_id='forward-'+createHash('sha256').update(JSON.stringify([this.state.gateway_id,d.property_id,d.guid,packet.message_id])).digest('hex');
  const message={...packet,outputs:packet.outputs||[],message_id,source_message_id:packet.source_message_id||packet.message_id,integration_trace:[...trace,this.state.gateway_id]};
  try{
   const payload=mappedPayload(profile,d,message,{received:true});payload.integration_trace=message.integration_trace;payload.source_message_id=message.source_message_id;
   const queued=this.outbox.enqueue(d.guid,payload,profile,{ownership_revision:d.ownership_start_revision||0,source_kind:'http_ingest'});return {status:queued?'pending':'dropped',message_id};
  }catch(e){if(!(e instanceof ValidationError))throw e;this.onOutcome({device:d.guid,status:'dropped',message_id,attempts:0,reason:'template_invalid'});return {status:'dropped',message_id};}
 }
}
