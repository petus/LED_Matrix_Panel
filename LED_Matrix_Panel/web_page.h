/**
 * @file    web_page.h
 * @brief   The settings web page (HTML + CSS + JS in one string, Czech).
 *
 * Stored in flash and sent as is. The page talks to the panel only through
 * the JSON API in web.cpp, so it can be edited without touching C++.
 * Field names: every input has data-k="<key>" matching Settings::toJson().
 */
#pragma once

#include <pgmspace.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="cs"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LED Matrix Panel</title>
<link rel="icon" href="data:,">
<style>
:root{--bg:#f4f5f7;--card:#fff;--tx:#1c1e21;--mut:#6b7280;--ln:#e3e5e8;--ac:#0a84ff;--ok:#1a9b4b;--bad:#d93025}
@media(prefers-color-scheme:dark){:root{--bg:#121316;--card:#1c1e22;--tx:#e8e9eb;--mut:#9aa0a6;--ln:#2c2f34;--ac:#4aa3ff}}
*{box-sizing:border-box}body{margin:0;font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--tx)}
header{padding:14px 16px 0;max-width:760px;margin:auto}h1{font-size:20px;margin:0 0 2px;font-weight:600}
#sub{color:var(--mut);font-size:13px}
nav{display:flex;gap:4px;overflow-x:auto;padding:12px 16px;max-width:760px;margin:auto;scrollbar-width:none}
nav button{flex:none;border:1px solid var(--ln);background:var(--card);color:var(--tx);padding:7px 12px;border-radius:999px;font:inherit;cursor:pointer}
nav button.on{background:var(--ac);border-color:var(--ac);color:#fff}
main{max-width:760px;margin:auto;padding:0 16px 40px}
section{display:none}section.on{display:block}
.card{background:var(--card);border:1px solid var(--ln);border-radius:12px;padding:14px 16px;margin-bottom:12px}
.card h2{font-size:15px;margin:0 0 10px;font-weight:600}
.row{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:7px 0;border-top:1px solid var(--ln)}
.row:first-of-type{border-top:0}.row label{flex:1}.row small{display:block;color:var(--mut);font-size:12px}
input[type=number],input[type=text],input[type=password],input[type=time],select{width:150px;padding:6px 8px;border:1px solid var(--ln);border-radius:8px;background:var(--bg);color:var(--tx);font:inherit}
input[type=range]{width:150px}input[type=color]{width:56px;height:32px;border:1px solid var(--ln);border-radius:8px;background:none;padding:2px}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--ac)}
.btn{border:0;background:var(--ac);color:#fff;padding:9px 16px;border-radius:9px;font:inherit;cursor:pointer}
.btn.sec{background:var(--ln);color:var(--tx)}.btn.bad{background:var(--bad)}
.btns{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 14px}.kv b{font-weight:500;color:var(--mut)}
.list div{padding:8px 0;border-top:1px solid var(--ln);cursor:pointer}.list div:first-child{border-top:0}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:#222;color:#fff;padding:9px 16px;border-radius:9px;opacity:0;transition:.3s;pointer-events:none}
#toast.on{opacity:1}.mut{color:var(--mut);font-size:13px}
.seg{display:flex;gap:6px;flex-wrap:wrap}.seg button{border:1px solid var(--ln);background:var(--bg);color:var(--tx);padding:6px 12px;border-radius:8px;cursor:pointer;font:inherit}
.seg button.on{background:var(--ac);color:#fff;border-color:var(--ac)}
progress{width:100%}
footer{max-width:760px;margin:0 auto;padding:8px 16px 28px;text-align:center;color:var(--mut);font-size:13px}footer a{color:var(--ac);text-decoration:none;font-weight:600}
</style></head><body>
<header><h1>LED Matrix Panel</h1><div id="sub">načítám…</div></header>
<nav id="nav"></nav>
<main>

<section id="s-stav">
 <div class="card"><h2>Právě teď</h2><div class="kv" id="st"></div></div>
 <div class="card"><h2>Obrazovka</h2>
  <div class="seg" id="modes"><button data-m="AUTO">Automaticky</button><button data-m="ROTATE">Střídání</button><button data-m="CLOCK">Hodiny</button><button data-m="PLANE">Letadlo</button><button data-m="RADAR">Radar</button><button data-m="FORECAST">Předpověď</button></div>
  <p class="mut">Automaticky: hodiny a občas předpověď, při dešti v okruhu radar, letadlo podle nastavení. Střídání: vybrané obrazovky dokola. Volba se pamatuje i po restartu. Nastavení je v záložce Displej.</p></div>
</section>

<section id="s-wifi">
 <div class="card"><h2>Připojení</h2><div class="kv" id="wst"></div></div>
 <div class="card"><h2>Připojit k síti</h2>
  <div class="row"><label>Název sítě (SSID)</label><input type="text" id="wssid" autocomplete="off"></div>
  <div class="row"><label>Heslo</label><input type="password" id="wpass"></div>
  <div class="btns"><button class="btn" onclick="wifiConnect()">Připojit</button><button class="btn sec" onclick="scan()">Hledat sítě</button></div>
  <div class="list" id="nets"></div></div>
 <div class="card"><h2>Zapomenout síť</h2><p class="mut">Panel zapomene domácí WiFi a znovu vytvoří vlastní síť pro nastavení.</p>
  <button class="btn bad" onclick="if(confirm('Opravdu zapomenout WiFi?'))post('/api/wifi/forget')">Zapomenout</button></div>
</section>

<section id="s-poloha">
 <div class="card"><h2>Kde panel je</h2>
  <div class="row"><label>Hledat obec<small>potřebuje internet v telefonu / počítači</small></label><input type="text" id="q" placeholder="např. Brno"></div>
  <div class="list" id="geo"></div>
  <div class="row"><label>Název místa</label><input type="text" data-k="place"></div>
  <div class="row"><label>Zeměpisná šířka</label><input type="number" step="0.0001" data-k="lat"></div>
  <div class="row"><label>Zeměpisná délka</label><input type="number" step="0.0001" data-k="lon"></div>
  <p class="mut">Poloha určuje okruh pro letadla i déšť a zelenou LED na mapě.</p></div>
</section>

<section id="s-letadla">
 <div class="card"><h2>Letadla (adsb.fi)</h2>
  <div class="row"><label>Zobrazovat letadla</label><input type="checkbox" data-k="planeOn"></div>
  <div class="row"><label>Okruh<small>km od polohy, 1–100</small></label><input type="number" min="1" max="100" data-k="planeRadius" oninput="viewDraw()"></div>
  <div class="row"><label>Minimální výška<small>m, ignoruje např. letiště</small></label><input type="number" min="0" max="15000" step="50" data-k="planeMinAlt"></div>
  <div class="row"><label>Jen letadla s volacím znakem</label><input type="checkbox" data-k="planeCallsign"></div>
  <div class="row"><label>Interval dotazů<small>s, 5–300</small></label><input type="number" min="5" max="300" data-k="planePeriod"></div>
  <div class="row"><label>Zobrazit aspoň<small>s, i když letadlo odletí</small></label><input type="number" min="0" max="120" data-k="planeMinShow"></div>
 </div>
 <div class="card"><h2>Směr výhledu</h2>
  <p class="mut">Letadlo ve směru, kam se díváš (např. z okna), se ukáže i za okruhem a má přednost před ostatními.</p>
  <div class="row"><label>Používat směr výhledu</label><input type="checkbox" data-k="viewOn" onchange="viewDraw()"></div>
  <div class="row"><label>Směr</label><select data-k="viewDir" data-type="int" onchange="viewDraw()"><option value="0">S – sever</option><option value="45">SV – severovýchod</option><option value="90">V – východ</option><option value="135">JV – jihovýchod</option><option value="180">J – jih</option><option value="225">JZ – jihozápad</option><option value="270">Z – západ</option><option value="315">SZ – severozápad</option></select></div>
  <div class="row"><label>Šířka výhledu<small>± stupňů od směru, 5–90</small></label><input type="number" min="5" max="90" data-k="viewHalf" oninput="viewDraw()"></div>
  <div class="row"><label>Dosah ve výhledu<small>km, 1–50, nezávisle na okruhu</small></label><input type="number" min="1" max="50" data-k="viewRange" oninput="viewDraw()"></div>
  <svg id="viewSvg" viewBox="-110 -110 220 220" style="width:220px;height:220px;display:block;margin:10px auto 0"></svg>
 </div>
</section>

<section id="s-radar">
 <div class="card"><h2>Srážkový radar (ČHMÚ)</h2>
  <div class="row"><label>Zobrazovat radar</label><input type="checkbox" data-k="radarOn"></div>
  <div class="row"><label>Okruh srážek<small>km od polohy, 5–500</small></label><input type="number" min="5" max="500" data-k="rainRadius"></div>
  <div class="row"><label>Práh intenzity<small>dBZ – 20 slabý déšť, 40 silný, 52+ bouřka</small></label><input type="number" min="4" max="60" step="4" data-k="rainDbz"></div>
  <div class="row"><label>Počet snímků<small>1–6, po 5 minutách</small></label><input type="number" min="1" max="6" data-k="radarFrames"></div>
  <div class="row"><label>Rychlost animace<small>ms na snímek</small></label><input type="number" min="100" max="5000" step="50" data-k="animStep"></div>
  <div class="row"><label>Podržet poslední snímek<small>ms</small></label><input type="number" min="0" max="10000" step="100" data-k="animHold"></div>
  <div class="row"><label>Legenda dBZ</label><input type="checkbox" data-k="legend"></div>
  <div class="row"><label>Města na mapě</label><input type="checkbox" data-k="cities"></div>
  <div class="row"><label>Pozadí mapy<small>tmavě zelená plocha republiky</small></label><input type="checkbox" data-k="mapBg"></div>
 </div>
</section>

<section id="s-hodiny">
 <div class="card"><h2>Hodiny</h2>
  <div class="row"><label>Typ</label><select data-k="clockAnalog" data-type="bool"><option value="true">ručičkové</option><option value="false">digitální</option></select></div>
  <div class="row"><label>Vteřiny</label><input type="checkbox" data-k="clockSeconds"></div>
  <div class="row"><label>Datum</label><input type="checkbox" data-k="clockDate"></div>
  <div class="row"><label>Ručičky</label><input type="color" data-k="colHands"></div>
  <div class="row"><label>Vteřiny</label><input type="color" data-k="colSecond"></div>
  <div class="row"><label>Číslice (digitální)</label><input type="color" data-k="colDigits"></div>
  <div class="row"><label>Ciferník<small>značky hodin, minutové jsou tmavší</small></label><input type="color" data-k="colDial"></div>
  <div class="row"><label>Den v týdnu a měsíc<small>u digitálních celý řádek s datem</small></label><input type="color" data-k="colDateLabel"></div>
  <div class="row"><label>Den a rok</label><input type="color" data-k="colDateValue"></div>
 </div>
 <div class="card"><h2>Předpověď počasí</h2>
  <div class="row"><label>Zobrazovat předpověď<small>teď · dnes · zítra, data Open-Meteo</small></label><input type="checkbox" data-k="fcOn"></div>
  <div class="row"><label>Hodiny mezi předpověďmi<small>sekundy</small></label><input type="number" min="5" max="3600" data-k="fcEvery"></div>
  <div class="row"><label>Předpověď zobrazit na<small>sekundy</small></label><input type="number" min="3" max="600" data-k="fcShow"></div>
 </div>
</section>

<section id="s-displej">
 <div class="card"><h2>Automatický režim</h2>
  <div class="row"><label>Letadla<small>když je letadlo v okruhu nebo ve výhledu</small></label><select data-k="autoPlane" data-type="int"><option value="0">ukázat ihned</option><option value="1">ukazovat občas</option><option value="2">nikdy</option></select></div>
  <div class="row"><label>Občas: zobrazit na<small>sekundy</small></label><input type="number" min="3" max="600" data-k="autoPlaneShow"></div>
  <div class="row"><label>Občas: pauza mezi<small>sekundy</small></label><input type="number" min="5" max="3600" data-k="autoPlaneEvery"></div>
  <p class="mut">Jinak hodiny a předpověď (nastavení v záložce Hodiny a počasí), při dešti v okruhu radar.</p>
 </div>
 <div class="card"><h2>Střídání obrazovek</h2>
  <div class="row"><label>Hodiny</label><input type="checkbox" data-k="rotClock"></div>
  <div class="row"><label>Předpověď</label><input type="checkbox" data-k="rotForecast"></div>
  <div class="row"><label>Radar</label><input type="checkbox" data-k="rotRadar"></div>
  <div class="row"><label>Radar i bez deště<small>jinak se bez srážek v okruhu přeskočí</small></label><input type="checkbox" data-k="rotRadarDry"></div>
  <div class="row"><label>Letadlo<small>přeskočí se, když žádné neletí</small></label><input type="checkbox" data-k="rotPlane"></div>
  <div class="row"><label>Interval<small>sekundy na obrazovku</small></label><input type="number" min="3" max="3600" data-k="rotS"></div>
 </div>
 <div class="card"><h2>Jas</h2>
  <div class="row"><label>Ve dne <small id="bdv"></small></label><input type="range" min="1" max="255" data-k="brightDay" oninput="bdv.textContent=this.value"></div>
  <div class="row"><label>V noci <small id="bnv"></small></label><input type="range" min="1" max="255" data-k="brightNight" oninput="bnv.textContent=this.value"></div>
 </div>
 <div class="card"><h2>Noční režim</h2>
  <div class="row"><label>Kdy je noc</label><select data-k="nightMode" data-type="int"><option value="0">nikdy</option><option value="1">podle času</option><option value="2">od západu do východu slunce</option></select></div>
  <div class="row"><label>Od</label><input type="time" data-k="nightStart"></div>
  <div class="row"><label>Do</label><input type="time" data-k="nightEnd"></div>
  <div class="row"><label>V noci</label><select data-k="nightAction" data-type="int"><option value="0">ztlumit</option><option value="1">vypnout displej</option></select></div>
  <div class="row"><label>V noci jen hodiny<small>bez letadel a radaru</small></label><input type="checkbox" data-k="nightClockOnly"></div>
 </div>
</section>

<section id="s-system">
 <div class="card"><h2>Zařízení</h2>
  <div class="row"><label>Název v síti<small>adresa <span id="hn"></span>.local, po restartu</small></label><input type="text" maxlength="31" data-k="hostname"></div>
  <div class="row"><label>Časové pásmo</label><select id="tzsel"></select></div>
  <div class="row"><label>Vlastní (POSIX TZ)</label><input type="text" data-k="tz"></div>
 </div>
 <div class="card"><h2>Záloha nastavení</h2>
  <div class="btns"><a class="btn sec" href="/api/export" download="led-matrix-nastaveni.json" style="text-decoration:none">Stáhnout zálohu</a>
  <label class="btn sec">Obnovit ze souboru<input type="file" accept=".json" hidden onchange="importCfg(this)"></label></div></div>
 <div class="card"><h2>Aktualizace firmwaru</h2><p class="mut">Soubor .bin z Arduino IDE (Sketch → Export Compiled Binary).</p>
  <input type="file" id="fw" accept=".bin"><progress id="fwp" value="0" max="100" hidden></progress>
  <div class="btns"><button class="btn" onclick="ota()">Nahrát</button></div></div>
 <div class="card"><h2>Restart a obnovení</h2>
  <div class="btns"><button class="btn sec" onclick="post('/api/reboot')">Restartovat</button>
  <button class="btn bad" onclick="if(confirm('Smazat všechna nastavení včetně WiFi?'))post('/api/reset')">Tovární nastavení</button></div></div>
</section>

<div class="btns" id="savebar" style="display:none"><button class="btn" onclick="save()">Uložit nastavení</button></div>
</main>
<footer>Vymyslel a vytvořil <a href="https://chiptron.cz" target="_blank" rel="noopener">chiptron.cz</a></footer>
<div id="toast"></div>

<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const TABS=[['stav','Stav'],['wifi','WiFi'],['poloha','Poloha'],['letadla','Letadla'],['radar','Radar'],['hodiny','Hodiny a počasí'],['displej','Displej'],['system','Systém']];
const TZ=[['CET-1CEST,M3.5.0,M10.5.0/3','Střední Evropa (Praha)'],['GMT0BST,M3.5.0/1,M10.5.0','Londýn'],['EET-2EEST,M3.5.0/3,M10.5.0/4','Východní Evropa'],['UTC0','UTC'],['','vlastní…']];
let cfg={};
const SCR={'clock':'hodiny','aircraft':'letadlo','radar':'radar','setup AP':'nastavení WiFi','connecting':'připojování','IP address':'IP adresa','off (night)':'vypnuto (noc)','forecast':'předpověď'};
function toast(t,ms){const e=$('#toast');e.textContent=t;e.classList.add('on');clearTimeout(e.t);e.t=setTimeout(()=>e.classList.remove('on'),ms||2200)}
async function api(u,o){const r=await fetch(u,o);if(!r.ok)throw new Error(await r.text()||r.status);const t=await r.text();try{return JSON.parse(t)}catch(e){return t}}
async function post(u,b){try{await api(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})});toast('Hotovo')}catch(e){toast('Chyba: '+e.message)}}
function tab(id){$$('section').forEach(s=>s.classList.toggle('on',s.id=='s-'+id));$$('nav button').forEach(b=>b.classList.toggle('on',b.dataset.t==id));
 $('#savebar').style.display=['stav','wifi'].includes(id)?'none':'flex';location.hash=id}
