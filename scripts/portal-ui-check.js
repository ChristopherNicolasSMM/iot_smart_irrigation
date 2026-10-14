import {createRequire} from 'node:module';import {mkdirSync,writeFileSync} from 'node:fs';import {resolve} from 'node:path';import assert from 'node:assert/strict';
import {createApp} from '../src/server.js';
const require=createRequire(import.meta.url);
let chromium;try{({chromium}=require('playwright'));}catch{throw Error('Teste visual opcional: instale playwright com npm install --no-save --package-lock=false playwright');}
// Own disposable server and local browser. No user bank, credential or GPIO.
const app=createApp({dbPath:':memory:',keyFile:null,vaultRequired:false,adminPassword:'admin-demo-2026',operatorPassword:'operador-demo-2026'});let browser;
try{
 await new Promise(r=>app.server.listen(0,'127.0.0.1',r));const url=`http://127.0.0.1:${app.server.address().port}`;
 const options={headless:true};if(process.env.UI_BROWSER_EXECUTABLE)options.executablePath=process.env.UI_BROWSER_EXECUTABLE;if(process.env.UI_BROWSER_ARGS)options.args=JSON.parse(process.env.UI_BROWSER_ARGS);if(process.env.UI_BROWSER_CHANNEL)options.channel=process.env.UI_BROWSER_CHANNEL;
 browser=await chromium.launch(options);const context=await browser.newContext({viewport:{width:1366,height:900},locale:'pt-BR'}),page=await context.newPage(),errors=[];
 page.on('pageerror',e=>errors.push(e.message));await page.goto(url);
 await page.locator('#login-form input[name="password"]').fill('admin-demo-2026');await page.locator('#login-form button').click();await page.locator('#shell').waitFor({state:'visible'});
 await page.getByRole('button',{name:'Dispositivos e regras',exact:true}).click();
 const schedule=page.locator('.editor[data-group="schedules"]').first();assert.equal(await schedule.locator('.weekday-check').count(),7);
 for(const day of [3,4,5,6,7])await schedule.locator(`[data-weekday="${day}"] .weekday-check`).uncheck();
 await schedule.locator('[data-weekday="1"] input[data-time-key="start_time"]').fill('06:00');await schedule.locator('[data-weekday="2"] input[data-time-key="start_time"]').fill('07:30');
 await page.getByRole('button',{name:'Publicar configuração',exact:true}).click();await page.locator('#config-state').filter({hasText:'pendente'}).waitFor();
 await page.locator('[data-advance="10"]').click();await page.locator('#config-state').filter({hasText:'aplicada'}).waitFor();
 const state=await page.evaluate(async()=>fetch('/api/state').then(r=>r.json()));assert.deepEqual(state.devices[0].config.schedules[0].day_times.map(d=>d.weekday),[1,2]);assert.equal(state.devices[0].config.schedules[0].day_times[1].times[0].start_time,'07:30');
 for(const name of ['Visão geral','Acionamento','Histórico e eventos','Fila de envio','Gerenciar dispositivo','Propriedades e áreas','Minha senha','Integrações']){await page.getByRole('button',{name,exact:true}).click();await page.locator('#page-title').filter({hasText:name}).waitFor();}
 await page.getByRole('button',{name:'Acionamento',exact:true}).click();await page.locator('#stop').click();await page.locator('#command-result').filter({hasText:'completed'}).waitFor();await page.locator('#start').click();await page.locator('#command-result').filter({hasText:'started'}).waitFor();await page.locator('[data-advance="10"]').click();await page.locator('#command-result').filter({hasText:'completed'}).waitFor();
 await page.locator('[data-advance="900"]').click();await page.getByRole('button',{name:'Histórico e eventos',exact:true}).click();await page.locator('#query-history').click();await page.locator('#history-rows').filter({hasText:'Umidade'}).waitFor();
 const artifact=process.env.UI_QA_OUTPUT;if(artifact){await page.locator('#toast').waitFor({state:'hidden'});mkdirSync(artifact,{recursive:true});await page.screenshot({path:resolve(artifact,'desktop-history.png'),fullPage:true});}
 // Hold an already-authorized state response across logout. It must not reopen
 // the old user's panel, even though its HTTP status was 200 before revocation.
 const staleResponse=page.waitForResponse(response=>response.url()===url+'/api/state');let releaseStale,entered;
 const gate=new Promise(r=>releaseStale=r),blocked=new Promise(r=>entered=r);let delayed=false;
 await page.route('**/api/state',async route=>{const response=await route.fetch();if(!delayed){delayed=true;entered();await gate;}await route.fulfill({response});});
 await page.locator('#refresh').click();await blocked;
 await page.locator('#logout').click();await page.locator('#login').waitFor({state:'visible'});releaseStale();await (await staleResponse).finished();await page.unroute('**/api/state');assert.equal(await page.locator('#shell').isVisible(),false);
 await page.locator('#login-form input[name="username"]').fill('operador');await page.locator('#login-form input[name="password"]').fill('operador-demo-2026');await page.locator('#login-form button').click();await page.locator('#shell').waitFor({state:'visible'});assert.equal(await page.locator('[data-tab="config"]').isVisible(),false);assert.equal(await page.locator('[data-tab="integration"]').isVisible(),false);
 await page.setViewportSize({width:390,height:844});await page.getByRole('button',{name:'Visão geral',exact:true}).click();assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1),'Overflow horizontal na largura móvel');
 if(artifact)await page.screenshot({path:resolve(artifact,'mobile-overview.png'),fullPage:true});assert.deepEqual(errors,[],'Erros JavaScript do portal');const report={application_version:'0.13.0',node:process.version,platform:process.platform,browser:browser.version(),viewports:[{width:1366,height:900},{width:390,height:844}],checks:['login_admin','weekly_checkboxes_7','day_times_persisted','configuration_pending_applied','navigation_tabs','internal_command_completed','history_15min','logout','stale_response_after_logout_blocked','operator_restrictions','mobile_no_horizontal_overflow'],page_errors:errors,disposable_database:true};if(artifact)writeFileSync(resolve(artifact,'report.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
}finally{await browser?.close();app.close();}
