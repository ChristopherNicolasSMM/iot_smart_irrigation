import {DatabaseSync} from 'node:sqlite';
import {existsSync,linkSync,rmSync,chmodSync} from 'node:fs';
import {resolve,dirname,join} from 'node:path';
import {randomBytes} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {createKeyFile,loadVault,openVault,writeVaultMetadata,fieldContext} from './secrets.js';
export function migrateSecrets({source,target,keyFile,nextKeyFile}){
 if(!existsSync(source)||existsSync(target)||existsSync(target+'-wal')||existsSync(target+'-shm')||resolve(source)===resolve(target))throw Error('Migração exige origem existente e destino novo');
 const targetVault=loadVault(nextKeyFile||keyFile);if(!targetVault.encrypted)throw Error('Defina arquivo de chave de destino');
 let origin,copy;const temp=join(dirname(resolve(target)),'.vault-'+randomBytes(12).toString('hex'));
 try{
  origin=new DatabaseSync(source,{readOnly:true});const encrypted=origin.prepare("SELECT name FROM sqlite_master WHERE name='secrets_meta'").get();const oldVault=openVault(origin,{keyFile:encrypted?keyFile:null,required:false});
  if(origin.prepare('PRAGMA integrity_check').get().integrity_check!=='ok')throw Error('Integridade SQLite inválida');
  origin.prepare('VACUUM INTO ?').run(temp);chmodSync(temp,0o600);origin.close();origin=null;
  copy=new DatabaseSync(temp);copy.exec('PRAGMA secure_delete=ON; PRAGMA journal_mode=DELETE; BEGIN IMMEDIATE');
  const tables=new Set(copy.prepare("SELECT name FROM sqlite_master WHERE type='table'").all().map(x=>x.name));let fields=0;
  for(const table of ['state','controller','integration_headers','outbox']){
   if(!tables.has(table))continue;const idColumn=table==='integration_headers'?'device':'id',columns=table==='outbox'?['body','profile','headers']:['body'];
   for(const row of copy.prepare(`SELECT * FROM ${table}`).iterate()){
    if(table==='outbox'&&row.retained)continue;
    for(const column of columns){const context=fieldContext(table,row[idColumn],column);const text=oldVault.decode(row[column],context);JSON.parse(text);copy.prepare(`UPDATE ${table} SET ${column}=? WHERE ${idColumn}=?`).run(targetVault.encode(text,context),row[idColumn]);fields++;}
   }
  }
  if(!tables.has('state')&&!tables.has('controller'))throw Error('Banco do portal/cliente obrigatório');
  writeVaultMetadata(copy,targetVault);copy.exec('COMMIT; VACUUM');
  if(copy.prepare('PRAGMA integrity_check').get().integrity_check!=='ok')throw Error('Integridade SQLite inválida');copy.close();copy=null;linkSync(temp,target);
  return {path:resolve(target),key_id:targetVault.id,fields,source_unchanged:true};
 }finally{origin?.close();copy?.close();for(const suffix of ['','-journal','-wal','-shm'])rmSync(temp+suffix,{force:true});}
}
if(process.argv[1]===fileURLToPath(import.meta.url)){try{const [action,source,target,...extra]=process.argv.slice(2);if(extra.length)throw Error('Argumentos inválidos');let result;if(action==='keygen'&&source&&!target)result=createKeyFile(source);else if(['migrate','rotate'].includes(action)&&source&&target){if(action==='rotate'&&!process.env.VAULT_NEXT_KEY_FILE)throw Error('Defina VAULT_NEXT_KEY_FILE');result=migrateSecrets({source,target,keyFile:process.env.VAULT_KEY_FILE,nextKeyFile:action==='rotate'?process.env.VAULT_NEXT_KEY_FILE:undefined});}else throw Error('Uso: secrets keygen arquivo | migrate|rotate origem destino');console.log(JSON.stringify(result));}catch(e){console.error(e.message);process.exitCode=1;}}