$('#nav').innerHTML=TABS.map(([i,t])=>`<button data-t="${i}">${t}</button>`).join('');
$$('nav button').forEach(b=>b.onclick=()=>tab(b.dataset.t));
function fill(){$$('[data-k]').forEach(e=>{const v=cfg[e.dataset.k];if(v===undefined)return;
 if(e.type=='checkbox')e.checked=v;else e.value=String(v)});
 bdv.textContent=cfg.brightDay;bnv.textContent=cfg.brightNight;$('#hn').textContent=cfg.hostname;
 const t=$('#tzsel');t.innerHTML=TZ.map(([v,n])=>`<option value="${v}">${n}</option>`).join('');
 t.value=TZ.some(z=>z[0]==cfg.tz)?cfg.tz:'';t.onchange=()=>{if(t.value)$('[data-k=tz]').value=t.value}}
function collect(){const o={};$$('[data-k]').forEach(e=>{const k=e.dataset.k;let v;
 if(e.type=='checkbox')v=e.checked;else if(e.dataset.type=='bool')v=e.value=='true';else if(e.dataset.type=='int'||e.type=='range')v=parseInt(e.value);
 else if(e.type=='number')v=e.step&&e.step.includes('.')?parseFloat(e.value):parseInt(e.value);else v=e.value;
 if(typeof v=='number'&&isNaN(v))return;o[k]=v});return o}
