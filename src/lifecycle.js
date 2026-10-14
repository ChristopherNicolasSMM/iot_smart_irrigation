import {ValidationError} from './engine.js';
import {AccessError} from './properties.js';
import {defaultProfile} from './integration.js';
export class DeviceLifecycle {
 constructor(db,state){this.db=db;
  db.exec('CREATE TABLE IF NOT EXISTS device_registry(guid TEXT PRIMARY KEY,property_id TEXT NOT NULL); CREATE TABLE IF NOT EXISTS device_transfers(id INTEGER PRIMARY KEY,device TEXT,source_property TEXT,target_property TEXT,recorded_at TEXT,actor TEXT,source_config TEXT); CREATE TABLE IF NOT EXISTS device_audit(id INTEGER PRIMARY KEY,device TEXT,property_id TEXT,recorded_at TEXT,actor TEXT,action TEXT,detail TEXT);');
  for(const d of state.devices)this.register(d);
  for(const table of ['events','samples','configuration_versions','outbox','machine_commands']){
   if(!db.prepare(`PRAGMA table_info(${table})`).all().some(c=>c.name==='property_id'))db.exec(`ALTER TABLE ${table} ADD COLUMN property_id TEXT`);
   db.exec(`UPDATE ${table} SET property_id=(SELECT property_id FROM device_registry WHERE guid=${table}.device) WHERE property_id IS NULL; CREATE INDEX IF NOT EXISTS ${table}_property ON ${table}(property_id,device); CREATE TRIGGER IF NOT EXISTS ${table}_ownership AFTER INSERT ON ${table} WHEN NEW.property_id IS NULL BEGIN UPDATE ${table} SET property_id=(SELECT property_id FROM device_registry WHERE guid=NEW.device) WHERE rowid=NEW.rowid; END;`);
  }
 }
 register(d){this.db.prepare('INSERT INTO device_registry VALUES(?,?) ON CONFLICT(guid) DO UPDATE SET property_id=excluded.property_id').run(d.guid,d.property_id||'property-default');}
 audit(d,actor,action,detail={}){this.db.prepare('INSERT INTO device_audit(device,property_id,recorded_at,actor,action,detail) VALUES(?,?,?,?,?,?)').run(d.guid,d.property_id,new Date().toISOString(),actor,action,JSON.stringify(detail));}
 historyDevices(state,property){const list=new Map(state.devices.filter(d=>d.property_id===property).map(d=>[d.guid,{guid:d.guid,name:d.config.name,timezone:d.config.timezone,archived:!!d.archived,historical_only:false}]));for(const r of this.db.prepare('SELECT device,source_config FROM device_transfers WHERE source_property=? ORDER BY id DESC').all(property))if(!list.has(r.device)){const c=JSON.parse(r.source_config);list.set(r.device,{guid:r.device,name:c.name,timezone:c.timezone,archived:true,historical_only:true});}return [...list.values()];}
 historyDevice(state,property,guid){const current=state.devices.find(d=>d.guid===guid&&d.property_id===property);if(current)return {...current,history_property_id:property};const row=this.db.prepare('SELECT source_config FROM device_transfers WHERE device=? AND source_property=? ORDER BY id DESC LIMIT 1').get(guid,property);if(!row)throw new AccessError();return {guid,config:JSON.parse(row.source_config),history_property_id:property};}
 idle(d){if(Object.keys(d.active).length)throw new ValidationError('Pare todas as saídas antes desta operação');if(d.config_status!=='applied')throw new ValidationError('Aguarde aplicar ou corrigir a configuração pendente');
  if(this.db.prepare("SELECT id FROM machine_commands WHERE device=? AND status IN ('requested','received','started','unconfirmed') LIMIT 1").get(d.guid))throw new ValidationError('Há comando pendente ou execução sem reconciliação');
  if(this.db.prepare("SELECT id FROM outbox WHERE device=? AND status='sending' LIMIT 1").get(d.guid))throw new ValidationError('Aguarde terminar o envio em andamento');
 }
 archive(d,actor,engine){if(d.archived)throw new ValidationError('Dispositivo já arquivado');this.idle(d);
  if(d.transport==='http'){
   const age=Date.now()-Date.parse(d.last_runtime_at||'');if(!d.online||!Number.isFinite(age)||age<0||age>90000)throw new ValidationError('Cliente HTTP exige confirmação recente de controlador ocioso');
   if(d.config.components.some(c=>c.kind==='irrigation_actuator'&&c.enabled)||d.config.schedules.some(s=>s.enabled))throw new ValidationError('Desabilite solenoides e agendas, publique e aguarde confirmação do cliente antes de arquivar');
  }
  d.archived=true;d.archived_at=new Date().toISOString();d.online=false;
  this.db.prepare('DELETE FROM device_credentials WHERE device=?').run(d.guid);
  const cancelled=this.db.prepare("UPDATE outbox SET status='cancelled',error='device_archived',finished_at=CAST(strftime('%s','now') AS INTEGER)*1000 WHERE device=? AND status='pending'").run(d.guid).changes;
  d.integration??=defaultProfile();d.integration.enabled=false;this.audit(d,actor,'archived',{cancelled_messages:cancelled});engine.event(d,'device_archived','credentials_revoked',{actor,source:'lifecycle',recorded_at:new Date().toISOString()});return {guid:d.guid,archived:true,cancelled_messages:cancelled};
 }
 disabledConfig(d){const c=structuredClone(d.config);for(const x of c.components)if(x.kind==='irrigation_actuator')x.enabled=false;for(const s of c.schedules)s.enabled=false;return c;}
 version(d,c,engine){const revision=Math.max(d.revision,d.desired_revision)+1,time=new Date().toISOString();engine.configure(d,c,revision);d.desired_revision=revision;d.desired_config=structuredClone(c);d.config_status='applied';d.config_error=null;this.db.prepare("INSERT INTO configuration_versions(device,revision,status,body,published_at,applied_at,error) VALUES(?,?,'applied',?,?,?,NULL)").run(d.guid,revision,JSON.stringify(c),time,time);}
 restore(d,actor,engine){if(!d.archived)throw new ValidationError('Dispositivo não está arquivado');const c=this.disabledConfig(d);if(d.transport==='http'){const revision=Math.max(d.revision,d.desired_revision)+1;d.desired_revision=revision;d.desired_config=c;d.config_status='pending';this.db.prepare("INSERT INTO configuration_versions(device,revision,status,body,published_at,applied_at,error) VALUES(?,?,'pending',?,?,NULL,NULL)").run(d.guid,revision,JSON.stringify(c),new Date().toISOString());}else this.version(d,c,engine);
  d.archived=false;d.next_sample_uptime=(engine.s.uptime||0)+900;d.next_sample=engine.s.now+900;d.online=d.transport!=='http'&&d.powered!==false;delete d.controller_status;delete d.controller_status_received_at;delete d.last_runtime_at;d.integration??=defaultProfile();d.integration.enabled=false;this.audit(d,actor,'restored');engine.event(d,'device_restored','outputs_disabled',{actor,source:'lifecycle',recorded_at:new Date().toISOString()});return {guid:d.guid,archived:false,outputs_disabled:true};
 }
 transfer(d,session,body,properties,engine){const scope=properties.scope(session);if(!scope.system_admin)throw new AccessError('Somente administrador do sistema pode transferir',403);if(!d.archived)throw new ValidationError('Arquive o dispositivo antes de transferir');this.idle(d);
  const target=properties.list(session.username).find(p=>p.id===body.property_id&&p.role==='admin');if(!target||target.id===d.property_id)throw new ValidationError('Selecione outra propriedade autorizada');const area=properties.areaList(target.id).find(a=>a.id===body.area_id);if(!area)throw new AccessError('Área de destino não encontrada');
  const source=d.property_id;this.db.prepare('INSERT INTO device_transfers(device,source_property,target_property,recorded_at,actor,source_config) VALUES(?,?,?,?,?,?)').run(d.guid,source,target.id,new Date().toISOString(),session.username,JSON.stringify(d.config));this.audit(d,session.username,'transferred_out',{target_property:target.id});engine.event(d,'device_transferred_out','ownership_changed',{actor:session.username,source:'lifecycle',recorded_at:new Date().toISOString()});
  d.property_id=target.id;this.register(d);const c=this.disabledConfig(d);c.area_id=area.id;c.area=area.name;for(const z of c.zones){z.area_id=area.id;z.area_name=area.name;}
  this.version(d,c,engine);d.ownership_start_revision=d.revision;d.integration=defaultProfile();this.db.prepare('DELETE FROM integration_headers WHERE device=?').run(d.guid);this.db.prepare('DELETE FROM device_credentials WHERE device=?').run(d.guid);d.last_received={};d.last_communication_at=null;delete d.power_off_at;d.sensors={};d.last_stop={};d.last_stop_uptime={};d.occurrences={};d.decisions={};d.commands={};d.online=false;d.last_seen=null;delete d.controller_status;delete d.controller_status_received_at;delete d.last_runtime_at;d.integration??=defaultProfile();d.integration.enabled=false;
  this.audit(d,session.username,'transferred_in',{source_property:source});engine.event(d,'device_transferred_in','outputs_disabled',{actor:session.username,source:'lifecycle',recorded_at:new Date().toISOString()});return {guid:d.guid,property_id:target.id,area_id:area.id,archived:true,credential_revoked:true};
 }
}
