import test from 'node:test';
import assert from 'node:assert/strict';
import {portal,packet} from './helpers.js';
import {assertContract} from '../src/contracts.js';
const observation=(d,extra={})=>({event_id:'schedule-firmware-stable-id',device_guid:d.guid,
  kind:'recovery_observation',reason:'unobserved_window',source:'static',
  recorded_at:'2040-01-02T12:00:00Z',revision:d.revision,zone_id:'zone_1',
  schedule_id:'morning',time_slot_id:'monday',occurrence_id:'a123456789012345',
  window_start:'2040-01-01T06:00:00Z',window_end:'2040-01-01T06:10:00Z',...extra});
test('observação de firmware preserva agenda/janela e deduplica após reboot',async t=>{
  const f=await portal(t),event=observation(f.d);
  const first=packet(f.d,'firmware-boot-a',{readings:[],events:[event],simulated:false});
  assertContract('Telemetry',first);
  assert.equal((await f.call('/iot/v1/telemetry',first,f.token)).status,200);
  const second=packet(f.d,'firmware-boot-b',{readings:[],events:[{...event,recorded_at:'2040-01-03T12:00:00Z'}],simulated:false});
  assert.equal((await f.call('/iot/v1/telemetry',second,f.token)).status,200);
  const rows=f.app.db.prepare("SELECT body FROM events WHERE json_extract(body,'$.event_id')=?").all(event.event_id);
  assert.equal(rows.length,1);const saved=JSON.parse(rows[0].body);
  for(const key of ['schedule_id','time_slot_id','occurrence_id','window_start','window_end']) assert.equal(saved[key],event[key]);
  assert.equal(saved.simulated,false);assert.equal(saved.kind,'recovery_observation');
});
test('conflito de firmware mantém agenda omitida e agenda concorrente sem acionar saída',async t=>{
  const f=await portal(t),event=observation(f.d,{event_id:'schedule-conflict-stable-id',kind:'omitted',reason:'schedule_overlap',competing_schedule_id:'evening'});
  const body=packet(f.d,'firmware-conflict',{readings:[],events:[event],simulated:false});
  assert.equal((await f.call('/iot/v1/telemetry',body,f.token)).status,200);
  const saved=JSON.parse(f.app.db.prepare("SELECT body FROM events WHERE json_extract(body,'$.event_id')=?").get(event.event_id).body);
  assert.equal(saved.competing_schedule_id,'evening');assert.equal(saved.schedule_id,'morning');
  assert.equal(f.app.db.prepare('SELECT COUNT(*) n FROM machine_commands').get().n,0);
});