// Top view of the radius (circle) and the viewing sector, north up.
function viewDraw(){const q=k=>$(`[data-k=${k}]`),R=90;
 const on=q('viewOn').checked,dir=+q('viewDir').value||0,half=+q('viewHalf').value||30,rng=+q('viewRange').value||5,rad=+q('planeRadius').value||10;
 const m=Math.max(rng,rad),rr=rad/m*R,vr=rng/m*R;
 // bearing b (0 = north, clockwise) -> SVG point (y grows downwards)
 const pt=(b,r)=>[(r*Math.sin(b*Math.PI/180)).toFixed(1),(-r*Math.cos(b*Math.PI/180)).toFixed(1)];
 const [x1,y1]=pt(dir-half,vr),[x2,y2]=pt(dir+half,vr),large=2*half>180?1:0;
 let s=`<circle r="${rr}" fill="none" stroke="currentColor" stroke-opacity=".4"/>`;
 if(on)s+=`<path d="M0 0L${x1} ${y1}A${vr} ${vr} 0 ${large} 1 ${x2} ${y2}Z" fill="#3b82f6" fill-opacity=".35" stroke="#3b82f6"/>`;
 s+=`<circle r="3" fill="#22c55e"/><text y="-96" text-anchor="middle" font-size="14" fill="currentColor">S</text><text x="100" y="5" text-anchor="middle" font-size="14" fill="currentColor">V</text><text y="108" text-anchor="middle" font-size="14" fill="currentColor">J</text><text x="-100" y="5" text-anchor="middle" font-size="14" fill="currentColor">Z</text>`;
 s+=`<text x="${(rr*0.72).toFixed(0)}" y="${(rr*0.72+12).toFixed(0)}" font-size="11" fill="currentColor" fill-opacity=".6">${rad} km</text>`;
 if(on)s+=`<text x="${pt(dir,vr+12)[0]}" y="${+pt(dir,vr+12)[1]+4}" text-anchor="middle" font-size="11" fill="#3b82f6">${rng} km</text>`;
 $('#viewSvg').innerHTML=s}
