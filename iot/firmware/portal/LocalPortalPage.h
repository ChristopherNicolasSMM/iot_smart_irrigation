#pragma once
#include <Arduino.h>

// Linked into firmware flash; no filesystem or external web assets required.
namespace irrigation {
static const char localPortalPage[] PROGMEM =
    R"HTML(<!doctype html><html lang="pt-BR"><meta name="viewport" content="width=device-width"><title>Irrigação local</title><h1>Irrigação local</h1><p>Saída elétrica não confirma vazão. Hardware experimental.</p><pre id="state"></pre><label>Conjunto <select id="zone"></select></label><label>Teste (s) <input id="duration" type="number" min="1" max="30" value="5"></label><button onclick="act('start')">Testar</button><button onclick="act('stop')">Parar</button><pre id="result"></pre><script>let token='';async function refresh(){let r=await fetch('/api/state');if(!r.ok)return;let d=await r.json();token=d.csrf;state.textContent=JSON.stringify(d,null,2);let old=zone.value;zone.replaceChildren(...d.zones.map(z=>{let o=document.createElement('option');o.value=z.id;o.textContent=z.id;return o}));zone.value=old||zone.options[0]?.value||''}async function act(action){let r=await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json','X-CSRF-Token':token},body:JSON.stringify({zone_id:zone.value,action,source:'test',duration_s:Number(duration.value)})});result.textContent=await r.text();refresh()}refresh();setInterval(refresh,3000)</script></html>)HTML";
} // namespace irrigation
