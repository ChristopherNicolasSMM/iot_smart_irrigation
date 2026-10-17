// Capture the exact embedded firmware page against a synthetic localhost API.
// No ESP32, credentials, central service, GPIO or persisted user data is accessed.
import {createServer} from 'node:http';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const require=createRequire(import.meta.url);
const {chromium}=require(process.env.UI_PLAYWRIGHT_MODULE||'playwright');
const output=fileURLToPath(new URL('../iot/docs/manual-esp32/imagens/',import.meta.url));
mkdirSync(output,{recursive:true});
const embedded=readFileSync(new URL('../iot/firmware/portal/LocalPortalPage.h',import.meta.url));
const html=embedded.toString().split('R"HTML(')[1].split(')HTML";')[0];
const guid='00000000-0000-4000-8000-000000000001';
const configuration={name:'Horta de demonstração',area:'Canteiro de hortaliças',timezone:'America/Sao_Paulo',max_simultaneous:1,
 components:[{id:'valve_1',kind:'irrigation_actuator',name:'Solenoide do canteiro',enabled:true},{id:'soil_1',kind:'soil_moisture',name:'Umidade do canteiro',enabled:true},{id:'temp_1',kind:'soil_temperature',name:'Temperatura do solo',enabled:true}],
 zones:[{id:'zone_1',name:'Canteiro principal',actuator_id:'valve_1',sensor_id:'soil_1',temperature_sensor_id:'temp_1',mode:'hybrid',max_run_s:600,min_pause_s:1800,start_below:30,target:45}],
 schedules:[{id:'agenda_horta',zone_id:'zone_1',enabled:true,calendar:'weekly',day_times:[{weekday:1,times:[{id:'manha_seg',start_time:'06:00',termination:'duration',duration_s:600}]},{weekday:3,times:[{id:'manha_qua',start_time:'07:30',termination:'duration',duration_s:600}]}]}]};