async function load(){cfg=await api('/api/config');fill();viewDraw()}
async function save(){try{cfg=await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(collect())});fill();viewDraw();toast('Uloženo')}catch(e){toast('Chyba: '+e.message)}}
const kv=(el,rows)=>el.innerHTML=rows.map(([k,v])=>`<b>${k}</b><span>${v}</span>`).join('');
async function status(){try{const s=await api('/api/status');
 $('#sub').textContent=`${s.time||'--:--'} · ${s.wifi.connected?s.wifi.ip:'bez WiFi'} · ${s.fw}`;
 const p=s.plane,r=s.radar;
 kv($('#st'),[['Čas',s.time?`${s.time} ${s.date}`:'zatím nenastaven'],['Obrazovka',(SCR[s.screen]||s.screen)+(s.night?' · noc':'')],
  ['Letadlo',p.nearby?`${p.callsign||p.reg||p.hex} ${p.type} · ${p.distKm.toFixed(1)} km · ${p.altM} m · ${p.speedKmh} km/h${p.count>1?` · +${p.count-1} další`:''}`:(p.on?'žádné v okruhu':'vypnuto')],
  ['Radar',r.on?(r.frames?`${r.frames} snímků, poslední ${r.newest}`:'stahuji…'):'vypnuto'],
  ['Počasí',s.forecast.on?(s.forecast.temp!==undefined?`teď ${s.forecast.temp} °C · dnes ${s.forecast.todayMax}/${s.forecast.todayMin} · zítra ${s.forecast.tomorrowMax}/${s.forecast.tomorrowMin} °C`:'stahuji…'):'vypnuto'],
  ['Déšť v okruhu',r.nearby?`ano – ${r.nearestKm} km, až ${r.maxDbz} dBZ`:'ne'],
  ['Paměť',`${Math.round(s.heap/1024)} kB volné, největší blok ${Math.round(s.heapMax/1024)} kB`],['Běží',fmtUp(s.uptime)]]);
 const w=s.wifi;kv($('#wst'),[['Stav',w.connected?'připojeno':(w.ap?'vlastní síť '+w.apSsid:'připojuji…')],['Síť',w.ssid||'–'],['Adresa',w.connected?`${w.ip} (${cfg.hostname||''}.local)`:'–'],['Signál',w.connected?w.rssi+' dBm':'–']]);
 $$('#modes button').forEach(b=>b.classList.toggle('on',b.dataset.m==s.mode))}catch(e){$('#sub').textContent='panel neodpovídá'}}
