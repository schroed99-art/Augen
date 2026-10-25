#pragma once
// Web-Oberfläche (wird unter http://augen.local/ ausgeliefert).
// Die Vorschau rechnet mit derselben Logik wie augen_render.h / augen_anim.h.
// Ohne Gerät (Datei lokal geöffnet) läuft sie im Demo-Modus.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Augen</title>
<style>
:root{--bg:#101215;--card:#191c21;--line:#2a2f37;--text:#e8eaed;--mute:#8a919c;--acc:#4da3ff;--ok:#3ecf8e;--warn:#f0b429}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:720px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}
h1{font-size:20px;margin:0}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--mute);margin:0 0 10px}
.pill{font-size:12px;padding:4px 10px;border-radius:99px;background:var(--card);border:1px solid var(--line);color:var(--mute)}
.pill.ok{color:var(--ok);border-color:#235c44}.pill.warn{color:var(--warn);border-color:#5c4a1a}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:12px}
.eyes{display:flex;gap:4%;justify-content:center;background:#000;border-radius:14px;padding:18px 8px;margin-bottom:12px}
.eyes canvas{width:42%;max-width:240px;aspect-ratio:1;border-radius:50%;box-shadow:0 0 0 6px #1d2a44}
.row{display:flex;gap:12px;flex-wrap:wrap}
.row>*{flex:1 1 200px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(104px,1fr));gap:8px}
button{font:inherit;color:var(--text);background:#232831;border:1px solid var(--line);border-radius:10px;padding:10px 8px;cursor:pointer}
button:hover{border-color:#3b4350}button:active{transform:scale(.97)}
button.on{background:#17345a;border-color:var(--acc)}
.pad{position:relative;width:100%;max-width:260px;aspect-ratio:1;margin:0 auto;border-radius:50%;background:radial-gradient(circle,#232a35,#15181d);border:1px solid var(--line);touch-action:none;cursor:crosshair}
.pad::before,.pad::after{content:"";position:absolute;background:var(--line)}
.pad::before{left:50%;top:8%;bottom:8%;width:1px}.pad::after{top:50%;left:8%;right:8%;height:1px}
.knob{position:absolute;width:34px;height:34px;margin:-17px 0 0 -17px;border-radius:50%;background:var(--acc);box-shadow:0 0 12px #4da3ff88;left:50%;top:50%;z-index:1}
.sw{display:flex;align-items:center;justify-content:space-between;gap:10px}
.sw input{width:44px;height:24px;accent-color:var(--acc)}
label.f{display:flex;align-items:center;justify-content:space-between;gap:8px;margin:8px 0}
select{font:inherit;color:var(--text);background:#111418;border:1px solid var(--line);border-radius:8px;padding:7px}
input[type=color]{width:52px;height:32px;border:1px solid var(--line);border-radius:8px;background:none;padding:2px}
input[type=text],input[type=password]{width:100%;font:inherit;color:var(--text);background:#111418;border:1px solid var(--line);border-radius:8px;padding:9px;margin:4px 0 8px}
details summary{cursor:pointer;color:var(--mute)}
code{font:12px ui-monospace,Menlo,monospace;color:#c9d1d9;background:#111418;padding:2px 5px;border-radius:5px;word-break:break-all}
.api li{margin:6px 0}.hint{color:var(--mute);font-size:13px}
</style>
</head>
<body>
<main>
<header><h1>👀 Augen <small id="version" style="font-size:12px;color:var(--mute);font-weight:400"></small></h1><span id="status" class="pill">verbinde …</span></header>

<div class="eyes"><canvas id="cL" width="240" height="240"></canvas><canvas id="cR" width="240" height="240"></canvas></div>

<div class="row">
  <section class="card">
    <h2>Blick</h2>
    <div class="pad" id="pad"><div class="knob" id="knob"></div></div>
    <p class="hint" style="text-align:center;margin:8px 0 0">Ziehen zum Schauen · Doppeltippen = Mitte</p>
  </section>
  <section class="card">
    <h2>Ausdruck</h2>
    <div class="grid" id="expr"></div>
    <div class="sw" style="margin-top:14px"><span>Automatik (lebendig)</span><input type="checkbox" id="auto" checked></div>
  </section>
</div>

<section class="card">
  <h2>Aktionen</h2>
  <div class="grid" id="act"></div>
</section>

<section class="card">
  <h2>Schlafen &amp; Bewegungsmelder</h2>
  <p class="hint" id="wachinfo" style="margin-top:0"></p>
  <div class="grid"><button id="wecken">Aufwecken</button><button id="schlafen">Schlafen legen</button></div>
  <label class="f">Von selbst einschlafen
    <select id="nach"><option value="0">nie</option><option value="60">nach 1 min</option><option value="300">nach 5 min</option>
    <option value="900">nach 15 min</option><option value="1800">nach 30 min</option><option value="3600">nach 60 min</option></select></label>
  <label class="f">Im Schlaf
    <select id="modus"><option value="zu">Augen geschlossen</option><option value="aus">Display aus</option></select></label>
</section>

<section class="card">
  <h2>Aussehen</h2>
  <div class="grid" id="stil"></div>
  <label class="f">Iris <input type="color" id="iris"></label>
  <label class="f">Lider <input type="color" id="lid"></label>
</section>

<section class="card">
  <details><summary>WLAN einrichten</summary>
    <p class="hint" id="wlaninfo"></p>
    <input type="text" id="ssid" placeholder="WLAN-Name (SSID)">
    <input type="password" id="pass" placeholder="Passwort">
    <button id="wsave">Speichern &amp; neu starten</button>
  </details>
</section>

<section class="card">
  <details><summary>HTTP-Befehle (für die Hausautomation)</summary>
    <ul class="api hint" id="api"></ul>
  </details>
</section>
</main>

<script>
"use strict";
// ---------- Renderer (wie augen_render.h) ----------
const W=240,MID=120,IR=62,ID=124,WEG=40;
const c8=v=>v<0?0:v>255?255:v|0;
const px=(r,g,b)=>(0xFF000000|(c8(b)<<16)|(c8(g)<<8)|c8(r))>>>0;   // ImageData: ABGR
const R_=c=>c&255,G_=c=>(c>>8)&255,B_=c=>(c>>16)&255;
const sc=(c,f)=>px(R_(c)*f/256,G_(c)*f/256,B_(c)*f/256);
const hex=h=>[parseInt(h.substr(0,2),16),parseInt(h.substr(2,2),16),parseInt(h.substr(4,2),16)];
const WHITE=px(255,255,255),PUP=px(8,8,12);

class Renderer{
  constructor(){this.iris=new Uint32Array(ID*ID);this.sclera=new Uint32Array(128);this.katze=new Uint32Array(256);this.eu=new Int16Array(W);this.el=new Int16Array(W)}
  setStyle(st,irisHex,lidHex){
    this.st=st;const[ir,ig,ib]=hex(irisHex),[lr,lg,lb]=hex(lidHex);
    this.lid=px(lr,lg,lb);this.lash=px(lr*.4,lg*.4,lb*.4);
    this.ic=px(ir,ig,ib);this.idim=px(ir*.4,ig*.4,ib*.4);this.idark=px(ir/8,ig/8,ib/8);this.krand=px(ir/3,ig/4,ib/6);
    for(let k=0;k<128;k++){const r=Math.sqrt(k*256),f=r<78?1:1-.5*Math.pow(Math.min((r-78)/44,1),1.4);this.sclera[k]=px(248*f+(1-f)*70,242*f,236*f)}
    for(let k=0;k<256;k++){const d=Math.sqrt(k*256);let t=1.1-.55*Math.min(d/150,1);if(d<40)t+=.15*(1-d/40);this.katze[k]=px(ir*t,ig*t,ib*t)}
    for(let j=0;j<ID;j++)for(let i=0;i<ID;i++){
      const dx=i-IR+.5,dy=j-IR+.5,d=Math.hypot(dx,dy)/IR;if(d>1)continue;
      const a=Math.atan2(dy,dx),f1=.5+.5*Math.sin(a*29+1.8*Math.sin(a*5+d*4)),f2=.5+.5*Math.sin(a*47+2.5*Math.sin(a*3)-d*7);
      let t=.45+.35*f1*f2+.2*(d-.4);const k=(d-.42)/.06;t+=.18*Math.exp(-k*k);if(d<.3)t*=.85;if(d>.84)t*=1-.75*(d-.84)/.16;
      const w=t>.7?(t-.7)*1.5:0,r=Math.min(ir*t*1.2,255),g=Math.min(ig*t*1.2,255),b=Math.min(ib*t*1.2,255);
      this.iris[j*ID+i]=px(r+(255-r)*w,g+(255-g)*w,b+(255-b)*w)}
  }
  beginFrame(f,rechts){
    const s=rechts?-1:1,ly=f.lookY*14,lidU=Math.min(Math.max(f.lidU,0),1),baseL=250+(118-250)*f.lidL+ly*.5,cv=f.curvL*60-35;
    for(let x=0;x<W;x++){const u=(x-119.5)/120,uu=u*u,yl=baseL+cv*uu+f.tiltL*45*u*s,yo=-25+ly+45*uu+f.tiltU*70*u*s;
      this.eu[x]=Math.trunc(yo+(yl-yo)*lidU);this.el[x]=Math.trunc(yl)}
    this.cx=MID+Math.round(f.lookX*WEG);this.cy=MID+Math.round(f.lookY*WEG);
    const p=Math.min(Math.max(f.pupil,0),1);
    if(this.st===1){const sw=5+Math.trunc(38*p),sh=92;this.sw2=sw*sw;this.sh2=sh*sh;this.rw2=(sw+5)**2;this.rh2=(sh+6)**2}
    else{const pr=(this.st===0?15:12)+Math.trunc(22*p);this.pr2=pr*pr}
  }
  ball(x,y,r2){
    const dx=x-this.cx,dy=y-this.cy,d2=dx*dx+dy*dy;
    if(this.st===0){
      if(d2<IR*IR){if((dx+21)**2+(dy+23)**2<90)return WHITE;if((dx-16)**2+(dy-14)**2<12)return px(230,230,235);
        if(d2<this.pr2)return PUP;return this.iris[(dy+IR)*ID+dx+IR]}
      return this.sclera[Math.min(r2>>8,127)]}
    if(this.st===1){
      if((dx+30)**2+(dy+40)**2<120)return WHITE;const xx=dx*dx,yy=dy*dy;
      if(xx*this.sh2+yy*this.sw2<this.sw2*this.sh2)return PUP;
      if(xx*this.rh2+yy*this.rw2<this.rw2*this.rh2)return this.krand;
      return this.katze[Math.min(d2>>8,255)]}
    const bg=(y&3)===0?px(0,0,0):px(8,8,8);
    if(d2<16)return WHITE;if(d2<this.pr2)return this.ic;if(d2<2500)return this.idark;if(d2<3136)return bg;
    if(d2<4624)return d2<3600?this.ic:this.idim;return bg}
  render(out){
    let i=0;
    for(let y=0;y<W;y++){const dyc=y-MID;
      for(let x=0;x<W;x++,i++){const dxc=x-MID,r2=dxc*dxc+dyc*dyc;
        if(r2>14641){out[i]=0xFF000000;continue}
        const eu=this.eu[x],el=this.el[x];let c;
        if(y<eu)c=y>=eu-3?this.lash:this.lid;
        else if(y>=el)c=y<el+3?this.lash:this.lid;
        else{c=this.ball(x,y,r2);const du=y-eu,dl=el-y;if(du<16)c=sc(c,110+du*9);else if(dl<5)c=sc(c,190+dl*13)}
        out[i]=c}}
  }
}

// ---------- Animation (wie augen_anim.h) ----------
const L=(u,l,cv,tu,tl,p)=>({lidU:u,lidL:l,curvL:cv,tiltU:tu,tiltL:tl,pupil:p});
const AUSDRUECKE=[
  ["neutral","Neutral",L(.18,.10,0,0,0,.5)],["froh","Fröhlich",L(.12,.72,1,0,0,.55)],
  ["boese","Böse",L(.30,.22,0,.75,-.2,.3)],["traurig","Traurig",L(.32,.08,0,-.75,0,.65)],
  ["ueberrascht","Überrascht",L(0,0,0,0,0,.12)],["muede","Müde",L(.58,.12,0,-.15,0,.6)],
  ["skeptisch","Skeptisch",L(.48,.32,0,.2,0,.45),L(.08,.05,0,-.25,0,.45)],
  ["verliebt","Verliebt",L(.15,.55,.8,-.3,0,1)],["schlaf","Schlafen",L(1,.15,0,0,0,.5)]];
const AKTIONEN=[["blinzeln","Blinzeln"],["zwinkern_l","Zwinkern links"],["zwinkern_r","Zwinkern rechts"],["zu","Augen zu"],["auf","Augen auf"],
  ["rollen","Augen rollen"],["umschauen","Umschauen"],["schielen","Schielen"],["erschrecken","Erschrecken"]];

class Engine{
  constructor(){this.cur=[0,1].map(()=>({lookX:0,lookY:0,pupil:.5,lidU:0,lidL:0,curvL:0,tiltU:0,tiltL:0}));
    this.zx=0;this.zy=0;this.conv=0;this.expr=0;this.vorher=0;this.autoOn=true;this.closed=false;this.doppel=false;
    this.t=0;this.nextSacc=1;this.nextBlink=2.5;this.manualUntil=0;this.bt=[-1,-1];this.bd=[.18,.18];this.seq=0;this.seqT=0;
    this.setExpr(0);this.cur.forEach((c,e)=>Object.assign(c,this.ziel[e]))}
  setExpr(i){this.expr=i;const a=AUSDRUECKE[i];this.ziel=[a[2],a[3]||a[2]];if(i!==AUSDRUECKE.length-1)this.closed=false}
  ausdruck(n){const i=AUSDRUECKE.findIndex(a=>a[0]===n);if(i>=0)this.setExpr(i)}
  look(x,y){this.zx=Math.max(-1,Math.min(1,x));this.zy=Math.max(-1,Math.min(1,y));this.manualUntil=this.t+4}
  blink(e,d){this.bt[e]=0;this.bd[e]=d}
  aktion(n){switch(n){
    case"blinzeln":this.blink(0,.18);this.blink(1,.18);break;case"zwinkern_l":this.blink(0,.5);break;case"zwinkern_r":this.blink(1,.5);break;
    case"zu":this.closed=true;break;case"auf":this.closed=false;if(this.expr===AUSDRUECKE.length-1)this.setExpr(0);break;
    case"einschlafen":this.setExpr(AUSDRUECKE.length-1);break;
    case"aufwachen":this.closed=false;this.setExpr(0);this.startSeq(2);this.blink(0,.3);this.blink(1,.3);break;
    case"rollen":this.startSeq(1);break;case"umschauen":this.startSeq(2);break;case"schielen":this.startSeq(3);break;case"erschrecken":this.startSeq(4)}}
  startSeq(s){if(this.seq===4)this.setExpr(this.vorher);this.seq=s;this.seqT=0;this.conv=0;
    if(s===4){this.vorher=this.expr;this.setExpr(4);this.cur.forEach((c,e)=>c.pupil=this.ziel[e].pupil)}}
  runSeq(dt){const q=this.seqT+=dt;
    if(this.seq===1){if(q<.25){this.zx=0;this.zy=-.95}else if(q<1.65){const a=(q-.25)/1.4*6.2832;this.zx=Math.sin(a)*.95;this.zy=-Math.cos(a)*.95}else if(q<2){this.zx=this.zy=0}else this.seq=0}
    else if(this.seq===2){if(q<.8){this.zx=-.95;this.zy=.05}else if(q<1.7){this.zx=.95;this.zy=.05}else if(q<2.2){this.zx=this.zy=0}else this.seq=0}
    else if(this.seq===3){if(q<1.8){this.conv=.8;this.zx=0;this.zy=.2}else{this.conv=0;this.zy=0;this.seq=0}}
    else if(this.seq===4){if(q>1.4){this.setExpr(this.vorher);this.seq=0}}
    if(!this.seq)this.manualUntil=this.t+.8}
  update(dt){dt=Math.min(dt,.2);this.t+=dt;const r=Math.random;if(this.seq)this.runSeq(dt);
    if(this.autoOn&&!this.closed&&!this.seq&&this.t>this.manualUntil&&this.t>=this.nextSacc){
      if(r()<.35){this.zx=(r()-.5)*.3;this.zy=(r()-.5)*.2}else{this.zx=(r()*2-1)*.8;this.zy=(r()*2-1)*.5}this.nextSacc=this.t+.5+r()*2.5}
    if(this.autoOn&&!this.closed&&this.t>=this.nextBlink){this.blink(0,.18);this.blink(1,.18);
      if(this.doppel){this.doppel=false;this.nextBlink=this.t+2+r()*4.5}else if(r()<.15){this.doppel=true;this.nextBlink=this.t+.35}else this.nextBlink=this.t+2+r()*4.5}
    const kl=1-Math.exp(-22*dt),kd=1-Math.exp(-10*dt),kp=1-Math.exp(-4*dt);
    this.cur.forEach((c,e)=>{const z=Object.assign({},this.ziel[e]);if(this.closed)z.lidU=1;
      c.lookX+=(this.zx+(e?-this.conv:this.conv)-c.lookX)*kl;c.lookY+=(this.zy-c.lookY)*kl;
      for(const k of["lidU","lidL","curvL","tiltU","tiltL"])c[k]+=(z[k]-c[k])*kd;c.pupil+=(z.pupil-c.pupil)*kp;
      if(this.bt[e]>=0){this.bt[e]+=dt;if(this.bt[e]>this.bd[e])this.bt[e]=-1}})}
  frame(e){const f=Object.assign({},this.cur[e]);
    if(this.bt[e]>=0){const p=this.bt[e]/this.bd[e];let c=p<.4?p/.4:p<.55?1:1-(p-.55)/.45;c=c*c*(3-2*c);f.lidU+=(1-f.lidU)*c}return f}
}

// ---------- Oberfläche ----------
const $=id=>document.getElementById(id);
const STILE=[["mensch","Mensch","3c82c8","1e1614"],["katze","Katze","ffc414","0a0a0e"],["roboter","Roboter","00dcff","283038"]];
const eng=new Engine(),ren=new Renderer();
let stil=0,iris="3c82c8",lid="1e1614",online=false,wach=true,dunkel=false,dunkelTimer;

function status(ok,text){const s=$("status");s.textContent=text;s.className="pill "+(ok?"ok":"warn")}
async function api(path){
  if(location.protocol==="file:")return null;
  try{const r=await fetch(path);const j=await r.json();if(!online){online=true;status(true,"verbunden")}return j}
  catch(e){online=false;status(false,"keine Verbindung");return null}}

function setStil(i,ir,li){stil=i;iris=ir;lid=li;ren.setStyle(i,iris,lid);$("iris").value="#"+iris;$("lid").value="#"+lid;
  document.querySelectorAll("#stil button").forEach((b,k)=>b.classList.toggle("on",k===i))}
function markExpr(){document.querySelectorAll("#expr button").forEach((b,k)=>b.classList.toggle("on",k===eng.expr))}

AUSDRUECKE.forEach(([id,name])=>{const b=document.createElement("button");b.textContent=name;
  b.onclick=()=>{eng.ausdruck(id);markExpr();api("api/expr?name="+id)};$("expr").append(b)});
AKTIONEN.forEach(([id,name])=>{const b=document.createElement("button");b.textContent=name;
  b.onclick=()=>{eng.aktion(id);markExpr();api("api/action?name="+id)};$("act").append(b)});
STILE.forEach(([id,name,ir,li],i)=>{const b=document.createElement("button");b.textContent=name;
  b.onclick=()=>{setStil(i,ir,li);api("api/style?name="+id)};$("stil").append(b)});
$("auto").onchange=e=>{eng.autoOn=e.target.checked;if(!eng.autoOn){eng.zx=eng.zy=0}api("api/auto?on="+(eng.autoOn?1:0))};
for(const k of["iris","lid"])$(k).oninput=e=>{const v=e.target.value.slice(1);if(k==="iris")iris=v;else lid=v;ren.setStyle(stil,iris,lid);sendFarbe()};
let farbTimer;function sendFarbe(){clearTimeout(farbTimer);farbTimer=setTimeout(()=>api(`api/style?iris=${iris}&lid=${lid}`),250)}

// Schlafen / Wecken (lokal nachgebildet, Gerät ist maßgeblich)
function zeigeWach(w){
  if(w===wach)return;wach=w;clearTimeout(dunkelTimer);
  if(w){dunkel=false;eng.aktion("aufwachen")}
  else{eng.aktion("einschlafen");if($("modus").value==="aus")dunkelTimer=setTimeout(()=>{dunkel=true},1500)}
  markExpr();$("wachinfo").textContent=w?"Wach.":"Schläft – eine Bewegungsmeldung weckt die Augen."}
$("wecken").onclick=()=>{zeigeWach(true);api("api/presence?on=1")};
$("schlafen").onclick=()=>{zeigeWach(false);api("api/presence?on=0")};
for(const k of["nach","modus"])$(k).onchange=()=>api(`api/sleep?nach=${$("nach").value}&modus=${$("modus").value}`);

// Blick-Pad: lokal sofort, ans Gerät höchstens alle 80 ms
const pad=$("pad"),knob=$("knob");let ziehen=false,offen=null,inFlug=false;
function zeigeKnob(x,y){knob.style.left=(50+x*42)+"%";knob.style.top=(50+y*42)+"%"}
async function sendeBlick(){if(inFlug||!offen)return;inFlug=true;const[x,y]=offen;offen=null;
  await api(`api/look?x=${x.toFixed(2)}&y=${y.toFixed(2)}`);setTimeout(()=>{inFlug=false;sendeBlick()},80)}
function blick(x,y){const d=Math.hypot(x,y);if(d>1){x/=d;y/=d}zeigeKnob(x,y);eng.look(x,y);offen=[x,y];sendeBlick()}
function ausPad(e){const r=pad.getBoundingClientRect();blick(((e.clientX-r.left)/r.width-.5)/.42,((e.clientY-r.top)/r.height-.5)/.42)}
pad.onpointerdown=e=>{ziehen=true;pad.setPointerCapture(e.pointerId);ausPad(e)};
pad.onpointermove=e=>{if(ziehen)ausPad(e)};
pad.onpointerup=pad.onpointercancel=()=>{ziehen=false};
pad.ondblclick=()=>blick(0,0);

$("wsave").onclick=async()=>{const b=new URLSearchParams({ssid:$("ssid").value,pass:$("pass").value});
  try{await fetch("api/wifi",{method:"POST",body:b});alert("Gespeichert. Das Gerät startet neu und ist danach unter http://augen.local erreichbar.")}
  catch(e){alert("Speichern fehlgeschlagen")}};

const base=location.protocol==="file:"?"http://augen.local":location.origin;
$("api").innerHTML=[
  ["/api/look?x=0.5&y=-0.2","Blick, x/y von -1 bis 1"],
  ["/api/expr?name=froh","Ausdruck: "+AUSDRUECKE.map(a=>a[0]).join(", ")],
  ["/api/action?name=zwinkern_l","Aktion: "+AKTIONEN.map(a=>a[0]).join(", ")],
  ["/api/auto?on=1","Automatik an (0 = aus)"],
  ["/api/style?name=katze&iris=ffc414&lid=0a0a0e","Stil: mensch, katze, roboter; Farben als RRGGBB"],
  ["/api/presence?on=1","Bewegung/Präsenz: weckt und startet den Schlaf-Timer neu (on=0 = schlafen)"],
  ["/api/sleep?nach=300&modus=aus","von selbst einschlafen nach Sekunden (0 = nie), Modus zu/aus"],
  ["/api/open · /api/close · /api/status","für den NexusHomeBase-Treiber „esp“: auf = wach, zu = schlafen"],
  ["/api/state","aktueller Zustand als JSON"]].map(([p,t])=>`<li><code>${base}${p}</code><br>${t}</li>`).join("");

// Vorschau zeichnen
const ctx=[$("cL"),$("cR")].map(c=>c.getContext("2d")),img=ctx.map(c=>c.createImageData(W,W)),buf=img.map(i=>new Uint32Array(i.data.buffer));
let vorher=performance.now();
function tick(jetzt){eng.update((jetzt-vorher)/1000);vorher=jetzt;
  for(let e=0;e<2;e++){if(dunkel)buf[e].fill(0xFF000000);else{ren.beginFrame(eng.frame(e),e===1);ren.render(buf[e])}ctx[e].putImageData(img[e],0,0)}
  requestAnimationFrame(tick)}

(async()=>{
  setStil(0,iris,lid);markExpr();
  const s=await api("api/state");
  if(s){const i=Math.max(0,STILE.findIndex(x=>x[0]===s.stil));setStil(i,s.iris.toLowerCase(),s.lid.toLowerCase());
    $("version").textContent=s.version?"v"+s.version:"";
    eng.ausdruck(s.ausdruck);eng.autoOn=s.auto;eng.closed=s.zu;$("auto").checked=s.auto;markExpr();
    $("nach").value=String(s.schlaf_nach);$("modus").value=s.schlaf_modus;wach=s.wach;dunkel=!s.wach&&s.schlaf_modus==="aus";
    $("wachinfo").textContent=wach?"Wach.":"Schläft – eine Bewegungsmeldung weckt die Augen.";
    setInterval(async()=>{const z=await api("api/state");if(z)zeigeWach(z.wach)},3000);
    $("wlaninfo").textContent=s.ap?"Gerät läuft im Setup-WLAN. Heimnetz eintragen:":"Verbunden mit „"+s.ssid+"“ ("+s.ip+").";}
  else{status(false,"Demo – kein Gerät");$("wlaninfo").textContent="Nur am Gerät verfügbar.";$("wachinfo").textContent="Wach."}
  requestAnimationFrame(tick)})();
</script>
</body>
</html>
)HTML";