function fixture(extra={}){return {offline:false,state:{firmware:'0.6.0-experimental',guid,revision:3,local_role:'admin',local_role_type:'sensor_bench',capabilities:{configure:true,calibrate:true,outbox:true,command:true},clock_valid:true,hardware_enabled:false,remote_output_known:true,boot_hold:false,recovery_hold:false,lost_events:0,tzdb_version:'2026e',csrf:'manual-synthetic-csrf',configuration_body_limit:15872,zones:[{id:'zone_1',active:false,output_known:true}]},...extra};}
let model=fixture();const posts=[],errors=[],captures=[],externalRequests=[];
const server=createServer(async(req,res)=>{
 try{
  const u=new URL(req.url,'http://127.0.0.1');res.setHeader('Cache-Control','no-store');
  if(u.pathname==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');return res.end(html);}
  res.setHeader('Content-Type','application/json');
  const send=(data,status=200)=>{res.statusCode=status;res.end(JSON.stringify(data));};
  if(req.method==='POST'){
   let raw='';for await(const chunk of req){raw+=chunk;if(raw.length>32768)return send({error:'too_large'},413);}
   if(req.headers['x-csrf-token']!==model.state.csrf)return send({error:'csrf_invalid'},403);
   const body=JSON.parse(raw);posts.push(u.pathname);
   if(u.pathname==='/api/command'){model.state.zones[0].active=body.action==='start';return send({command_status:body.action==='start'?'started':'completed'});}
   if(u.pathname==='/api/events/replay')return send({queued:true});
   if(u.pathname==='/api/configuration/draft')return send({error:'revision_conflict'},409);
   if(u.pathname==='/api/outbox/retry')return send({retry_pending:true});
   return send({error:'emulator_operation_not_implemented'},501);
  }
  switch(u.pathname){
   case '/api/state':return model.offline?send({error:'Sem comunicação com a placa (simulação)'},503):send(model.state);
   case '/api/timezones':return send({version:'2026e',names:['America/Sao_Paulo','America/New_York','Australia/Lord_Howe','UTC','Etc/UTC']});
   case '/api/configuration':return send(configuration);
   case '/api/provisioning':return send({profile_revision:2,ssid:'Horta-Demonstracao-2G',portal_url:'',has_operator_password:true,has_wifi_password:true,has_device_token:false,has_portal_ca:false,ip:'192.0.2.10',bindings:{actuators:['valve_1','','',''],moisture:['soil_1','','',''],temperature:['temp_1','','','']}});
   case '/api/sensors':return send({local_available:true,raw:[3080,2010,1790,1900],roms:['281234567890abcd'],calibration:[{slot:0,dry:3200,wet:1200,rom:'281234567890abcd'}]});
   case '/api/events':return send({events:[{sequence_cursor:'42',event_id:'demo-event-42',device_guid:guid,zone_id:'zone_1',actuator_id:'valve_1',kind:'completed',reason:'duration_elapsed',source:'hybrid',recorded_at:'2026-10-14T10:40:00Z',revision:3},{sequence_cursor:'41',event_id:'demo-event-41',device_guid:guid,zone_id:'zone_1',kind:'started',reason:'soil_dry_within_window',source:'hybrid',recorded_at:'2026-10-14T10:30:00Z',revision:3}],next_before:null,retained_limit:64,replay_overwritten:0});
   case '/api/outbox':return send({count:2,capacity:16,head_attempts:5,head_blocked:true,head_digest:'a'.repeat(64),head_path:'/iot/v1/telemetry',head_message_id:'demo-message-42',cancellations:[{recorded_at:'2026-10-16T18:00:00Z',message_id:'demo-message-18',reason:'Mensagem cancelada após conferência do administrador.'}]});
   default:return send({error:'not_found'},404);
  }
 }catch{res.statusCode=500;res.end('{"error":"synthetic_api_error"}');}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const browser=await chromium.launch({headless:true,...(process.env.UI_BROWSER_EXECUTABLE?{executablePath:process.env.UI_BROWSER_EXECUTABLE}:{}),args:JSON.parse(process.env.UI_BROWSER_ARGS||'[]')});
const context=await browser.newContext({viewport:{width:1280,height:900},locale:'pt-BR',timezoneId:'America/Sao_Paulo',deviceScaleFactor:1});
const url=`http://127.0.0.1:${server.address().port}`;let page;context.on('request',request=>{if(!request.url().startsWith(url+'/'))externalRequests.push(request.url());});
async function open(next=fixture(),viewport={width:1280,height:900}){
 if(page)await page.close();model=next;page=await context.newPage();await page.setViewportSize(viewport);
 page.on('pageerror',error=>errors.push(error.message));await page.goto(url);
 await page.locator('#connection').filter({hasText:'conectado'}).waitFor();
}
async function tab(name,ready){await page.locator(`[data-tab=${name}]`).click();if(ready)await page.locator(ready).first().waitFor({state:'visible'});}
async function capture(file,selector=null,fullPage=true){
 await page.locator('body').click({position:{x:4,y:4}});await page.evaluate(()=>document.fonts.ready);
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,`${file}: horizontal overflow`);
 const path=output+file;
 if(selector){const previous=page.viewportSize();const height=await page.evaluate(()=>document.documentElement.scrollHeight);await page.setViewportSize({width:previous.width,height:Math.max(previous.height,height+40)});await page.locator(selector).screenshot({path,animations:'disabled'});await page.setViewportSize(previous);}
 else{await page.evaluate(()=>window.scrollTo(0,0));await page.screenshot({path,fullPage,animations:'disabled'});}
 const bytes=readFileSync(path);captures.push({file,width:bytes.readUInt32BE(16),height:bytes.readUInt32BE(20),bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')});
}
try{
 await open();assert.equal(await page.locator('#start').isEnabled(),false);await capture('01-operacao.png');
 const running=fixture();running.state.hardware_enabled=true;await open(running);await page.locator('#start').click();await page.locator('#command-result').filter({hasText:'started'}).waitFor();await capture('02-teste-simulado.png');await page.locator('#stop').click();await page.locator('#command-result').filter({hasText:'completed'}).waitFor();
 await open();await tab('configuration','#config-editor > .panel');
 await capture('03-dispositivo.png','#config-editor > .panel:nth-child(1)');
 await capture('04-componentes.png','#config-editor > .panel:nth-child(2)');
 await capture('05-conjuntos.png','#config-editor > .panel:nth-child(3)');
 await capture('06-agenda-semanal.png','#config-editor > .panel:nth-child(4)');
 let schedule=page.locator('#config-editor > .panel').nth(3).locator(':scope > .panel').first();
 await schedule.locator('select').nth(1).selectOption('date');
 await schedule.locator('input[type=date]').fill('2026-10-20');await schedule.locator('input[type=date]').blur();
 await schedule.locator('select').nth(2).selectOption('end_time');
 await schedule.locator('input[type=time]').nth(0).fill('23:50');await schedule.locator('input[type=time]').nth(0).blur();
 await schedule.locator('input[type=time]').nth(1).fill('00:10');await schedule.locator('input[type=time]').nth(1).blur();
 await page.getByLabel('Atravessa meia-noite',{exact:true}).check();await capture('07-agenda-data.png','#config-editor > .panel:nth-child(4)');
 await open();await tab('sensors','#dry');await page.locator('#calibration-slot').selectOption('0');await page.locator('#rom-slot').selectOption('0');await capture('08-sensores.png');
 await tab('events','#event-list details');await page.locator('#event-list details').first().locator('summary').click();await capture('09-eventos.png');
 await page.locator('[data-replay]').first().click();await page.locator('#notice').filter({hasText:'identidade original'}).waitFor();
 await open();await tab('outbox','#queue-head');await page.locator('#cancel-reason').fill('Conferência administrativa do envio');await capture('10-fila.png');
 await page.locator('#retry-outbox').click();await page.locator('#notice').filter({hasText:'Reprocessamento solicitado'}).waitFor();
 await open();await tab('provisioning','#provision-info');await page.locator('#provision-info').filter({hasText:'Revisão 2'}).waitFor();
 for(const id of ['wifi-password','admin-password','operator-password','token','ca'])assert.equal(await page.locator('#provision-'+id).inputValue(),'');
 await capture('11-wifi-acesso.png');
 const operator=fixture();operator.state.local_role='operator';operator.state.capabilities={configure:false,calibrate:false,outbox:false,command:true};await open(operator);assert.equal(await page.locator('[data-tab=configuration]').isVisible(),false);await capture('12-operador.png');
 const recovery=fixture();recovery.state.recovery_hold=true;recovery.state.boot_hold=true;await open(recovery);await tab('provisioning','#activate-recovery');await capture('13-recuperacao.png');
 await open(fixture(),{width:390,height:844});await capture('14-celular.png');
 const central=fixture();central.state.local_role_type='matrix';central.state.capabilities.configure=false;central.state.capabilities.calibrate=false;await open(central);await tab('configuration','#config-editor > .panel');assert.equal(await page.locator('#config-editor input').first().isDisabled(),true);await capture('15-configuracao-central.png',null,false);
 await open();model.offline=true;await page.locator('#connection').filter({hasText:'Sem comunicação'}).waitFor({timeout:10000});await capture('16-sem-comunicacao.png');
 assert.deepEqual(errors,[]);assert.deepEqual(externalRequests,[]);
 const report={captured_at:new Date().toISOString(),firmware:'0.6.0-experimental',source_commit:process.env.MANUAL_SOURCE_COMMIT||null,embedded_page_sha256:createHash('sha256').update(embedded).digest('hex'),browser:await browser.version(),scope:'Exact embedded page with synthetic localhost HTTP API; no ESP32 CPU/SDK emulation, real authentication, GPIO, sensor, flash or central connection.',viewport:{desktop:'1280x900',mobile:'390x844'},external_requests:externalRequests,javascript_errors:errors,synthetic_post_routes:posts,captures};
 writeFileSync(new URL('../iot/docs/manual-esp32/capturas.json',import.meta.url),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({screenshots:captures.length,bytes:captures.reduce((sum,row)=>sum+row.bytes,0),javascript_errors:errors},null,2));
}finally{await context.close();await browser.close();await new Promise(resolve=>server.close(resolve));}