function fmtUp(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+' d ':'')+h+' h '+m+' min'}
$$('#modes button').forEach(b=>b.onclick=async()=>{await post('/api/mode',{mode:b.dataset.m});status()});
async function scan(){$('#nets').innerHTML='<div class="mut">hledám…</div>';for(let i=0;i<20;i++){const r=await api('/api/scan');
 if(r.done){$('#nets').innerHTML=r.networks.sort((a,b)=>b.rssi-a.rssi).map(n=>`<div data-s="${encodeURIComponent(n.ssid)}">${n.open?'🔓':'🔒'} ${n.ssid.replace(/</g,'&lt;')} <span class="mut">${n.rssi} dBm</span></div>`).join('')||'<div class="mut">nic nenalezeno</div>';
  $$('#nets div[data-s]').forEach(d=>d.onclick=()=>{$('#wssid').value=decodeURIComponent(d.dataset.s);$('#wpass').focus()});return}
 await new Promise(r=>setTimeout(r,800))}$('#nets').innerHTML='<div class="mut">hledání nedoběhlo</div>'}
let wifiBusy=0;async function wifiConnect(){if(wifiBusy)return;const ssid=$('#wssid').value.trim();if(!ssid){toast('Zadej název sítě');return}wifiBusy=1;setTimeout(()=>wifiBusy=0,8000);
 await post('/api/wifi',{ssid,pass:$('#wpass').value});toast('Panel se přepíná na domácí WiFi a ukáže novou adresu. Tato síť se teď odpojí.',8000)}
