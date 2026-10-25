#pragma once
#include <Arduino.h>

// Page de configuration servie par l'ESP32 (HTML + JS embarques, aucune dependance externe).
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD Deck</title>
<style>
:root{--bg:#0f1115;--card:#181b22;--line:#2a2f3a;--fg:#e8eaf0;--mut:#8a93a6;--acc:#4f8cff}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif}
header{display:flex;gap:10px;align-items:center;padding:12px 16px;border-bottom:1px solid var(--line);flex-wrap:wrap}
h1{font-size:18px;margin:0;flex:1}
.pill{padding:3px 10px;border-radius:99px;background:#252a35;font-size:13px;color:var(--mut)}
.pill.on{background:#143d26;color:#4ade80}
main{max-width:860px;margin:0 auto;padding:16px;display:grid;gap:16px}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px}
h2{font-size:13px;margin:0 0 10px;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}
.tabs{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}
button,select,input{font:inherit;color:var(--fg);background:#232833;border:1px solid var(--line);border-radius:8px;padding:7px 11px}
button{cursor:pointer}
button.p{background:var(--acc);border-color:var(--acc);color:#fff}
button.d{color:#ff7a7a}
.tab.on{background:var(--acc);border-color:var(--acc);color:#fff}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;max-width:420px;margin:10px 0}
.cell{aspect-ratio:3/2;border-radius:10px;border:2px dashed var(--line);display:flex;align-items:center;justify-content:center;text-align:center;padding:4px;font-size:13px;cursor:pointer;overflow:hidden;color:var(--mut)}
.cell.full{border-style:solid;border-color:transparent;font-weight:600}
.cell.sel{outline:3px solid var(--acc);outline-offset:1px}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin:8px 0}
label{color:var(--mut);font-size:13px;min-width:60px}
input[type=text],input[type=password]{flex:1;min-width:140px}
input[type=color]{padding:2px;width:46px;height:34px}
input[type=file]{flex:1;min-width:160px}
.msg{font-size:13px;color:var(--mut);margin:6px 0}
.bar{position:sticky;bottom:0;background:var(--bg);padding:10px 0;display:flex;gap:10px;align-items:center;flex-wrap:wrap}
progress{width:140px}
</style></head><body>
<header><h1>CYD Deck</h1><span class="pill" id="pBle">Bluetooth : ?</span><span class="pill" id="pIp">IP : ?</span><span class="pill" id="pFw"></span></header>
<main>
<section><h2>Pages et boutons</h2>
 <div class="tabs" id="tabs"></div>
 <div class="row"><label>Nom page</label><input type="text" id="pname" maxlength="19" oninput="renamePage(this.value)"><button class="d" onclick="delPage()">Supprimer la page</button></div>
 <div class="grid" id="grid"></div>
 <div id="editor"></div>
</section>
<section><h2>Affichage et clavier</h2>
 <div class="row"><label>Clavier PC</label><select id="azerty" onchange="setS('azerty',this.value==='1')"><option value="1">AZERTY (Belgique / France)</option><option value="0">QWERTY (US)</option></select></div>
 <div class="row"><label>Couleurs</label><label style="min-width:0"><input type="checkbox" id="invert" onchange="setS('invert',this.checked)"> inverser les couleurs de l'écran</label></div>
 <div class="row"><label>Luminosité</label><input type="range" id="bright" min="10" max="255" oninput="setS('brightness',+this.value)"></div>
 <p class="msg">Raccourcis : <code>ctrl+shift+m</code>, <code>win+d</code>, <code>alt+tab</code>, <code>f13</code>… Modificateurs : ctrl, shift, alt, altgr, win.</p>
</section>
<section><h2>Maintenance</h2>
 <div class="row"><label>Firmware</label><input type="file" id="fw" accept=".bin"><button onclick="upload()">Envoyer</button><progress id="fwp" max="100" value="0"></progress></div>
 <p class="msg">Envoie le fichier <code>.pio/build/&lt;env&gt;/firmware.bin</code> (ou <code>*-ota.bin</code> du dossier web/firmware).</p>
 <div class="row"><label>Wi-Fi</label><input type="text" id="ssid" placeholder="Nom du réseau"><input type="password" id="wpass" placeholder="Mot de passe"><button onclick="saveWifi()">Enregistrer + redémarrer</button></div>
 <div class="row"><label>Accès web</label><input type="password" id="npass" placeholder="Nouveau mot de passe (4+ car.)"><button onclick="savePass()">Changer</button></div>
 <div class="row"><button onclick="exportCfg()">Exporter la config</button><label style="min-width:0">Importer :</label><input type="file" id="imp" accept=".json" onchange="importCfg(this)"></div>
</section>
</main>
<main><div class="bar"><button class="p" onclick="save()">Enregistrer sur le CYD</button><span class="msg" id="msg"></span></div></main>
<script>
const $=s=>document.querySelector(s);
const MEDIA=[['playpause','Lecture / Pause'],['prev','Piste précédente'],['next','Piste suivante'],['stop','Stop'],['mute','Muet'],['volup','Volume +'],['voldown','Volume -']];
let cfg=null,cur=0,sel=null;
const esc=s=>String(s==null?'':s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function say(t,err){const m=$('#msg');m.textContent=t;m.style.color=err?'#ff7a7a':'#4ade80'}
async function api(path,opt){
  const r=await fetch(path,opt);const t=await r.text();let j=null;try{j=JSON.parse(t)}catch(e){}
  if(!r.ok)throw new Error((j&&(j.error||j.msg))||t||('HTTP '+r.status));
  return j!==null?j:t;
}
const post=(p,o)=>api(p,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});
const btnAt=i=>cfg.pages[cur].buttons.find(b=>b.i===i);
function textOn(h){const n=parseInt(h.slice(1),16),r=n>>16,g=(n>>8)&255,b=n&255;return (r*299+g*587+b*114)/1000>150?'#000':'#fff'}

function drawTabs(){
  $('#tabs').innerHTML=cfg.pages.map((p,i)=>'<button class="tab '+(i===cur?'on':'')+'" onclick="go('+i+')">'+esc(p.name)+'</button>').join('')+(cfg.pages.length<6?'<button onclick="addPage()">+</button>':'');
  $('#pname').value=cfg.pages[cur].name;
}
function drawGrid(){
  let g='';
  for(let i=0;i<9;i++){
    const b=btnAt(i),s=sel===i?' sel':'';
    g+=b?'<div class="cell full'+s+'" style="background:'+esc(b.color)+';color:'+textOn(b.color)+'" onclick="pick('+i+')">'+esc(b.label)+'</div>'
        :'<div class="cell'+s+'" onclick="pick('+i+')">+</div>';
  }
  $('#grid').innerHTML=g;
}
function drawEditor(){
  const e=$('#editor');
  if(sel===null){e.innerHTML='<p class="msg">Touche une case pour la modifier.</p>';return}
  const b=btnAt(sel);
  if(!b){e.innerHTML='<button class="p" onclick="mk()">Créer un bouton ici</button>';return}
  const val=b.action==='media'
    ?'<select onchange="setf(\'value\',this.value)">'+MEDIA.map(m=>'<option value="'+m[0]+'"'+(b.value===m[0]?' selected':'')+'>'+m[1]+'</option>').join('')+'</select>'
    :'<input type="text" value="'+esc(b.value)+'" maxlength="63" placeholder="'+(b.action==='key'?'ctrl+shift+m, win+d, f13…':'Texte à taper')+'" oninput="setf(\'value\',this.value)">';
  const opt=(v,t)=>'<option value="'+v+'"'+(b.action===v?' selected':'')+'>'+t+'</option>';
  e.innerHTML='<div class="row"><label>Nom</label><input type="text" value="'+esc(b.label)+'" maxlength="23" oninput="setf(\'label\',this.value)"><input type="color" value="'+esc(b.color)+'" oninput="setf(\'color\',this.value)"></div>'
   +'<div class="row"><label>Action</label><select onchange="setAct(this.value)">'+opt('key','Raccourci clavier')+opt('media','Touche multimédia')+opt('text','Taper du texte')+'</select>'+val+'</div>'
   +'<div class="row"><button onclick="testBtn()">Tester</button><button class="d" onclick="rm()">Supprimer le bouton</button></div>';
}
function draw(){
  drawTabs();drawGrid();drawEditor();
  $('#azerty').value=cfg.settings.azerty?'1':'0';
  $('#invert').checked=!!cfg.settings.invert;
  $('#bright').value=cfg.settings.brightness;
}
function go(i){cur=i;sel=null;draw()}
function pick(i){sel=i;drawGrid();drawEditor()}
function addPage(){cfg.pages.push({name:'Page '+(cfg.pages.length+1),buttons:[]});cur=cfg.pages.length-1;sel=null;draw()}
function delPage(){if(cfg.pages.length<2){say('Il faut garder au moins une page',1);return}
  if(!confirm('Supprimer la page « '+cfg.pages[cur].name+' » ?'))return;
  cfg.pages.splice(cur,1);cur=0;sel=null;draw()}
function renamePage(v){cfg.pages[cur].name=v;
  const t=$('#tabs').children[cur];if(t)t.textContent=v}
function mk(){cfg.pages[cur].buttons.push({i:sel,label:'Bouton',color:'#3a6ea5',action:'key',value:''});drawGrid();drawEditor()}
function rm(){const p=cfg.pages[cur];p.buttons=p.buttons.filter(b=>b.i!==sel);drawGrid();drawEditor()}
function setf(k,v){btnAt(sel)[k]=v;drawGrid()}
function setAct(a){const b=btnAt(sel);b.action=a;b.value=a==='media'?'playpause':'';drawEditor()}
function setS(k,v){cfg.settings[k]=v}
async function testBtn(){const b=btnAt(sel);
  try{await post('/api/press',{action:b.action,value:b.value});say('Envoyé au PC',0)}catch(e){say(e.message,1)}}
async function save(){
  try{await post('/api/config',cfg);say('Enregistré sur le CYD ✔',0)}catch(e){say(e.message,1)}}
function exportCfg(){
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(cfg,null,1)],{type:'application/json'}));
  a.download='cyd-deck.json';a.click()}
function importCfg(inp){
  const f=inp.files[0];if(!f)return;const r=new FileReader();
  r.onload=()=>{try{const j=JSON.parse(r.result);if(!j.pages||!j.pages.length)throw new Error('Fichier invalide');
    j.settings=j.settings||{invert:false,brightness:200,azerty:true};cfg=j;cur=0;sel=null;draw();say('Importé : pense à enregistrer',0)}
    catch(e){say(e.message,1)}};
  r.readAsText(f)}
function upload(){
  const f=$('#fw').files[0];if(!f){say('Choisis un fichier .bin',1);return}
  const fd=new FormData();fd.append('update',f);
  const x=new XMLHttpRequest();x.open('POST','/update');
  x.upload.onprogress=e=>{if(e.lengthComputable)$('#fwp').value=e.loaded/e.total*100};
  x.onload=()=>say(x.status===200?'Firmware installé, redémarrage…':'Échec : '+x.responseText,x.status!==200);
  x.onerror=()=>say('Connexion perdue (normal si le CYD redémarre)',0);
  x.send(fd)}
async function saveWifi(){
  try{await post('/api/wifi',{ssid:$('#ssid').value,pass:$('#wpass').value});say('Wi-Fi enregistré, le CYD redémarre…',0)}catch(e){say(e.message,1)}}
async function savePass(){
  try{await post('/api/pass',{pass:$('#npass').value});say('Mot de passe changé, le CYD redémarre…',0)}catch(e){say(e.message,1)}}
async function poll(){
  try{const s=await api('/api/state');
    const b=$('#pBle');b.textContent='Bluetooth : '+(s.ble?'connecté':'non connecté');b.className='pill'+(s.ble?' on':'');
    $('#pIp').textContent=(s.ap?'Mode config · ':'')+s.ip+(s.sd?' · SD':'');$('#pFw').textContent='v'+s.fw;
  }catch(e){$('#pBle').textContent='CYD hors ligne';$('#pBle').className='pill'}
}
(async()=>{
  try{cfg=await api('/api/config');draw()}catch(e){say(e.message,1)}
  poll();setInterval(poll,4000);
})();
</script></body></html>)HTML";
