import {createCipheriv,createDecipheriv,randomBytes,createHash} from 'node:crypto';
import {readFileSync,writeFileSync,statSync} from 'node:fs';
const PREFIX='irrvault1:';
const plainVault={encrypted:false,encode:text=>text,decode:text=>{if(text.startsWith(PREFIX))throw Error('vault_key_required');return text;}};
export function createKeyFile(path){
 const key=randomBytes(32),id=randomBytes(12).toString('hex');
 writeFileSync(path,JSON.stringify({version:1,active:id,keys:{[id]:key.toString('base64')}})+'\n',{flag:'wx',mode:0o600});key.fill(0);
 return {path,key_id:id};
}
export function loadVault(path){
 if(!path)return plainVault;
 const info=statSync(path);if(info.size>16384||process.platform!=='win32'&&(info.mode&0o077))throw Error('Arquivo de chave exige modo 0600 e limite de 16 KiB');
 let file;try{file=JSON.parse(readFileSync(path,'utf8'));}catch{throw Error('Arquivo de chave inválido');}if(file.version!==1||typeof file.active!=='string'||!file.keys||typeof file.keys!=='object'||Array.isArray(file.keys)||!Object.hasOwn(file.keys,file.active)||Object.keys(file.keys).length>8)throw Error('Arquivo de chave inválido');
 const keys=new Map();for(const [id,value] of Object.entries(file.keys)){if(!/^[a-zA-Z0-9_-]{1,64}$/.test(id)||typeof value!=='string'||! /^[A-Za-z0-9+/]{43}=$/.test(value))throw Error('Chave inválida');const key=Buffer.from(value,'base64');if(key.length!==32)throw Error('Chave inválida');keys.set(id,key);}
 const fingerprint=createHash('sha256').update(keys.get(file.active)).digest('hex');
 return {encrypted:true,id:file.active,fingerprint,
  encode(text,context){const iv=randomBytes(12),cipher=createCipheriv('aes-256-gcm',keys.get(file.active),iv);cipher.setAAD(Buffer.from(context));const encrypted=Buffer.concat([cipher.update(text,'utf8'),cipher.final()]);return PREFIX+JSON.stringify({k:file.active,n:iv.toString('base64'),t:cipher.getAuthTag().toString('base64'),c:encrypted.toString('base64')});},
  decode(text,context){if(!text.startsWith(PREFIX))throw Error('vault_plaintext_blocked');try{const data=JSON.parse(text.slice(PREFIX.length)),key=keys.get(data.k);if(!key)throw Error();const iv=Buffer.from(data.n,'base64'),tag=Buffer.from(data.t,'base64');if(iv.length!==12||tag.length!==16)throw Error();const decipher=createDecipheriv('aes-256-gcm',key,iv);decipher.setAAD(Buffer.from(context));decipher.setAuthTag(tag);return Buffer.concat([decipher.update(Buffer.from(data.c,'base64')),decipher.final()]).toString('utf8');}catch{throw Error('vault_authentication_failed');}}
 };
}
export function openVault(db,{keyFile=process.env.VAULT_KEY_FILE,required=process.env.VAULT_REQUIRED==='1'}={}){
 const exists=db.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='secrets_meta'").get();
 const meta=exists?db.prepare('SELECT * FROM secrets_meta WHERE id=1').get():null;
 const vault=loadVault(keyFile);
 if(meta){if(!vault.encrypted||vault.id!==meta.key_id||vault.fingerprint!==meta.fingerprint)throw Error('vault_key_required_or_mismatch');return vault;}
 const hasData=db.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name IN ('state','controller','outbox','integration_headers')").all().some(({name})=>db.prepare(`SELECT 1 FROM ${name} LIMIT 1`).get());
 if(vault.encrypted&&hasData)throw Error('Banco legado: use secrets migrate para novo banco antes de ativar chave');
 if(required&&!vault.encrypted)throw Error('VAULT_KEY_FILE obrigatório');
 if(vault.encrypted)writeVaultMetadata(db,vault);
 return vault;
}
export function writeVaultMetadata(db,vault){db.exec('CREATE TABLE IF NOT EXISTS secrets_meta(id INTEGER PRIMARY KEY,key_id TEXT NOT NULL,fingerprint TEXT NOT NULL)');db.prepare('INSERT INTO secrets_meta VALUES(1,?,?) ON CONFLICT(id) DO UPDATE SET key_id=excluded.key_id,fingerprint=excluded.fingerprint').run(vault.id,vault.fingerprint);}
export const fieldContext=(table,id,column='body')=>JSON.stringify([table,String(id),column]);