let gt;$('#q').oninput=()=>{clearTimeout(gt);gt=setTimeout(geo,400)};
async function geo(){const q=$('#q').value.trim();if(q.length<2){$('#geo').innerHTML='';return}
 try{const r=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=6&language=cs&name='+encodeURIComponent(q))).json();
  $('#geo').innerHTML=(r.results||[]).map(p=>`<div data-lat="${p.latitude}" data-lon="${p.longitude}" data-n="${p.name}">${p.name} <span class="mut">${[p.admin1,p.country].filter(Boolean).join(', ')}</span></div>`).join('')||'<div class="mut">nic</div>';
  $$('#geo div[data-lat]').forEach(d=>d.onclick=()=>{$('[data-k=lat]').value=(+d.dataset.lat).toFixed(4);$('[data-k=lon]').value=(+d.dataset.lon).toFixed(4);$('[data-k=place]').value=d.dataset.n;$('#geo').innerHTML='';toast('Vybráno – nezapomeň Uložit')})}
 catch(e){$('#geo').innerHTML='<div class="mut">hledání potřebuje internet</div>'}}
async function importCfg(inp){const f=inp.files[0];if(!f)return;try{cfg=await api('/api/import',{method:'POST',headers:{'Content-Type':'application/json'},body:await f.text()});fill();toast('Obnoveno')}catch(e){toast('Chyba: '+e.message)}inp.value=''}
function ota(){const f=$('#fw').files[0];if(!f){toast('Vyber soubor .bin');return}const x=new XMLHttpRequest(),p=$('#fwp');p.hidden=false;
 x.upload.onprogress=e=>p.value=e.loaded/e.total*100;x.onload=()=>toast(x.status==200?'Nahráno, restartuji…':'Chyba: '+x.responseText);
 const d=new FormData();d.append('firmware',f);x.open('POST','/update');x.send(d)}
tab(location.hash.slice(1)||'stav');load().then(status);setInterval(status,2000);
</script></body></html>)HTML";
