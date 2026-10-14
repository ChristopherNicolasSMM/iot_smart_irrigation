import {readFileSync,writeFileSync} from 'node:fs';
const schema=JSON.parse(readFileSync(new URL('../contracts/iot-v1.schema.json',import.meta.url),'utf8'));
const ref=name=>({'$ref':'#/components/schemas/'+name});
const json=name=>({'application/json':{schema:ref(name)}});
const response=(description,name)=>({description,content:json(name)});
const definitions=JSON.parse(JSON.stringify(schema.$defs).replaceAll('#/$defs/','#/components/schemas/'));
const paths={};
const entries=[
 ['/iot/v1/telemetry','post','Receber telemetria','Telemetry','TelemetryAck','accepted confirma persistência local, não entrega externa. Repetição exige mesmo ID e conteúdo.'],
 ['/iot/v1/configuration','get','Consultar configuração desejada',null,'ConfigurationResponse','Aplicar somente após validar hash e limites locais; confirmar revisão/hash em ACK.'],
 ['/iot/v1/configuration/ack','post','Confirmar configuração','ConfigurationAck','AckResponse','Aplicação exige idle=true e hash correspondente. Revisão nunca regride.'],
 ['/iot/v1/commands','get','Consultar comandos',null,'CommandsResponse','reconcile_only=true não autoriza novo acionamento. Comandos expiram em UTC real.'],
 ['/iot/v1/commands/{command_id}/events','post','Confirmar transição de comando','CommandEvent','AckResponse','Confirmação tardia de started exige executed_at na validade original.'],
 ['/iot/v1/runtime','post','Informar controle atual','Runtime','AckResponse','Diagnóstico é opcional para compatibilidade; ausência torna condições do controle desconhecidas. Não cria amostra histórica.']
];
for(const [path,method,summary,request,reply,description] of entries){
 const operation={operationId:summary.normalize('NFD').replace(/[\u0300-\u036f]/g,'').replaceAll(' ','_'),summary,description,tags:['Dispositivo'],security:[{DeviceBearer:[]}],responses:{200:response('Resposta após processamento',reply),400:response('Contrato, referência, revisão ou transição inválida','Error'),401:response('Token ausente, inválido, revogado ou dispositivo arquivado','Error'),404:response('Rota inexistente','Error'),413:response('Corpo acima de 128 KiB','Error'),415:response('POST exige application/json','Error'),500:response('Falha interna; aceitação não confirmada','Error')}};
 if(request)operation.requestBody={required:true,content:json(request)};
 if(path.includes('{command_id}'))operation.parameters=[{name:'command_id',in:'path',required:true,schema:{type:'string',minLength:1,maxLength:100,pattern:'^[A-Za-z0-9_-]+$'}}];
 paths[path]={[method]:operation};
}
const output={openapi:'3.1.0',jsonSchemaDialect:'https://json-schema.org/draft/2020-12/schema',info:{title:'IoT Smart Irrigation — dispositivo HTTP',version:schema['x-contract-version'],description:'Contrato executável do piloto HTTP v1. API humana do portal é documentada em docs/CONTRATO_API.md, fora deste OpenAPI. Limites do ESP32 serão avaliados antes do firmware.'},servers:[{url:'http://127.0.0.1:8080',description:'Laboratório local; produção exige TLS'}],tags:[{name:'Dispositivo',description:'Token por GUID, separado de contas humanas'}],paths,components:{securitySchemes:{DeviceBearer:{type:'http',scheme:'bearer',description:'Credencial emitida e rotacionada por administrador autorizado no portal'}},schemas:definitions}};
const file=new URL('../contracts/openapi.json',import.meta.url),text=JSON.stringify(output,null,2)+'\n';
if(process.argv.includes('--check')){if(readFileSync(file,'utf8')!==text)throw Error('OpenAPI desatualizado: execute npm run contracts:generate');console.log('OpenAPI consistente com schemas');}else writeFileSync(file,text);
