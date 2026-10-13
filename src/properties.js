import { randomUUID, randomBytes, scryptSync, timingSafeEqual } from 'node:crypto';
import { ValidationError } from './engine.js';
export class AccessError extends Error { constructor(message='Recurso não encontrado',status=404){super(message);this.status=status;} }
const label = text => typeof text==='string'&&text.trim().length>0&&text.length<=100;
export class Properties {
 constructor(db,state){this.db=db;
  const seedMemberships=!db.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='memberships'").get();
  db.exec('CREATE TABLE IF NOT EXISTS properties(id TEXT PRIMARY KEY,name TEXT NOT NULL,timezone TEXT NOT NULL); CREATE TABLE IF NOT EXISTS areas(id TEXT PRIMARY KEY,property_id TEXT NOT NULL,name TEXT NOT NULL,UNIQUE(property_id,name)); CREATE TABLE IF NOT EXISTS memberships(property_id TEXT,username TEXT,role TEXT NOT NULL,PRIMARY KEY(property_id,username));');
  if(!db.prepare('PRAGMA table_info(users)').all().some(c=>c.name==='system_admin')){db.exec('ALTER TABLE users ADD COLUMN system_admin INTEGER NOT NULL DEFAULT 0');db.prepare("UPDATE users SET system_admin=1 WHERE username='admin'").run();}
  db.prepare("INSERT OR IGNORE INTO properties VALUES('property-default','Propriedade principal','America/Sao_Paulo')").run();
  for(const [column,definition] of [['enabled','INTEGER NOT NULL DEFAULT 1'],['auth_version','INTEGER NOT NULL DEFAULT 0']])if(!db.prepare('PRAGMA table_info(users)').all().some(c=>c.name===column))db.exec(`ALTER TABLE users ADD COLUMN ${column} ${definition}`);
  db.exec('CREATE TABLE IF NOT EXISTS account_audit(id INTEGER PRIMARY KEY,recorded_at TEXT NOT NULL,actor TEXT NOT NULL,target TEXT NOT NULL,action TEXT NOT NULL,property_id TEXT)');
  if(seedMemberships)for(const user of db.prepare('SELECT username,role FROM users').all())if(['admin','operador'].includes(user.username))db.prepare("INSERT OR IGNORE INTO memberships VALUES('property-default',?,?)").run(user.username,user.role);
  for(const d of state.devices){d.property_id??='property-default';const area=this.ensureArea(d.property_id,d.config.area||'Sem área');d.area_id??=area.id;d.config.area_id??=d.area_id;if(d.desired_config)d.desired_config.area_id??=d.area_id;}
 }
 ensureArea(property_id,name){let area=this.db.prepare('SELECT * FROM areas WHERE property_id=? AND name=?').get(property_id,name);if(!area){area={id:randomUUID(),property_id,name};this.db.prepare('INSERT INTO areas VALUES(?,?,?)').run(area.id,property_id,name);}return area;}
 list(username){return this.db.prepare('SELECT p.*,m.role FROM properties p JOIN memberships m ON p.id=m.property_id WHERE m.username=? ORDER BY p.name').all(username);}
 scope(session){const memberships=this.list(session.username);const property=memberships.find(p=>p.id===session.property_id)||memberships[0];if(!property)throw new AccessError('Usuário sem propriedade autorizada',403);session.property_id=property.id;const user=this.db.prepare('SELECT system_admin FROM users WHERE username=?').get(session.username);return {property,role:property.role,system_admin:!!user?.system_admin};}
 areaList(property){return this.db.prepare('SELECT * FROM areas WHERE property_id=? ORDER BY name').all(property);}
 bind(config,property_id){if(!config||typeof config!=='object'||Array.isArray(config))throw new ValidationError('Configuração inválida');const c=structuredClone(config);const property=this.db.prepare('SELECT * FROM properties WHERE id=?').get(property_id);if(!property)throw new AccessError();
  let area;if(c.area_id){area=this.db.prepare('SELECT * FROM areas WHERE id=? AND property_id=?').get(c.area_id,property_id);if(!area)throw new AccessError();}else{if(!label(c.area))throw new ValidationError('Selecione uma área');area=this.ensureArea(property_id,c.area.trim());}
  c.area_id=area.id;c.area=area.name;for(const z of c.zones||[]){const groupArea=z.area_id?this.db.prepare('SELECT * FROM areas WHERE id=? AND property_id=?').get(z.area_id,property_id):area;if(!groupArea)throw new AccessError();z.area_id=groupArea.id;z.area_name=groupArea.name;}c.timezone??=property.timezone;return c;
 }
 saveProperty(session,{id,name,timezone}){const scope=this.scope(session);if(!scope.system_admin)throw new AccessError('Somente administrador do sistema',403);if(!label(name)||typeof timezone!=='string')throw new ValidationError('Nome/fuso inválidos');try{new Intl.DateTimeFormat('pt-BR',{timeZone:timezone});}catch{throw new ValidationError('Fuso inválido');}
  if(id){if(!this.list(session.username).some(p=>p.id===id))throw new AccessError();this.db.prepare('UPDATE properties SET name=?,timezone=? WHERE id=?').run(name.trim(),timezone,id);}else{id=randomUUID();this.db.prepare('INSERT INTO properties VALUES(?,?,?)').run(id,name.trim(),timezone);this.db.prepare("INSERT INTO memberships VALUES(?,?,'admin')").run(id,session.username);this.ensureArea(id,'Área principal');}return {id,name,timezone};
 }
 saveArea(property_id,{id,name}){if(!label(name))throw new ValidationError('Nome de área inválido');const old=id&&this.db.prepare('SELECT * FROM areas WHERE id=? AND property_id=?').get(id,property_id);if(id&&!old)throw new AccessError();const duplicate=this.db.prepare('SELECT * FROM areas WHERE property_id=? AND name=?').get(property_id,name.trim());if(duplicate&&duplicate.id!==id)throw new ValidationError('Já existe uma área com esse nome');if(id)this.db.prepare('UPDATE areas SET name=? WHERE id=?').run(name.trim(),id);else{id=randomUUID();this.db.prepare('INSERT INTO areas VALUES(?,?,?)').run(id,property_id,name.trim());}return {id,property_id,name:name.trim()};}
 createUser(session,{username,password,role,property_id}){if(!this.scope(session).system_admin)throw new AccessError('Somente administrador do sistema',403);if(typeof username!=='string'||!/^[a-zA-Z0-9_-]{3,40}$/.test(username)||typeof password!=='string'||password.length<10||password.length>200||!['admin','operator'].includes(role))throw new ValidationError('Usuário, perfil ou senha inválidos (mínimo 10 caracteres)');if(!this.list(session.username).some(p=>p.id===property_id))throw new AccessError();if(this.db.prepare('SELECT username FROM users WHERE username=?').get(username))throw new ValidationError('Usuário já cadastrado');const salt=randomBytes(16).toString('hex');this.db.prepare('INSERT INTO users(username,role,salt,hash,system_admin) VALUES(?,?,?,?,0)').run(username,role,salt,scryptSync(password,salt,32).toString('hex'));this.db.prepare('INSERT INTO memberships VALUES(?,?,?)').run(property_id,username,role);this.audit(session.username,username,'user_created',property_id);return {username,role,property_id};}
 members(property_id){return this.db.prepare('SELECT m.username,m.role,u.enabled FROM memberships m JOIN users u ON u.username=m.username WHERE property_id=? ORDER BY m.username').all(property_id);}
 saveMember(session,{username,role,property_id}){if(!this.scope(session).system_admin)throw new AccessError('Somente administrador do sistema',403);if(!['admin','operator'].includes(role)||!this.db.prepare('SELECT username FROM users WHERE username=?').get(username))throw new ValidationError('Usuário/perfil inválidos');if(!this.list(session.username).some(p=>p.id===property_id))throw new AccessError();if(username===session.username&&role!=='admin')throw new ValidationError('Administrador do sistema mantém acesso administrativo');this.db.prepare('INSERT INTO memberships VALUES(?,?,?) ON CONFLICT(property_id,username) DO UPDATE SET role=excluded.role').run(property_id,username,role);this.audit(session.username,username,'membership_saved',property_id);return {username,role,property_id};}
 audit(actor,target,action,property_id=null){this.db.prepare('INSERT INTO account_audit(recorded_at,actor,target,action,property_id) VALUES(?,?,?,?,?)').run(new Date().toISOString(),actor,target,action,property_id);}
 account(session,{username,password,current_password,enabled},action){
  if(action!=='password_self'&&!this.scope(session).system_admin)throw new AccessError('Somente administrador do sistema',403);
  if(action==='password_self')username=session.username;
  if(typeof username!=='string'||!/^[a-zA-Z0-9_-]{3,40}$/.test(username))throw new ValidationError('Usuário inválido');
  const user=this.db.prepare('SELECT * FROM users WHERE username=?').get(username);if(!user)throw new AccessError();
  if(action==='enabled'){
   if(typeof enabled!=='boolean')throw new ValidationError('Estado de conta inválido');
   if(!enabled&&user.system_admin)throw new ValidationError('Não é permitido desativar administrador do sistema');
   this.db.prepare('UPDATE users SET enabled=?,auth_version=auth_version+1 WHERE username=?').run(Number(enabled),username);
   this.audit(session.username,username,enabled?'user_enabled':'user_disabled');return {username,enabled};
  }
  if(action==='password_reset'&&username===session.username)throw new ValidationError('Use Minha senha com a senha atual');
  if(action==='password_self'&&(typeof current_password!=='string'||current_password.length>200||!timingSafeEqual(scryptSync(current_password,user.salt,32),Buffer.from(user.hash,'hex'))))throw new ValidationError('Senha atual inválida');
  if(typeof password!=='string'||password.length<10||password.length>200)throw new ValidationError('Senha deve ter entre 10 e 200 caracteres');
  const salt=randomBytes(16).toString('hex');this.db.prepare('UPDATE users SET salt=?,hash=?,auth_version=auth_version+1 WHERE username=?').run(salt,scryptSync(password,salt,32).toString('hex'),username);
  this.audit(session.username,username,action);return {username,sessions_revoked:true};
 }
 revokeMember(session,{username,property_id}){
  if(!this.scope(session).system_admin)throw new AccessError('Somente administrador do sistema',403);
  if(!this.list(session.username).some(p=>p.id===property_id))throw new AccessError();
  if(typeof username!=='string'||!/^[a-zA-Z0-9_-]{3,40}$/.test(username))throw new ValidationError('Usuário inválido');
  if(username===session.username)throw new ValidationError('Administrador mantém seu próprio vínculo');
  if(!this.db.prepare('SELECT username FROM memberships WHERE property_id=? AND username=?').get(property_id,username))throw new AccessError();
  this.db.prepare('DELETE FROM memberships WHERE property_id=? AND username=?').run(property_id,username);
  this.db.prepare('UPDATE users SET auth_version=auth_version+1 WHERE username=?').run(username);
  this.audit(session.username,username,'membership_revoked',property_id);return {username,property_id,sessions_revoked:true};
 }
}
