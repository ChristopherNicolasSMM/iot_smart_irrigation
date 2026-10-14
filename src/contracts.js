import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {ValidationError} from './engine.js';
export const contract=JSON.parse(readFileSync(new URL('../contracts/iot-v1.schema.json',import.meta.url),'utf8'));
const supported=new Set(['$schema','$id','$defs','title','description','x-contract-version','$ref','type','const','enum','not','allOf','anyOf','minLength','maxLength','pattern','format','minimum','maximum','minItems','maxItems','uniqueItems','items','required','maxProperties','properties','additionalProperties']);
function checkSchema(schema){
 for(const key of Object.keys(schema))if(!supported.has(key))throw Error('Unsupported contract keyword: '+key);
 if(schema.$ref&&!/^#\/\$defs\/[A-Za-z0-9]+$/.test(schema.$ref))throw Error('Unsupported contract reference');
 if(schema.$ref&&!contract.$defs[schema.$ref.slice(8)])throw Error('Unknown contract reference');
 for(const group of [schema.$defs,schema.properties])for(const child of Object.values(group||{}))checkSchema(child);
 for(const branch of [...schema.allOf||[],...schema.anyOf||[]])checkSchema(branch);
 for(const child of [schema.not,schema.items,schema.additionalProperties])if(child&&typeof child==='object')checkSchema(child);
}
checkSchema(contract);
export const CONTRACT_VERSION=contract['x-contract-version'];
export const configurationHash=config=>createHash('sha256').update(JSON.stringify(config)).digest('hex');
const equal=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
function calendarDate(value){const m=/^(\d{4})-(\d{2})-(\d{2})/.exec(value);if(!m)return false;const d=new Date(0);d.setUTCFullYear(+m[1],+m[2]-1,+m[3]);return d.getUTCFullYear()===+m[1]&&d.getUTCMonth()===+m[2]-1&&d.getUTCDate()===+m[3];}
function formatOK(format,value){
 if(format==='date')return /^\d{4}-\d{2}-\d{2}$/.test(value)&&calendarDate(value);
 if(format==='date-time'){const m=/T(\d{2}):(\d{2}):([0-5]\d)(?:\.\d+)?(?:Z|([+-])(\d{2}):(\d{2}))$/.exec(value);return !!m&&+m[1]<24&&+m[2]<60&&(!m[4]||+m[5]<24&&+m[6]<60)&&calendarDate(value)&&Number.isFinite(Date.parse(value));}
 throw Error('Unsupported contract format: '+format);
}
function matchesType(type,value){return type==='null'?value===null:type==='object'?value!==null&&typeof value==='object'&&!Array.isArray(value):type==='array'?Array.isArray(value):type==='integer'?Number.isSafeInteger(value):type==='number'?typeof value==='number'&&Number.isFinite(value):typeof value===type;}
// Deliberately bounded subset used by our schemas, not a general JSON Schema engine.
// No network refs/eval/coercion/default mutation. Failures expose paths, never values.
function problem(schema,value,path,depth){
 if(depth>64)return path+' excede profundidade';
 if(schema.$ref){const name=schema.$ref.match(/^#\/\$defs\/([A-Za-z0-9]+)$/)?.[1];if(!name||!contract.$defs[name])throw Error('Unsupported contract reference');return problem(contract.$defs[name],value,path,depth+1);}
 if(schema.type&&!([schema.type].flat().some(t=>matchesType(t,value))))return path+' tem tipo inválido';
 if(Object.hasOwn(schema,'const')&&!equal(value,schema.const))return path+' tem valor não permitido';
 if(schema.enum&&!schema.enum.some(v=>equal(v,value)))return path+' tem opção inválida';
 if(schema.not&&!problem(schema.not,value,path,depth+1))return path+' não é permitido';
 if(schema.allOf){for(const branch of schema.allOf){const error=problem(branch,value,path,depth+1);if(error)return error;}}
 if(schema.anyOf&&!schema.anyOf.some(branch=>!problem(branch,value,path,depth+1)))return path+' não atende alternativas do contrato';
 if(typeof value==='string'){
  const length=[...value].length;
  if(schema.minLength!==undefined&&length<schema.minLength||schema.maxLength!==undefined&&length>schema.maxLength)return path+' tem tamanho inválido';
  if(schema.pattern&&!new RegExp(schema.pattern).test(value))return path+' tem formato inválido';
  if(schema.format&&!formatOK(schema.format,value))return path+' tem data inválida';
 }
 if(typeof value==='number'&&(schema.minimum!==undefined&&value<schema.minimum||schema.maximum!==undefined&&value>schema.maximum))return path+' está fora do limite';
 if(Array.isArray(value)){
  if(schema.minItems!==undefined&&value.length<schema.minItems||schema.maxItems!==undefined&&value.length>schema.maxItems)return path+' tem quantidade inválida';
  if(schema.uniqueItems&&new Set(value.map(v=>JSON.stringify(v))).size!==value.length)return path+' tem itens repetidos';
  if(schema.items)for(let i=0;i<value.length;i++){const error=problem(schema.items,value[i],path+'/'+i,depth+1);if(error)return error;}
 }else if(value!==null&&typeof value==='object'){
  for(const key of schema.required||[])if(!Object.hasOwn(value,key)||value[key]===undefined)return path+'/'+key+' obrigatório';
  const entries=Object.entries(value).filter(([,v])=>v!==undefined);
  if(schema.maxProperties!==undefined&&entries.length>schema.maxProperties)return path+' tem propriedades demais';
  for(const [key,item] of entries){const child=schema.properties?.[key];if(child){const error=problem(child,item,path+'/'+key.replaceAll('~','~0').replaceAll('/','~1'),depth+1);if(error)return error;}else if(schema.additionalProperties===false)return path+' contém propriedade não permitida';else if(schema.additionalProperties&&typeof schema.additionalProperties==='object'){const error=problem(schema.additionalProperties,item,path+'/*',depth+1);if(error)return error;}}
 }
 return null;
}
export function contractError(name,value){if(!contract.$defs[name])throw Error('Unknown contract: '+name);return problem(contract.$defs[name],value,'/'+name,0);}
export function assertContract(name,value){const error=contractError(name,value);if(error)throw new ValidationError('Contrato v1: '+error);return value;}
export const machineContract=(path,request=false)=>path==='/iot/v1/configuration'?(request?null:'ConfigurationResponse'):path==='/iot/v1/configuration/ack'?(request?'ConfigurationAck':'AckResponse'):path==='/iot/v1/commands'?(request?null:'CommandsResponse'):path.startsWith('/iot/v1/commands/')?(request?'CommandEvent':'AckResponse'):path==='/iot/v1/runtime'?(request?'Runtime':'AckResponse'):path==='/iot/v1/telemetry'?(request?'Telemetry':'TelemetryAck'):null;
