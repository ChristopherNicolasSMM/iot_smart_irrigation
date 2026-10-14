import {DatabaseSync} from 'node:sqlite';
import {createCipheriv,createDecipheriv,randomBytes,scryptSync,createHash} from 'node:crypto';
import {readFileSync,writeFileSync,mkdtempSync,rmSync,existsSync,linkSync,statSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {openVault,fieldContext} from './secrets.js';

const MAGIC=Buffer.from('IRRBACK1'),LIMIT=64*1024*1024;
const sha256=bytes=>createHash('sha256').update(bytes).digest('hex');
function validatePassword(password){
 if(typeof password!=='string'||password.length<16||password.length>1024)throw Error('BACKUP_PASSWORD deve ter entre 16 e 1024 caracteres');
}
function passwordKey(password,salt){validatePassword(password);return scryptSync(password,salt,32);}
function inspect(db,keyFile){
 if(db.prepare('PRAGMA integrity_check').get().integrity_check!=='ok')throw Error('Integridade SQLite inválida');
 const tables=db.prepare("SELECT name FROM sqlite_master WHERE type='table'").all().map(x=>x.name);
 const kind=tables.includes('state')?'portal':'client',table=kind==='portal'?'state':'controller';
 const required=kind==='portal'?['state','users','configuration_versions','outbox','device_credentials','machine_commands']:['controller','command_acks','outbox','integration_headers'];
 if(!required.every(t=>tables.includes(t)))throw Error('Backup exige banco do portal v0.10+ ou cliente independente');
 const vault=openVault(db,{keyFile,required:false}),row=db.prepare(`SELECT body FROM ${table} WHERE id=1`).get();
 const state=JSON.parse(row?vault.decode(row.body,fieldContext(table,1)):'null');
 if(!state||!Array.isArray(state.devices))throw Error('Estado inválido');
 return {state,kind,vault,table};
}
function requireNewDestination(source,target){
 if(existsSync(target)||existsSync(target+'-wal')||existsSync(target+'-shm')||resolve(source)===resolve(target))throw Error('Operação exige destino novo sem arquivos WAL/SHM');
}
function encryptSnapshot(bytes,kind,password){
 const salt=randomBytes(16),iv=randomBytes(12),key=passwordKey(password,salt);
 try{
  const metadata=Buffer.from(JSON.stringify({schema:1,kind,created_at:new Date().toISOString(),sha256:sha256(bytes)}));
  const plain=Buffer.concat([Buffer.from(String(metadata.length).padStart(8,'0')),metadata,bytes]);
  const cipher=createCipheriv('aes-256-gcm',key,iv);cipher.setAAD(MAGIC);
  const encrypted=Buffer.concat([cipher.update(plain),cipher.final()]);
  return Buffer.concat([MAGIC,salt,iv,cipher.getAuthTag(),encrypted]);
 }finally{key.fill(0);}
}
function decryptSnapshot(source,password){
 if(statSync(source).size>LIMIT+4096)throw Error('Backup excede limite');
 const input=readFileSync(source);
 if(input.length<60||!input.subarray(0,8).equals(MAGIC))throw Error('Formato de backup inválido');
 const key=passwordKey(password,input.subarray(8,24));let plain;
 try{
  const decipher=createDecipheriv('aes-256-gcm',key,input.subarray(24,36));
  decipher.setAAD(MAGIC);decipher.setAuthTag(input.subarray(36,52));
  plain=Buffer.concat([decipher.update(input.subarray(52)),decipher.final()]);
 }catch{throw Error('Senha incorreta ou backup adulterado');}finally{key.fill(0);}
 const length=Number(plain.subarray(0,8).toString());
 if(!Number.isInteger(length)||length<1||length>2048)throw Error('Metadados inválidos');
 let meta;try{meta=JSON.parse(plain.subarray(8,8+length).toString());}catch{throw Error('Metadados inválidos');}
 const bytes=plain.subarray(8+length);
 if(meta.schema!==1||!['portal','client'].includes(meta.kind)||meta.sha256!==sha256(bytes))throw Error('Conteúdo inválido');
 return {meta,bytes};
}
function cleanup(path){for(const suffix of ['','-journal','-wal','-shm'])rmSync(path+suffix,{force:true});}
function quarantine(db,{state,kind,vault,table},meta){
 const restored_at=new Date().toISOString();state.paused=true;
 if(kind==='client')state.recovery_hold=true;
 if(state.integration)state.integration.enabled=false;
 for(const d of state.devices){
  d.archived=true;d.online=false;d.active={};
  if(kind==='portal')d.commands={};
  d.config_status='applied';d.desired_revision=d.revision;d.desired_config=structuredClone(d.config);
  if(d.integration)d.integration.enabled=false;
  delete d.controller_status;delete d.controller_status_received_at;delete d.last_runtime_at;
 }
 db.prepare(`UPDATE ${table} SET body=? WHERE id=1`).run(vault.encode(JSON.stringify(state),fieldContext(table,1)));
 db.exec('DELETE FROM integration_headers');
 if(kind==='portal')db.exec('DELETE FROM device_credentials; UPDATE users SET auth_version=auth_version+1');
 else db.exec('DELETE FROM command_acks');
 db.prepare("UPDATE outbox SET status='cancelled',error='backup_restored',finished_at=? WHERE status IN ('pending','sending')").run(Date.now());
 if(kind==='portal')db.prepare("UPDATE machine_commands SET status=CASE WHEN status='requested' THEN 'expired' ELSE 'unconfirmed' END WHERE status IN ('requested','received','started')").run();
 db.exec('CREATE TABLE IF NOT EXISTS recovery_audit(id INTEGER PRIMARY KEY,body TEXT NOT NULL)');
 db.prepare('INSERT INTO recovery_audit(body) VALUES(?)').run(JSON.stringify({restored_at,kind,backup_created_at:meta.created_at,source_sha256:meta.sha256,reason:'restore_quarantine',devices:state.devices.length}));
}
export function createBackup({source,target,password,keyFile=process.env.VAULT_KEY_FILE}){
 requireNewDestination(source,target);
 if(!existsSync(source))throw Error('Banco de origem inexistente');
 // Validate the password before any disk snapshot is created.
 validatePassword(password);
 const dir=mkdtempSync(join(tmpdir(),'irrigation-backup-'));let db;
 const artifact=join(dirname(resolve(target)),'.backup-'+randomBytes(12).toString('hex'));
 try{
  db=new DatabaseSync(source,{readOnly:true});const {kind}=inspect(db,keyFile);
  const snapshot=join(dir,'snapshot.sqlite');db.prepare('VACUUM INTO ?').run(snapshot);db.close();db=null;
  if(statSync(snapshot).size>LIMIT)throw Error('Banco excede limite de 64 MiB');
  writeFileSync(artifact,encryptSnapshot(readFileSync(snapshot),kind,password),{flag:'wx',mode:0o600});
  linkSync(artifact,target);
  return {path:resolve(target),bytes:statSync(target).size,kind};
 }finally{db?.close();cleanup(artifact);rmSync(dir,{recursive:true,force:true});}
}
export function restoreBackup({source,target,password,keyFile=process.env.VAULT_KEY_FILE}){
 requireNewDestination(source,target);
 const {meta,bytes}=decryptSnapshot(source,password);
 const temporary=join(dirname(resolve(target)),'.restore-'+randomBytes(12).toString('hex'));let db;
 try{
  writeFileSync(temporary,bytes,{flag:'wx',mode:0o600});db=new DatabaseSync(temporary);
  const context=inspect(db,keyFile);
  if(context.kind!==meta.kind)throw Error('Tipo de backup inválido');
  db.exec('PRAGMA journal_mode=DELETE; BEGIN IMMEDIATE');quarantine(db,context,meta);db.exec('COMMIT');
  inspect(db,keyFile);db.close();db=null;linkSync(temporary,target);
  return {path:resolve(target),quarantined:true,kind:context.kind,devices:context.state.devices.length};
 }finally{db?.close();cleanup(temporary);}
}
if(process.argv[1]===fileURLToPath(import.meta.url)){
 try{
  const [action,source,target,...extra]=process.argv.slice(2);
  if(!source||!target||extra.length||!['create','restore'].includes(action))throw Error('Uso: npm run backup -- create|restore origem destino');
  const result=(action==='create'?createBackup:restoreBackup)({source,target,password:process.env.BACKUP_PASSWORD});console.log(JSON.stringify(result));
 }catch(e){console.error(e.message);process.exitCode=1;}
}
