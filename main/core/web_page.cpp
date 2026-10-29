// The phone page served by the radar (see app/web.cpp): one HTML file with
// its style and script, talking to the /api/... endpoints.

#include "core/web.h"

namespace core::web {

const char kPage[] = R"PAGE(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#06140c">
<title>Plane Radar</title>
<style>
:root{--bg:#06140c;--panel:#0d2417;--line:#1d4a30;--text:#d8f5e3;--dim:#7fa893;--acc:#3ddc84;--warn:#ffb84d;--bad:#ff6b6b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 -apple-system,system-ui,Segoe UI,Roboto,sans-serif}
header{position:sticky;top:0;z-index:5;background:var(--bg);border-bottom:1px solid var(--line)}
h1{font-size:18px;margin:0;padding:12px 16px 6px;display:flex;align-items:center;gap:8px}
h1 .dot{width:10px;height:10px;border-radius:50%;background:var(--acc);box-shadow:0 0 8px var(--acc)}
h1 small{color:var(--dim);font-weight:400;font-size:13px;margin-left:auto}
nav{display:flex;overflow-x:auto;gap:4px;padding:0 10px 8px;scrollbar-width:none}
nav button{flex:none;background:none;border:1px solid var(--line);color:var(--dim);border-radius:18px;padding:6px 13px;font-size:14px}
nav button.on{background:var(--acc);color:#04210f;border-color:var(--acc);font-weight:600}
main{padding:12px 14px 80px;max-width:640px;margin:0 auto}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:12px 14px;margin:0 0 12px}
details.card{padding:0}
details summary{padding:13px 14px;font-weight:600;cursor:pointer;list-style:none;display:flex;justify-content:space-between}
details summary::after{content:"\25B8";color:var(--dim)}
details[open] summary::after{content:"\25BE"}
details .rows{padding:0 14px 6px}
.row{display:flex;align-items:center;gap:10px;padding:9px 0;border-top:1px solid var(--line)}
.row:first-child{border-top:none}
.row .lbl{flex:1;min-width:36%}
.row .val{text-align:right}
.conv{color:var(--dim);font-size:13px;white-space:nowrap}
.row .lbl b{display:block;font-weight:500;word-wrap:break-word}
.row .lbl span{display:block;color:var(--dim);font-size:13px}
input,select{background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:9px;padding:8px 10px;font-size:16px;max-width:100%}
input[type=number]{width:100px}input[type=time]{width:120px}
input.wide{width:100%}
select{max-width:190px}
.sw{position:relative;width:50px;height:30px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw i{position:absolute;inset:0;background:var(--line);border-radius:15px;transition:.2s}
.sw i::before{content:"";position:absolute;width:24px;height:24px;left:3px;top:3px;background:var(--text);border-radius:50%;transition:.2s}
.sw input:checked+i{background:var(--acc)}
.sw input:checked+i::before{transform:translateX(20px);background:#04210f}
.cats{display:flex;flex-wrap:wrap;gap:6px 12px;margin-top:6px}
.cats label{font-size:14px;color:var(--dim)}
button{font:inherit;font-size:15px;border-radius:10px;padding:8px 12px;cursor:pointer}
button.btn{background:var(--acc);color:#04210f;border:none;border-radius:10px;padding:9px 14px;font-size:15px;font-weight:600}
button.sec{background:none;color:var(--acc);border:1px solid var(--line)}
button.bad{background:none;color:var(--bad);border:1px solid var(--line)}
button:disabled{opacity:.5}
.muted{color:var(--dim);font-size:14px}
.flex{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px}
.grid a{display:block;color:var(--dim);font-size:12px;text-decoration:none;text-align:center}
.grid img{width:100%;aspect-ratio:1;object-fit:cover;border-radius:50%;border:1px solid var(--line);background:#000}
.bar{height:10px;background:var(--line);border-radius:5px;overflow:hidden;margin:10px 0}
.bar div{height:100%;width:0;background:var(--acc);transition:width .2s}
#toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%);background:var(--panel);border:1px solid var(--acc);color:var(--text);padding:10px 16px;border-radius:20px;opacity:0;transition:opacity .25s;pointer-events:none;max-width:90%;text-align:center;z-index:9}
#toast.show{opacity:1}
.hidden{display:none!important}
.sig{color:var(--dim);font-size:13px;white-space:nowrap}
</style></head><body>
<header><h1><span class="dot"></span>Plane Radar<small id="ver"></small></h1>
<nav id="tabs"></nav></header>
<main>
<div id="view"></div>
</main>
<div id="toast"></div>
<script>
const $=s=>document.querySelector(s);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let toastT;
function toast(t,ms){const e=$('#toast');e.textContent=t;e.classList.add('show');clearTimeout(toastT);toastT=setTimeout(()=>e.classList.remove('show'),ms||2200)}
async function api(path,body){
  const opt=body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)};
  const r=await fetch(path,opt);
  const j=await r.json().catch(()=>({ok:false,error:'Bad answer from the radar'}));
  if(!r.ok&&!j.error)j.error='Error '+r.status;
  return j;
}
const TABS=[['settings','Settings'],['watch','Watchlist'],['favorites','Favorites'],['wifi','Wi-Fi'],['log','Daily log'],['shots','Screenshots'],['update','Update']];
let tab='settings',status={};
function nav(){
  $('#tabs').innerHTML=TABS.map(([k,n])=>`<button class="${k==tab?'on':''}" onclick="go('${k}')">${n}</button>`).join('');
}
function go(k){tab=k;nav();try{localStorage.tab=k}catch(e){}render()}
async function start(){
  try{status=await api('/api/status')}catch(e){$('#view').innerHTML='<div class="card">Can\'t reach the radar. Is this phone on the same Wi-Fi?</div>';return}
  $('#ver').textContent=status.version+' · '+status.aircraft+' aircraft';
  try{if(localStorage.tab)tab=localStorage.tab}catch(e){}
  nav();render();
}
function render(){({settings:viewSettings,watch:viewWatch,favorites:viewFavorites,wifi:viewWifi,log:viewLog,shots:viewShots,update:viewUpdate})[tab]()}
const LABELS={home_lat:'Home latitude',home_lon:'Home longitude',home_radius_km:'Home zoom radius (km)',home_set:'Home location set',
  show_trails:'Trails',extended_trail:'Flight history on tap',show_speed_vectors:'Speed vectors',show_rings:'Range rings',show_runways:'Runways',
  show_airport_labels:'Airport codes',airport_codes:'Airport code style',show_route_info:'Airline and route on tap',autohide_buttons:'Auto-hide buttons',
  dim_after_min:'Dim after (minutes)',dim_brightness:'Dimmed brightness (%)',night_dim:'Night mode',night_sleep:'Sleep at night',night_by_sun:'Night is sunset to sunrise',
  night_from_min:'Night starts',night_to_min:'Night ends',night_brightness:'Night brightness (%)',show_stats:'On-screen stats',brightness:'Brightness (%)',
  aircraft_scale:'Aircraft size (%)',sweep_enabled:'Radar sweep',sweep_rpm:'Sweep speed (rpm)',sweep_trail_deg:'Sweep trail (degrees)',sweep_smooth:'Smooth trail fade',
  sweep_reveal:'Echoes fade after the beam',map_style:'Background map',map_labels:'Place names',map_tint:'Tint map to theme color',map_brightness:'Map brightness (%)',
  sd_cache:'Save map tiles on the card',lookup_cache:'Remember lookups on the card',carto_key:'CARTO API key',sound_enabled:'Sound',volume:'Volume (%)',
  sound_touch:'Touch sounds',sound_new_aircraft:'New aircraft',sound_sweep_ping:'Sonar ping each sweep',sound_alerts:'Emergency squawks',
  sound_proximity:'Proximity alert',alert_radius_km:'Alert radius (km)',alert_max_alt_ft:'Alert below (ft)',sound_favorite_seen:'Favorite detected',
  sound_favorite_near:'Favorite nearby',favorite_near_radius_km:'Favorite nearby radius (km)',watch_enabled:'Watchlist alerts',watch_military:'Military aircraft',
  watch_rotor:'Helicopters',watch_fast:'Fast jets',watch_types:'Types',watch_callsigns:'Callsigns and tail numbers',watch_radius_km:'Within (km of home)',
  sound_watch:'Play a sound',show_ground:'Aircraft on the ground',military_only:'Military only',min_alt_ft:'Hide below (ft)',max_alt_ft:'Hide above (ft)',
  min_speed_kt:'Hide slower than (kt)',max_speed_kt:'Hide faster than (kt)',categories:'Aircraft kinds',type_filter:'Only these types',source:'ADS-B feed',
  poll_interval_s:'Update every (s)',log_radius_km:'Count aircraft within (km)',web_enabled:'Phone settings page',auto_update_check:'Check for updates automatically',
  clock_mode:'Clock face',clock_24h:'24-hour clock',clock_seconds:'Seconds hand',clock_smooth_seconds:'Smooth seconds',clock_show_date:'Date',
  time_zone:'Time zone',wake_on_alarm:'Alarms wake the sleeping screen',label_size:'Label size',color_mode:'Aircraft colors',labels:'Labels'};
const UNIT={km:1,mi:1.609344,nm:1.852};
function conv(f){
  if(!/_km$/.test(f.key)||!status.units||status.units=='km')return '';
  const v=parseFloat(f.value)/UNIT[status.units];return isNaN(v)?'':`<span class="conv" id="c_${f.key}">= ${+v.toFixed(1)} ${status.units}</span>`;
}
function convUpd(key,val){const e=document.getElementById('c_'+key);if(e)e.textContent='= '+(+(parseFloat(val)/UNIT[status.units]).toFixed(1))+' '+status.units}
const nice=k=>{if(LABELS[k])return LABELS[k];const s=k.replace(/_/g,' ').replace(/\bkm\b/,'(km)').replace(/\bft\b/,'(ft)').replace(/\bkt\b/,'(kt)').replace(/\bmin\b$/,'').replace(/\bs$/,'(s)');return s.charAt(0).toUpperCase()+s.slice(1)};
async function save(key,value){
  const j=await api('/api/settings',{text:key+' = '+value});
  if(j.problems&&j.problems.length)toast(j.problems[0].replace(/^line \d+: /,''),3500);else toast('Saved');
}
function field(f){
  const id='f_'+f.key;let ctl='';
  if(f.help&&f.help.toLowerCase()==nice(f.key).toLowerCase())f.help='';
  const v=esc(f.value);
  switch(f.type){
    case 'bool':ctl=`<label class="sw"><input type="checkbox" ${f.value=='on'?'checked':''} onchange="save('${f.key}',this.checked?'on':'off')"><i></i></label>`;break;
    case 'choice':ctl=`<select onchange="save('${f.key}',this.value)">`+f.options.map(o=>`<option ${o==f.value?'selected':''}>${esc(o)}</option>`).join('')+'</select>';break;
    case 'zone':ctl=`<select onchange="save('${f.key}',this.value)">`+f.options.map((o,i)=>`<option value="${i}" ${i==f.value?'selected':''}>${esc(o)}</option>`).join('')+'</select>';break;
    case 'int':case 'number':{
      const shown=/_km$/.test(f.key)?+parseFloat(f.value).toFixed(2):f.value;
      ctl=`<div class="val"><input type="number" value="${shown}" min="${f.min}" max="${f.max}" step="${f.type=='int'?1:'any'}" oninput="convUpd('${f.key}',this.value)" onchange="save('${f.key}',this.value)"><br>${conv(f)}</div>`;break;}
    case 'time':ctl=`<input type="time" value="${v}" onchange="save('${f.key}',this.value)">`;break;
    case 'text':return `<div class="row"><div class="lbl"><b>${nice(f.key)}</b>${f.help?`<span>${esc(f.help)}</span>`:''}<input class="wide" value="${v}" maxlength="${f.maxlen}" onchange="save('${f.key}','&quot;'+this.value+'&quot;')"></div></div>`;
    case 'alarm':return `<div class="row"><div class="lbl"><b>${nice(f.key)}</b>${f.help?`<span>${esc(f.help)}</span>`:''}<input class="wide" value="${v}" onchange="save('${f.key}',this.value)"></div></div>`;
    case 'categories':{
      const on=f.value=='all'?f.options:f.value.split(/[ ,]+/);
      return `<div class="row"><div class="lbl"><b>${nice(f.key)}</b><span>${esc(f.help)}</span><div class="cats" id="${id}">`+
        f.options.map(o=>`<label><input type="checkbox" value="${o}" ${on.includes(o)?'checked':''} onchange="cats('${f.key}')"> ${o}</label>`).join('')+'</div></div></div>';
    }
  }
  return `<div class="row"><div class="lbl"><b>${nice(f.key)}</b>${f.help?`<span>${esc(f.help)}</span>`:''}</div>${ctl}</div>`;
}
function cats(key){const b=[...document.querySelectorAll('#f_'+key+' input:checked')].map(i=>i.value);save(key,b.length==8?'all':(b.length?b.join(', '):'none'))}
async function viewSettings(only){
  const j=await api('/api/settings');const v=$('#view');
  const secs=j.sections.filter(s=>only?s.title==only:s.title!='Watchlist');
  v.innerHTML=(only?'':'<p class="muted">Changes apply on the radar right away. Distances are set in km here.</p>')+secs.map((s,i)=>
    only?`<div class="card">${s.fields.map(field).join('')}</div>`:
    `<details class="card" ${i==0?'open':''}><summary>${esc(s.title)}</summary><div class="rows">${s.fields.map(field).join('')}</div></details>`).join('');
}
async function viewWatch(){
  await viewSettings('Watchlist');
  $('#view').insertAdjacentHTML('afterbegin','<p class="muted">The radar announces these aircraft (and shows them, whatever the filters) when they come within the distance below of home. Types are ICAO codes like A388, B52, C17, F35; * matches a prefix (A38* = any A380). Callsigns and registrations work the same way: RCH* catches US Air Force cargo flights.</p>');
}
async function viewFavorites(){
  const j=await api('/api/favorites');
  $('#view').innerHTML=`<div class="card"><b>Add a favorite</b><p class="muted">Tail number (N123AB) or ICAO hex code. The radar looks it up online.</p>
  <div class="flex"><input id="favq" placeholder="N123AB" autocapitalize="characters" style="flex:1"><button class="btn" onclick="favAdd()">Add</button></div></div>
  <div class="card"><b>${j.list.length} of ${j.max} saved</b>`+
  (j.list.length?j.list.map(f=>`<div class="row"><div class="lbl"><input value="${esc(f.label)}" maxlength="11" style="width:130px" onchange="favRename('${f.hex}',this.value)"><span>${esc(f.type||'?')} · ${f.hex}${f.in_range?' · in range':''}</span></div><button class="bad" data-n="${esc(f.label)}" onclick="favDel('${f.hex}',this.dataset.n)">Remove</button></div>`).join(''):'<p class="muted">None yet.</p>')+
  (j.unresolved.length?`<p class="muted">Waiting to be looked up: ${j.unresolved.map(esc).join(', ')}</p>`:'')+'</div>';
}
async function favAdd(){const q=$('#favq').value.trim();if(!q)return;const j=await api('/api/favorites/add',{q});toast(j.ok?'Looking up '+q+'...':j.error);setTimeout(viewFavorites,4000)}
async function favDel(hex,name){if(!confirm('Remove '+name+'?'))return;await api('/api/favorites/remove',{hex});viewFavorites()}
async function favRename(hex,label){const j=await api('/api/favorites/rename',{hex,label});toast(j.ok?'Saved':j.error)}
let scanTimer;
async function viewWifi(){
  const j=await api('/api/wifi');
  const c=j.current;
  $('#view').innerHTML=`<div class="card"><b>Now</b><p>${c.state=='connected'?`Connected to <b>${esc(c.ssid)}</b>, ${esc(c.ip)}, ${c.rssi} dBm`:esc(c.state)}</p></div>
  <div class="card"><b>Saved networks</b><p class="muted">Tried in this order when the current one can't connect. Up to ${j.max}.</p>`+
  j.saved.map((s,i)=>`<div class="row" data-s="${esc(s)}"><div class="lbl"><b>${esc(s)}</b>${i==0?'<span>used first</span>':''}</div>
    ${i>0?`<button class="sec" onclick="wifiUp(this.parentNode.dataset.s,${i==1})">▲</button><button class="sec" onclick="wifiUse(this.parentNode.dataset.s)">Use now</button>`:''}
    <button class="bad" onclick="wifiForget(this.parentNode.dataset.s,${i==0})">Forget</button></div>`).join('')+`</div>
  <div class="card"><b>Add a network</b><p class="muted">Saved for later: the radar stays on the current network and tries this one when that fails.</p>
  <div class="flex"><input id="ssid" list="nets" placeholder="Network name" style="flex:1"><button class="sec" onclick="scan()">Scan</button></div>
  <datalist id="nets"></datalist><div id="scanres" class="muted"></div>
  <div class="flex" style="margin-top:8px"><input id="wpass" type="password" placeholder="Password" style="flex:1"></div>
  <div class="flex" style="margin-top:8px"><button class="btn" onclick="wifiAdd(false)">Save</button><button class="sec" onclick="wifiAdd(true)">Save and connect now</button></div></div>`;
}
async function scan(){
  await api('/api/wifi/scan',{});$('#scanres').textContent='Scanning...';
  clearInterval(scanTimer);scanTimer=setInterval(async()=>{
    const j=await api('/api/wifi/scan');if(!j.done)return;clearInterval(scanTimer);
    $('#nets').innerHTML=j.networks.map(n=>`<option value="${esc(n.ssid)}">`).join('');
    $('#scanres').innerHTML=j.networks.map(n=>`<a href="#" onclick="$('#ssid').value=this.dataset.s;return false" data-s="${esc(n.ssid)}">${esc(n.ssid)}</a> <span class="sig">${n.rssi} dBm${n.secure?'':' open'}</span>`).join(' · ')||'Nothing found';
  },1500);
}
async function wifiAdd(now){
  const ssid=$('#ssid').value;if(!ssid)return;
  if(now&&!confirm('Switch the radar to '+ssid+' now? If that network is on another router, this page stops answering here.'))return;
  const j=await api('/api/wifi/add',{ssid,pass:$('#wpass').value,now:now?1:0});toast(j.ok?(now?'Connecting...':'Saved'):j.error);viewWifi();
}
async function wifiUse(s){if(!confirm('Switch the radar to '+s+' now?'))return;await api('/api/wifi/use',{ssid:s});toast('Connecting to '+s+'...')}
async function wifiForget(s,first){if(!confirm('Forget '+s+'?'+(first?' The radar is using it: it moves on to the next saved network.':'')))return;await api('/api/wifi/forget',{ssid:s});viewWifi()}
async function wifiUp(s,first){if(first&&!confirm(s+' becomes the network used first, and the radar switches to it now.'))return;await api('/api/wifi/up',{ssid:s});viewWifi()}
async function viewLog(){
  const j=await api('/api/daylog');
  $('#view').innerHTML=`<div class="card"><b>Today</b>${j.today.map(([k,v])=>`<div class="row"><div class="lbl"><span>${esc(k)}</span></div><div class="val">${esc(v)}</div></div>`).join('')}</div>
  <div class="card"><b>Earlier days</b>${j.days.length?j.days.map(d=>`<p>${esc(d)}</p>`).join(''):'<p class="muted">'+(j.sd?'Nothing saved yet.':'With an SD card, every day is kept.')+'</p>'}</div>`;
}
async function viewShots(){
  const j=await api('/api/shots');
  $('#view').innerHTML=`<div class="card flex"><button class="btn" onclick="shoot()" ${j.sd?'':'disabled'}>Take a screenshot now</button><span class="muted">${j.sd?'Or tap the camera button on the radar.':'Needs an SD card in the radar.'}</span></div>
  <div class="grid">${j.files.map(f=>`<a href="/shots/${encodeURIComponent(f)}" target="_blank"><img loading="lazy" src="/shots/${encodeURIComponent(f)}">${esc(f.replace('.png',''))}</a>`).join('')}</div>`+(j.files.length?'':'<p class="muted">No screenshots yet.</p>');
}
async function shoot(){const j=await api('/api/screenshot',{});toast(j.ok?'Saving...':j.error);setTimeout(viewShots,3000)}
function viewUpdate(){
  $('#view').innerHTML=`<div class="card"><b>Installed</b><p>${esc(status.version)} for the ${esc(status.board)} board</p></div>
  <div class="card"><b>Install a firmware file</b><p class="muted">Pick the file that ends in <b>-ota.bin</b> for this board (<b>${esc(status.board)}</b>). Your settings and favorites are kept. The radar checks the file and refuses one for another board.</p>
  <input type="file" id="fw" accept=".bin"><div class="bar"><div id="fwbar"></div></div><p id="fwmsg" class="muted"></p>
  <div class="flex"><button class="btn" id="fwgo" onclick="upload()">Install</button><button class="sec hidden" id="fwrestart" onclick="restart()">Restart now</button></div></div>
  <p class="muted">The radar's own update button installs new releases from GitHub too.</p>`;
}
function upload(){
  const f=$('#fw').files[0];if(!f){toast('Choose a file first');return}
  const x=new XMLHttpRequest();$('#fwgo').disabled=true;$('#fwmsg').textContent='Sending...';
  x.upload.onprogress=e=>{if(e.lengthComputable)$('#fwbar').style.width=(100*e.loaded/e.total)+'%'};
  x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
    $('#fwgo').disabled=false;
    $('#fwmsg').textContent=j.ok?('Installed '+j.version+'. The radar restarts by itself in a moment.'):(j.error||'Failed');
    if(j.ok)$('#fwrestart').classList.remove('hidden');else $('#fwbar').style.width='0';};
  x.onerror=()=>{$('#fwgo').disabled=false;$('#fwmsg').textContent='The upload stopped (Wi-Fi?)'};
  x.open('POST','/api/firmware');x.setRequestHeader('Content-Type','application/octet-stream');x.send(f);
}
async function restart(){await api('/api/restart',{});toast('Restarting... reload this page in a minute',5000)}
(async()=>{
  start();
})();
</script></body></html>
)PAGE";

}  // namespace core::web
