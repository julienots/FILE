// Feuille de style unique de l'UI (B). Style cartoon premium: verre teinté, contours épais, dégradés vifs, animations ressort.
// Unités vmin (paysage: vmin = hauteur) → tailles cohérentes téléphone/tablette. Safe-areas via --sa*.
export const CSS = `
:root{--sat:env(safe-area-inset-top,0px);--sar:env(safe-area-inset-right,0px);--sab:env(safe-area-inset-bottom,0px);--sal:env(safe-area-inset-left,0px);
 --ink:#1a0f33;--glass:rgba(22,14,52,.55);--glass2:rgba(40,26,90,.72);--line:rgba(255,255,255,.22);--gold:#ffd76a;--cy:#7fe8ff;--pk:#ff6fb5;--vi:#a07bff;
 --font:ui-rounded,"SF Pro Rounded","Nunito","Varela Round","Quicksand",system-ui,sans-serif;--spring:cubic-bezier(.34,1.56,.64,1)}
#ui{font-family:var(--font);color:#fff;-webkit-tap-highlight-color:transparent}
#ui .layer{position:absolute;inset:0;pointer-events:none!important}
#ui .layer>*{pointer-events:auto}
#ui .noev,#ui .noev *{pointer-events:none!important}
.hidden{display:none!important}
#ui .screens{z-index:20}#ui .cfx{z-index:5}#ui .dlg{z-index:10}
.txt-out{text-shadow:0 .3vmin 0 var(--ink),0 0 1.4vmin rgba(0,0,0,.5)}

/* ── HUD ── */
.hud-tl{position:absolute;top:calc(var(--sat) + 2.5vmin);left:calc(var(--sal) + 3vmin);display:flex;gap:2vmin;align-items:center}
.portrait{width:14vmin;height:14vmin;border-radius:50%;position:relative;flex:none;display:grid;place-items:center;font-size:6vmin;
 background:radial-gradient(circle at 35% 30%,#fff8,transparent 45%),radial-gradient(circle,var(--c1,#9fe8ff),var(--c2,#3a6aff));border:.6vmin solid #fff;box-shadow:0 .6vmin 0 var(--ink),0 0 3vmin var(--c1,#7fd0ff)}
.portrait b{position:absolute;bottom:-1.6vmin;left:50%;transform:translateX(-50%);background:var(--gold);color:var(--ink);font-size:2.6vmin;font-weight:900;border-radius:2vmin;padding:.2vmin 1.4vmin;border:.4vmin solid var(--ink);white-space:nowrap}
.bars{display:flex;flex-direction:column;gap:1vmin;min-width:38vmin}
.bars .nm{font-weight:900;font-size:3.4vmin;letter-spacing:.02em}
.bar{height:2.6vmin;border-radius:2vmin;background:rgba(10,6,30,.7);border:.4vmin solid var(--ink);overflow:hidden;position:relative;box-shadow:0 .4vmin 0 rgba(0,0,0,.35)}
.bar i{position:absolute;inset:0;transform-origin:left;transition:transform .35s var(--spring);border-radius:2vmin}
.bar i::after{content:"";position:absolute;left:4%;right:4%;top:15%;height:30%;background:rgba(255,255,255,.45);border-radius:2vmin}
.bar.hp i{background:linear-gradient(90deg,#ff4a7a,#ff8aa8)}
.bar.en i{background:linear-gradient(90deg,#3ab8ff,#8af0ff)}
.bar.xp{height:2vmin}.bar.xp i{background:linear-gradient(90deg,#ffb02e,#ffe98a)}
.bar span{position:absolute;right:1.2vmin;top:50%;transform:translateY(-50%);font-size:1.8vmin;font-weight:800;line-height:1}
.xpline{font-size:2.2vmin;opacity:.85;font-weight:700}
.hud-tr{position:absolute;top:calc(var(--sat) + 2.5vmin);right:calc(var(--sar) + 3vmin);display:flex;gap:1.6vmin;align-items:center}
.chip{display:flex;align-items:center;gap:1vmin;background:var(--glass);border:.4vmin solid var(--line);border-radius:4vmin;padding:.8vmin 2vmin;font-weight:900;font-size:3vmin;backdrop-filter:blur(6px)}
.ibtn{width:9vmin;height:9vmin;border-radius:50%;display:grid;place-items:center;font-size:4vmin;background:var(--glass2);border:.5vmin solid #fff;box-shadow:0 .6vmin 0 var(--ink);transition:transform .15s var(--spring)}
.ibtn:active{transform:scale(.88) translateY(.4vmin);box-shadow:0 .2vmin 0 var(--ink)}
.hud-top{position:absolute;top:calc(var(--sat) + 2vmin);left:50%;transform:translateX(-50%);text-align:center;max-width:46vw}
.loc{font-weight:900;font-size:3.6vmin}.loc small{display:block;font-size:2.2vmin;opacity:.75;font-weight:700}
.hint{margin-top:1vmin;font-size:2.8vmin;font-weight:800;background:var(--glass);border-radius:3vmin;padding:.8vmin 2.4vmin;display:inline-block;border:.3vmin solid var(--line)}
.hint:empty{display:none}.hint.go{background:linear-gradient(90deg,#ff9a3c,#ff5fa8);border-color:#fff;animation:pulse 1.2s ease-in-out infinite}
.quest{position:absolute;top:calc(var(--sat) + 22vmin);right:calc(var(--sar) + 3vmin);width:34vmin;background:var(--glass);border-left:.8vmin solid var(--gold);border-radius:1.6vmin;padding:1.4vmin 2vmin;font-size:2.4vmin;backdrop-filter:blur(6px)}
.quest h4{margin:0 0 .6vmin;font-size:2.2vmin;color:var(--gold);text-transform:uppercase;letter-spacing:.1em}
.quest .pg{margin-top:.8vmin;font-weight:800;opacity:.85}
.toasts{position:absolute;top:calc(var(--sat) + 17vmin);left:50%;transform:translateX(-50%);display:flex;flex-direction:column;align-items:center;gap:1vmin}
.toast{background:var(--glass2);border:.4vmin solid #fff;border-radius:3vmin;padding:1vmin 3vmin;font-weight:900;font-size:3vmin;box-shadow:0 .6vmin 0 var(--ink);animation:toastIn .45s var(--spring),toastOut .4s ease-in 2.6s forwards}
.toast.discover{background:linear-gradient(90deg,#3a6aff,#a07bff)}.toast.xp{background:linear-gradient(90deg,#ffb02e,#ff7a3c);font-size:2.6vmin;padding:.6vmin 2.4vmin}
.toast.warn{background:linear-gradient(90deg,#ff3b5c,#ff7a3c)}
.evo{position:absolute;inset:0;display:grid;place-items:center;text-align:center;background:radial-gradient(circle,rgba(255,240,200,.35),transparent 60%)}
.evo h1{margin:0;font-size:11vmin;font-weight:900;letter-spacing:.04em;background:linear-gradient(180deg,#fff,#ffe98a 50%,#ff9a3c);-webkit-background-clip:text;background-clip:text;color:transparent;
 filter:drop-shadow(0 1vmin 0 var(--ink)) drop-shadow(0 0 3vmin #ffb02e);animation:evoPop .9s var(--spring)}
.evo h2{margin:1vmin 0 0;font-size:5.5vmin;font-weight:900}.evo p{margin:1vmin auto 0;font-size:3vmin;max-width:60vmin;opacity:.9}
.evo .ab{margin-top:2vmin;display:inline-block;background:var(--glass2);border:.4vmin solid var(--gold);border-radius:3vmin;padding:1vmin 3vmin;font-weight:900;font-size:3vmin}
.evo{animation:fadeOut .6s ease-in 3.6s forwards}

/* ── Contrôles ── */
.joy{position:absolute;left:calc(var(--sal) + 5vmin);bottom:calc(var(--sab) + 6vmin);width:32vmin;height:32vmin;border-radius:50%;
 background:radial-gradient(circle,rgba(160,123,255,.12),rgba(255,255,255,.05) 70%);border:.6vmin solid rgba(255,255,255,.35);box-shadow:inset 0 0 4vmin rgba(127,208,255,.25)}
.joy i{position:absolute;left:50%;top:50%;width:14vmin;height:14vmin;margin:-7vmin;border-radius:50%;background:radial-gradient(circle at 35% 30%,#fff,#9fe8ff 40%,#3a8aff);
 border:.5vmin solid #fff;box-shadow:0 .6vmin 0 var(--ink),0 0 3vmin #7fd0ffaa;transition:transform .08s}
.joy.act i{transition:none}
.pad{position:absolute;right:calc(var(--sar) + 3vmin);bottom:calc(var(--sab) + 3vmin);width:48vmin;height:48vmin;pointer-events:none!important}
.btn{position:absolute;border-radius:50%;display:grid;place-items:center;color:#fff;font-weight:900;border:.6vmin solid #fff;pointer-events:auto;
 box-shadow:0 .8vmin 0 var(--ink),0 0 2.4vmin var(--glow,#a07bff88);transition:transform .12s var(--spring),box-shadow .12s;touch-action:none}
.btn.on{transform:scale(.88) translateY(.6vmin);box-shadow:0 .2vmin 0 var(--ink),0 0 4vmin var(--glow,#a07bff)}
.btn small{position:absolute;bottom:-3.6vmin;font-size:2vmin;white-space:nowrap;opacity:.9}
.b-attack{width:19vmin;height:19vmin;right:2vmin;bottom:2vmin;font-size:8vmin;background:radial-gradient(circle at 35% 30%,#ffb0c8,#ff4a7a);--glow:#ff4a7a99}
.b-jump{width:13vmin;height:13vmin;right:24vmin;bottom:1vmin;font-size:5.5vmin;background:radial-gradient(circle at 35% 30%,#b0ffd8,#2ac87a);--glow:#2ac87a99}
.b-boost{width:13vmin;height:13vmin;right:4vmin;bottom:24vmin;font-size:5.5vmin;background:radial-gradient(circle at 35% 30%,#b0e8ff,#2a8aff);--glow:#2a8aff99}
.b-special{width:13vmin;height:13vmin;right:21vmin;bottom:18vmin;font-size:5.5vmin;background:radial-gradient(circle at 35% 30%,#ffe9a0,#ffa02e);--glow:#ffa02e99}
.b-special .cd{position:absolute;inset:0;border-radius:50%;background:conic-gradient(rgba(10,6,30,.7) var(--p,0%),transparent 0)}
.b-action{width:15vmin;height:15vmin;right:33vmin;bottom:28vmin;font-size:3vmin;line-height:1.05;text-align:center;background:radial-gradient(circle at 35% 30%,#fff,#c0a0ff 55%,#7a4aff);--glow:#c0a0ff;
 animation:pulse 1.4s ease-in-out infinite}
.b-action.off{display:none}
.camzone{position:absolute;right:0;top:0;width:55%;height:100%}
.jzone{position:absolute;left:0;top:18%;width:45%;height:82%}
#ui.lefty .jzone{left:auto;right:0}
#ui.lefty .joy{left:auto;right:calc(var(--sar) + 5vmin)}
#ui.lefty .pad{right:auto;left:calc(var(--sal) + 3vmin);transform:scaleX(-1)}
#ui.lefty .pad .btn>*{transform:scaleX(-1)}#ui.lefty .pad .btn{}
#ui.lefty .camzone{right:auto;left:0}

/* ── Écrans (titre, pause, réglages) ── */
.screen{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:radial-gradient(ellipse at center,rgba(20,10,50,.35),rgba(5,3,15,.88));animation:fadeIn .35s}
.panel{background:linear-gradient(160deg,rgba(60,40,130,.92),rgba(24,14,60,.94));border:.7vmin solid #fff;border-radius:5vmin;padding:4vmin 6vmin;box-shadow:0 1.2vmin 0 var(--ink),0 0 8vmin rgba(160,123,255,.45);
 min-width:60vmin;max-width:90vw;max-height:86vh;overflow:auto;animation:popIn .45s var(--spring)}
.panel h2{margin:0 0 3vmin;text-align:center;font-size:6vmin;font-weight:900}
.menu{display:flex;flex-direction:column;gap:2vmin;align-items:stretch}
.cbtn{font:inherit;font-weight:900;font-size:4vmin;color:var(--ink);border:.6vmin solid var(--ink);border-radius:4vmin;padding:1.6vmin 6vmin;cursor:pointer;
 background:linear-gradient(180deg,#fff 0%,#ffe98a 55%,#ffb02e 100%);box-shadow:0 .9vmin 0 var(--ink);transition:transform .12s var(--spring)}
.cbtn:active{transform:translateY(.7vmin);box-shadow:0 .2vmin 0 var(--ink)}
.cbtn.alt{background:linear-gradient(180deg,#fff 0%,#c8e8ff 55%,#7fb8ff 100%)}
.cbtn.ghost{background:rgba(255,255,255,.12);color:#fff;border-color:#fff;box-shadow:0 .9vmin 0 rgba(0,0,0,.4)}
.cbtn.danger{background:linear-gradient(180deg,#fff 0%,#ffb0c0 55%,#ff4a7a 100%)}
.cbtn[disabled]{opacity:.45;filter:grayscale(.6)}
.title{flex-direction:column;background:radial-gradient(ellipse at 50% 40%,rgba(60,30,120,.25),rgba(5,3,15,.75))}
.logo{font-size:13vmin;font-weight:900;line-height:.9;text-align:center;letter-spacing:.02em;margin-bottom:1vmin;
 background:linear-gradient(180deg,#fff 10%,#9fe8ff 45%,#a07bff 75%,#ff6fb5);-webkit-background-clip:text;background-clip:text;color:transparent;
 filter:drop-shadow(0 1.2vmin 0 var(--ink)) drop-shadow(0 0 4vmin rgba(160,123,255,.8));animation:float 4s ease-in-out infinite}
.logo span{display:block;font-size:.55em;letter-spacing:.18em}
.tag{font-size:3.4vmin;font-weight:800;opacity:.9;margin-bottom:5vmin}
.title .menu{min-width:50vmin}
.ver{position:absolute;bottom:calc(var(--sab) + 2vmin);right:calc(var(--sar) + 3vmin);font-size:2.2vmin;opacity:.5}
.row{display:flex;align-items:center;justify-content:space-between;gap:4vmin;padding:1.4vmin 0;border-bottom:.3vmin solid rgba(255,255,255,.12);font-size:3.2vmin;font-weight:800}
.seg{display:flex;gap:1vmin}.seg button{font:inherit;font-size:2.6vmin;font-weight:900;color:#fff;background:rgba(255,255,255,.1);border:.4vmin solid rgba(255,255,255,.4);border-radius:3vmin;padding:.8vmin 2.2vmin}
.seg button.sel{background:var(--gold);color:var(--ink);border-color:var(--ink)}
input[type=range]{-webkit-appearance:none;appearance:none;width:30vmin;height:2.4vmin;border-radius:2vmin;background:rgba(255,255,255,.18);border:.3vmin solid rgba(255,255,255,.4)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:5vmin;height:5vmin;border-radius:50%;background:radial-gradient(circle at 35% 30%,#fff,var(--gold));border:.5vmin solid var(--ink)}
.tog{width:11vmin;height:5.6vmin;border-radius:3vmin;background:rgba(255,255,255,.15);border:.4vmin solid rgba(255,255,255,.5);position:relative;transition:background .2s}
.tog::after{content:"";position:absolute;top:.4vmin;left:.5vmin;width:4vmin;height:4vmin;border-radius:50%;background:#fff;transition:transform .25s var(--spring)}
.tog.on{background:#2ac87a}.tog.on::after{transform:translateX(5.2vmin)}
.stats{display:grid;grid-template-columns:1fr 1fr;gap:1.4vmin 4vmin;margin:0 0 3vmin;font-size:2.6vmin}
.stats b{display:block;font-size:3.6vmin;color:var(--gold)}
.confirm p{font-size:3.4vmin;text-align:center;font-weight:800;margin:0 0 3vmin}.confirm .menu{flex-direction:row;justify-content:center}

@keyframes pulse{0%,100%{transform:scale(1)}50%{transform:scale(1.06)}}
@keyframes toastIn{from{transform:translateY(-3vmin) scale(.6);opacity:0}}
@keyframes toastOut{to{transform:translateY(-2vmin);opacity:0}}
@keyframes popIn{from{transform:scale(.6);opacity:0}}
@keyframes fadeIn{from{opacity:0}}@keyframes fadeOut{to{opacity:0}}
@keyframes evoPop{0%{transform:scale(.2) rotate(-8deg);opacity:0}60%{transform:scale(1.15) rotate(2deg)}100%{transform:scale(1)}}
@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-1.2vmin)}}
@media (min-aspect-ratio:2/1){.quest{top:calc(var(--sat) + 20vmin)}}
`;
export const CSS2 = `
/* ── Dialogue ── */
.dlg{position:absolute;left:0;right:0;bottom:calc(var(--sab) + 3vmin);display:flex;justify-content:center;pointer-events:auto}
.dl-card{display:flex;gap:3vmin;align-items:flex-start;width:min(130vmin,92vw);background:linear-gradient(160deg,rgba(60,40,130,.95),rgba(24,14,60,.96));border:.7vmin solid #fff;border-radius:4vmin;
 padding:2.6vmin 3.4vmin;box-shadow:0 1vmin 0 var(--ink),0 0 6vmin rgba(160,123,255,.4);animation:popIn .35s var(--spring)}
.dlg.radio .dl-card{background:linear-gradient(160deg,rgba(20,70,90,.95),rgba(10,30,50,.96));border-style:dashed}
.dl-port{flex:none;width:15vmin;height:15vmin;border-radius:50%;display:grid;place-items:center;font-size:8vmin;background:radial-gradient(circle at 35% 30%,#fff,#c0a0ff);border:.6vmin solid #fff;box-shadow:0 .6vmin 0 var(--ink)}
.dlg.radio .dl-port{background:radial-gradient(circle at 35% 30%,#fff,#7fe8ff);animation:pulse 1s infinite}
.dl-body{flex:1;min-width:0}.dl-name{font-weight:900;font-size:3.4vmin;color:var(--gold);margin-bottom:.6vmin}
.dlg.radio .dl-name::after{content:" · radio";color:var(--cy);font-size:.7em}
.dl-text{font-size:3.4vmin;font-weight:700;line-height:1.35;min-height:9vmin}
.dl-choices{display:flex;flex-wrap:wrap;gap:1.4vmin;margin-top:1.6vmin}
.dl-choice{font:inherit;font-weight:900;font-size:3vmin;color:var(--ink);border:.5vmin solid var(--ink);border-radius:3vmin;padding:1vmin 3vmin;background:linear-gradient(180deg,#fff,#ffe98a 60%,#ffb02e);box-shadow:0 .6vmin 0 var(--ink)}
.dl-choice:active{transform:translateY(.5vmin);box-shadow:0 .1vmin 0 var(--ink)}
.dl-more{text-align:right;font-size:3vmin;animation:float 1s infinite}
/* ── Combat ── */
.vign{position:absolute;inset:0;box-shadow:inset 0 0 0 rgba(255,40,80,0);transition:box-shadow .5s}
.vign.hit{animation:hitFlash .45s ease-out}.vign.low{box-shadow:inset 0 0 12vmin rgba(255,40,80,.55);animation:lowPulse 1.2s infinite}
.combo{position:absolute;right:calc(var(--sar) + 6vmin);top:42%;font-weight:900;font-size:7vmin;color:#fff;transform:rotate(-8deg);
 background:linear-gradient(180deg,#fff,#ffe98a 50%,#ff7a3c);-webkit-background-clip:text;background-clip:text;color:transparent;filter:drop-shadow(0 .8vmin 0 var(--ink))}
.combo.pop{animation:evoPop .35s var(--spring)}
.bossbar{position:absolute;top:calc(var(--sat) + 19vmin);left:50%;transform:translateX(-50%);width:min(90vmin,60vw);text-align:center}
.bname{font-weight:900;font-size:4vmin;text-shadow:0 .4vmin 0 var(--ink)}.bname small{display:block;font-size:2.2vmin;color:var(--pk);letter-spacing:.2em;text-transform:uppercase}
.bbar{height:3.4vmin;margin-top:1vmin;border-radius:2vmin;background:rgba(10,6,30,.8);border:.5vmin solid #fff;overflow:hidden;box-shadow:0 .6vmin 0 var(--ink)}
.bbar i{display:block;height:100%;transform-origin:left;background:linear-gradient(90deg,#ff3b5c,#ff9a3c,#ffe98a);transition:transform .3s}
.bph{font-weight:900;font-size:2.4vmin;color:var(--gold);margin-top:.6vmin}.bossbar.shake{animation:shake .5s}
.ko{position:absolute;inset:0;display:grid;place-content:center;text-align:center;background:radial-gradient(circle,rgba(80,0,30,.35),rgba(10,0,10,.75));animation:fadeIn .4s}
.ko h1{margin:0;font-size:18vmin;font-weight:900;color:#ff4a7a;filter:drop-shadow(0 1.2vmin 0 var(--ink));animation:evoPop .6s var(--spring)}.ko p{font-size:4vmin;font-weight:900}
.b-special .cd{z-index:-0;pointer-events:none}.b-special .cd.noen{background:rgba(10,6,30,.55)}
.toast.loot{background:linear-gradient(90deg,#2ac87a,#3a8aff);font-size:2.6vmin}.toast.quest{background:linear-gradient(90deg,#a07bff,#ff6fb5)}
.toast.questDone{background:linear-gradient(90deg,#ffb02e,#2ac87a)}
/* ── Journal / inventaire ── */
.panel.wide{width:min(130vmin,90vw)}.menu2{display:flex;gap:2vmin}.menu2>*{flex:1;white-space:nowrap;padding:1.6vmin 2vmin}
.journal{display:flex;flex-direction:column;gap:2vmin;max-height:56vh;overflow:auto;padding-right:1vmin}
.qcard{background:rgba(255,255,255,.07);border:.4vmin solid rgba(255,255,255,.25);border-left:1vmin solid var(--vi);border-radius:2vmin;padding:1.6vmin 2.4vmin}
.qcard.main{border-left-color:var(--gold)}.qcard.secret{border-left-color:var(--pk)}.qcard.tracked{background:rgba(255,215,106,.14);border-color:var(--gold)}
.qh{display:flex;justify-content:space-between;align-items:baseline;gap:2vmin;font-size:3.2vmin}.qh small{opacity:.7;font-size:2.2vmin}
.qd{font-size:2.4vmin;opacity:.8;margin:.6vmin 0}.qs{font-size:2.6vmin;font-weight:700}.qs .ok{opacity:.55}.qs .cur{color:var(--gold)}.qs .todo{opacity:.4}
.qdone,.empty{text-align:center;opacity:.7;font-size:2.4vmin}
.inv{display:grid;grid-template-columns:repeat(auto-fill,minmax(18vmin,1fr));gap:2vmin;max-height:56vh;overflow:auto}
.slot{background:rgba(255,255,255,.08);border:.4vmin solid rgba(255,255,255,.3);border-radius:2.4vmin;padding:1.4vmin;text-align:center;position:relative}
.slot .ic{display:block;font-size:6vmin}.slot b{position:absolute;top:.8vmin;right:1.2vmin;font-size:2.8vmin;color:var(--gold)}.slot small{display:block;font-size:1.9vmin;font-weight:800;opacity:.85}
.bgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(36vmin,1fr));gap:2vmin;max-height:58vh;overflow:auto;padding:.5vmin}
.bgrid.small{grid-template-columns:repeat(auto-fill,minmax(30vmin,1fr));max-height:34vh}
.bcard{position:relative;background:rgba(255,255,255,.08);border:.5vmin solid rgba(255,255,255,.35);border-radius:3vmin;padding:1.6vmin 2vmin 1.6vmin 10vmin;min-height:14vmin;transition:transform .12s var(--spring)}
.bcard:active{transform:scale(.97)}.bcard .bi{position:absolute;left:1.6vmin;top:1.6vmin;font-size:6.5vmin}
.bcard b{display:block;font-size:3vmin}.bcard small{display:block;font-size:2.1vmin;opacity:.8;margin:.4vmin 0}.bcard .eff{font-size:2.1vmin;color:var(--cy);font-weight:800}
.bcard .costs{margin-top:.8vmin;display:flex;flex-wrap:wrap;gap:1vmin}.cost{background:rgba(0,0,0,.3);border-radius:2vmin;padding:.2vmin 1.4vmin;font-size:2.3vmin;font-weight:900}
.bcard .why{margin-top:.6vmin;font-size:2vmin;font-weight:900;color:#ffb0c0}.bgrid .empty{grid-column:1/-1}
.bcard.poor{opacity:.75}.bcard.locked{opacity:.45;filter:grayscale(.7)}.bcard.built{border-color:#2ac87a}
.base .stats{grid-template-columns:repeat(4,1fr)}
.codex-tabs{justify-content:center;margin-bottom:2vmin;flex-wrap:wrap}.cx-count{text-align:right;font-size:2.4vmin;opacity:.7;margin-bottom:1vmin}
.cxgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(30vmin,1fr));gap:1.6vmin;max-height:50vh;overflow:auto}
.cx{position:relative;padding:1.4vmin 1.6vmin 1.4vmin 9vmin;min-height:11vmin;background:rgba(255,255,255,.03);border:.2vmin solid rgba(255,255,255,.14);border-radius:1vmin}
.cx .g{position:absolute;left:1.6vmin;top:1.4vmin;font-size:5.5vmin;color:var(--neo);text-shadow:0 0 1.4vmin var(--neo)}.cx b{display:block;font-size:2.7vmin}.cx small{display:block;font-size:2vmin;opacity:.75}
.cx.unk{opacity:.4}.cx.unk .g{color:#888;text-shadow:none}.cx.r-rare{border-color:rgba(255,60,200,.6)}.cx.r-rare .g{color:var(--neo2);text-shadow:0 0 1.4vmin var(--neo2)}
.cx.r-leg{border-color:rgba(255,204,58,.7);box-shadow:0 0 1.4vmin rgba(255,204,58,.25)}.cx.r-leg .g{color:var(--neo3);text-shadow:0 0 1.4vmin var(--neo3)}
.galaxy{display:flex;gap:2vmin;align-items:stretch}.gmap{flex:1;min-width:0;max-height:56vh;background:radial-gradient(ellipse,rgba(40,20,80,.35),rgba(0,0,0,.6));border:.2vmin solid rgba(63,240,255,.25);border-radius:1vmin}
.gsys{font-size:22px;font-weight:700;letter-spacing:4px;text-anchor:middle;opacity:.8}.gname{font-size:18px;fill:#dfe6ff;text-anchor:middle;font-weight:600}
.gp{cursor:pointer}.gp circle{stroke-width:3;filter:drop-shadow(0 0 6px rgba(63,240,255,.5))}.gp.unk{opacity:.35;cursor:default}.gp.here circle{stroke:#ffcc3a;stroke-width:5}
.gring{fill:none;stroke:#ffcc3a;stroke-width:3;stroke-dasharray:8 6;animation:spin 6s linear infinite;transform-box:fill-box;transform-origin:center}
.ginfo{width:34vmin;flex:none;display:flex;flex-direction:column;gap:1vmin;font-size:2.4vmin}.ginfo b{font-size:3.6vmin;text-transform:uppercase;letter-spacing:.12em;color:var(--neo)}.ginfo p{margin:0;opacity:.85}.ginfo small{opacity:.7}
.ginfo .cbtn{margin-top:auto;font-size:2.8vmin;padding:1.4vmin 2vmin}
#ui .mgrid{display:grid;grid-template-columns:1fr 1fr;gap:1.4vmin}#ui .mgrid .cbtn{font-size:2.7vmin;padding:1.2vmin 1.4vmin;white-space:nowrap}
@keyframes spin{to{transform:rotate(360deg)}}
@keyframes hitFlash{0%{box-shadow:inset 0 0 14vmin rgba(255,40,80,.85)}100%{box-shadow:inset 0 0 0 rgba(255,40,80,0)}}
@keyframes lowPulse{50%{box-shadow:inset 0 0 18vmin rgba(255,40,80,.7)}}
@keyframes shake{20%{transform:translateX(-52%)}40%{transform:translateX(-48%)}60%{transform:translateX(-51%)}80%{transform:translateX(-49%)}}
`;
// ── Thème SOMBRE / néon (consigne utilisateur: « moins bébé, plus sombre et stylé »). Surcharge les styles ci-dessus. ──
export const CSS_DARK = `
:root{--ink:#000;--neo:#3ff0ff;--neo2:#ff3cc8;--neo3:#ffcc3a;--bad:#ff2a55;--panel:rgba(6,5,16,.88);--panel2:rgba(14,10,32,.92);--edge:rgba(63,240,255,.55);
 --font:"Rajdhani","Exo 2","Orbitron",ui-sans-serif,system-ui,sans-serif;--glowc:0 0 1.6vmin rgba(63,240,255,.45)}
#ui{letter-spacing:.02em}
.txt-out{text-shadow:0 0 1vmin rgba(0,0,0,.9),0 .2vmin .4vmin #000}
/* HUD */
.portrait{background:radial-gradient(circle at 50% 60%,var(--c2,#3a6aff)22,#05040c 70%);border:.35vmin solid var(--c1,var(--neo));box-shadow:0 0 2vmin var(--c1,var(--neo)),inset 0 0 2vmin var(--c2,#3a6aff);color:var(--c1);text-shadow:0 0 1.2vmin var(--c1)}
.portrait b{background:#05040c;color:var(--neo3);border:.25vmin solid var(--neo3);box-shadow:0 0 1vmin rgba(255,204,58,.5);font-weight:800;letter-spacing:.1em;text-transform:uppercase;font-size:2vmin}
.bars .nm{text-transform:uppercase;letter-spacing:.12em;font-size:3vmin}
.bar{background:rgba(0,0,0,.65);border:.2vmin solid rgba(255,255,255,.18);border-radius:.6vmin;box-shadow:none;height:2vmin}
.bar i{border-radius:.4vmin}.bar i::after{display:none}
.bar.hp i{background:linear-gradient(90deg,#8a0a2a,#ff2a55);box-shadow:0 0 1.2vmin #ff2a55}
.bar.en i{background:linear-gradient(90deg,#0a4a8a,#3ff0ff);box-shadow:0 0 1.2vmin #3ff0ff}
.bar.xp{height:1.3vmin}.bar.xp i{background:linear-gradient(90deg,#8a5a0a,#ffcc3a);box-shadow:0 0 1vmin #ffcc3a}
.xpline{opacity:.7;font-weight:600;letter-spacing:.06em}
.chip,.hint,.quest{background:var(--panel);border:.2vmin solid rgba(255,255,255,.14);backdrop-filter:blur(8px)}
.ibtn{background:var(--panel);border:.25vmin solid var(--edge);box-shadow:var(--glowc);color:var(--neo)}
.ibtn:active{box-shadow:0 0 3vmin var(--neo)}
.loc{text-transform:uppercase;letter-spacing:.2em;font-size:3.2vmin}.loc small{letter-spacing:.05em;text-transform:none;font-weight:500}
.hint.go{background:rgba(6,5,16,.9);border-color:var(--neo3);color:var(--neo3);box-shadow:0 0 2vmin rgba(255,204,58,.45)}
.quest{border-left:.5vmin solid var(--neo3);border-radius:.8vmin}.quest h4{letter-spacing:.25em}
.toast{background:var(--panel2);border:.2vmin solid var(--edge);border-radius:.8vmin;box-shadow:var(--glowc);font-weight:700;letter-spacing:.04em}
.toast.discover{background:var(--panel2);border-color:var(--neo);color:#cffaff}.toast.xp{background:var(--panel);border-color:rgba(255,204,58,.6);color:var(--neo3);box-shadow:0 0 1.4vmin rgba(255,204,58,.35)}
.toast.warn{background:var(--panel2);border-color:var(--bad);color:#ffc0cc;box-shadow:0 0 1.6vmin rgba(255,42,85,.45)}
.toast.loot{background:var(--panel2);border-color:#3aff9a;color:#c8ffe0}.toast.quest{background:var(--panel2);border-color:var(--neo2);color:#ffd0f4;box-shadow:0 0 1.6vmin rgba(255,60,200,.4)}
.toast.questDone{background:var(--panel2);border-color:var(--neo3);color:#fff0c0;box-shadow:0 0 1.6vmin rgba(255,204,58,.45)}
.evo{background:radial-gradient(circle,rgba(63,240,255,.12),transparent 60%)}
.evo h1{background:linear-gradient(180deg,#fff,#3ff0ff 55%,#ff3cc8);-webkit-background-clip:text;background-clip:text;filter:drop-shadow(0 0 2.4vmin rgba(63,240,255,.8));letter-spacing:.25em;font-size:9vmin}
.evo h2{text-transform:uppercase;letter-spacing:.2em}.evo .ab{background:var(--panel2);border:.25vmin solid var(--neo3);border-radius:.8vmin;box-shadow:0 0 2vmin rgba(255,204,58,.4)}
/* contrôles */
.joy{background:radial-gradient(circle,rgba(63,240,255,.07),rgba(0,0,0,.35) 70%);border:.3vmin solid rgba(63,240,255,.4);box-shadow:inset 0 0 3vmin rgba(63,240,255,.15),0 0 2vmin rgba(63,240,255,.15)}
.joy i{background:radial-gradient(circle at 50% 40%,#1a2a4a,#05040c);border:.35vmin solid var(--neo);box-shadow:0 0 2vmin var(--neo),inset 0 0 1.4vmin rgba(63,240,255,.5)}
.btn{background:radial-gradient(circle at 50% 35%,rgba(30,24,60,.92),rgba(4,3,12,.92))!important;border:.35vmin solid var(--glow,var(--neo));box-shadow:0 0 1.8vmin var(--glow,var(--neo)),inset 0 0 1.6vmin rgba(255,255,255,.06);color:var(--glow,var(--neo));text-shadow:0 0 1.2vmin var(--glow,var(--neo))}
.btn.on{transform:scale(.9);box-shadow:0 0 3.6vmin var(--glow,var(--neo)),inset 0 0 3vmin var(--glow,var(--neo))}
.b-attack{--glow:#ff2a55}.b-jump{--glow:#3aff9a}.b-boost{--glow:#3ff0ff}.b-special{--glow:#ffcc3a}.b-action{--glow:#c08aff;color:#e8d8ff;font-weight:700;letter-spacing:.06em;animation:none}
/* écrans */
.screen{background:radial-gradient(ellipse at center,rgba(4,3,12,.55),rgba(0,0,0,.92))}
.panel{background:linear-gradient(160deg,rgba(14,10,32,.96),rgba(4,3,10,.97));border:.25vmin solid var(--edge);border-radius:1.4vmin;box-shadow:0 0 4vmin rgba(63,240,255,.18),inset 0 0 6vmin rgba(63,240,255,.04)}
.panel h2{text-transform:uppercase;letter-spacing:.3em;font-weight:700;font-size:5vmin;color:#e8fbff;text-shadow:0 0 2vmin rgba(63,240,255,.6)}
.cbtn{background:linear-gradient(180deg,rgba(63,240,255,.16),rgba(63,240,255,.04));color:#dffcff;border:.25vmin solid var(--neo);border-radius:.9vmin;box-shadow:0 0 1.6vmin rgba(63,240,255,.3);
 font-weight:700;letter-spacing:.14em;text-transform:uppercase;font-size:3.4vmin}
.cbtn:active{transform:scale(.97);box-shadow:0 0 3vmin rgba(63,240,255,.6)}
.cbtn.alt{background:linear-gradient(180deg,rgba(255,60,200,.14),rgba(255,60,200,.03));border-color:var(--neo2);color:#ffe0f6;box-shadow:0 0 1.6vmin rgba(255,60,200,.28)}
.cbtn.ghost{background:transparent;border-color:rgba(255,255,255,.3);color:#cfd3e8;box-shadow:none}
.cbtn.danger{background:linear-gradient(180deg,rgba(255,42,85,.18),rgba(255,42,85,.04));border-color:var(--bad);color:#ffd0da;box-shadow:0 0 1.6vmin rgba(255,42,85,.3)}
.title{background:radial-gradient(ellipse at 50% 40%,rgba(10,6,30,.15),rgba(0,0,0,.82))}
.logo{background:linear-gradient(180deg,#ffffff 15%,#3ff0ff 55%,#ff3cc8 95%);-webkit-background-clip:text;background-clip:text;filter:drop-shadow(0 0 3vmin rgba(63,240,255,.6));letter-spacing:.12em;font-weight:800;font-size:12vmin}
.logo span{letter-spacing:.55em;font-size:.42em;margin-top:1vmin;filter:drop-shadow(0 0 1.4vmin rgba(255,60,200,.8))}
.tag{text-transform:uppercase;letter-spacing:.35em;font-weight:600;font-size:2.6vmin;opacity:.8;margin-top:3vmin}
.title{background:radial-gradient(ellipse 60% 55% at 50% 45%,rgba(0,0,0,.6),rgba(0,0,0,.25) 70%,rgba(0,0,0,.8))}
.row{border-bottom:.2vmin solid rgba(63,240,255,.12);font-weight:600;letter-spacing:.04em}
.seg button{border-radius:.7vmin;background:transparent;border-color:rgba(255,255,255,.25)}.seg button.sel{background:rgba(255,204,58,.15);color:var(--neo3);border-color:var(--neo3);box-shadow:0 0 1.2vmin rgba(255,204,58,.4)}
input[type=range]{background:rgba(255,255,255,.08);border:.2vmin solid rgba(63,240,255,.35);height:1.4vmin}
input[type=range]::-webkit-slider-thumb{background:#05040c;border:.35vmin solid var(--neo);box-shadow:0 0 1.4vmin var(--neo);width:4.4vmin;height:4.4vmin}
.tog{border-radius:.9vmin;background:rgba(255,255,255,.06);border-color:rgba(255,255,255,.3)}.tog::after{border-radius:.6vmin;background:#9aa0b8}
.tog.on{background:rgba(58,255,154,.15);border-color:#3aff9a;box-shadow:0 0 1.4vmin rgba(58,255,154,.4)}.tog.on::after{background:#3aff9a}
.stats b{color:var(--neo3);text-shadow:0 0 1vmin rgba(255,204,58,.5)}
/* dialogue */
.dl-card{background:linear-gradient(160deg,rgba(14,10,32,.96),rgba(4,3,10,.97));border:.25vmin solid rgba(255,60,200,.55);border-radius:1.2vmin;box-shadow:0 0 3vmin rgba(255,60,200,.2)}
.dlg.radio .dl-card{background:linear-gradient(160deg,rgba(4,20,30,.96),rgba(2,6,12,.97));border:.25vmin dashed rgba(63,240,255,.6);box-shadow:0 0 3vmin rgba(63,240,255,.2)}
.dl-port{background:radial-gradient(circle,#1a1430,#05040c);border:.3vmin solid var(--neo2);box-shadow:0 0 2vmin rgba(255,60,200,.5);filter:saturate(.55) contrast(1.1)}
.dlg.radio .dl-port{background:radial-gradient(circle,#0a2030,#05040c);border-color:var(--neo);box-shadow:0 0 2vmin rgba(63,240,255,.5)}
.dl-name{color:var(--neo2);text-transform:uppercase;letter-spacing:.18em;font-size:2.6vmin}.dlg.radio .dl-name{color:var(--neo)}
.dl-text{font-weight:500;color:#e8ecff}
.dl-choice{background:rgba(255,60,200,.08);color:#ffe0f6;border:.25vmin solid var(--neo2);border-radius:.8vmin;box-shadow:0 0 1.2vmin rgba(255,60,200,.3);font-weight:600;letter-spacing:.04em}
/* combat, journal, base */
.combo{background:linear-gradient(180deg,#fff,#ffcc3a 50%,#ff2a55);-webkit-background-clip:text;background-clip:text;filter:drop-shadow(0 0 1.6vmin rgba(255,42,85,.8));letter-spacing:.1em}
.bname{text-transform:uppercase;letter-spacing:.25em;text-shadow:0 0 2vmin rgba(255,42,85,.8)}.bname small{color:var(--bad)}
.bbar{border:.2vmin solid rgba(255,42,85,.7);border-radius:.6vmin;box-shadow:0 0 2vmin rgba(255,42,85,.4);height:2.4vmin;background:rgba(0,0,0,.7)}
.bbar i{background:linear-gradient(90deg,#5a0010,#ff2a55,#ff8a3a)}
.ko h1{color:var(--bad);filter:drop-shadow(0 0 3vmin rgba(255,42,85,.9));letter-spacing:.2em}
.qcard,.slot,.bcard{background:rgba(255,255,255,.03);border:.2vmin solid rgba(255,255,255,.14);border-radius:1vmin}
.qcard{border-left:.5vmin solid var(--neo2)}.qcard.main{border-left-color:var(--neo3)}.qcard.secret{border-left-color:var(--neo)}
.qcard.tracked{background:rgba(255,204,58,.06);border-color:rgba(255,204,58,.6);box-shadow:0 0 1.6vmin rgba(255,204,58,.2)}
.qs .cur{color:var(--neo3)}.slot b{color:var(--neo3)}.bcard .eff{color:var(--neo)}.bcard.built{border-color:#3aff9a;box-shadow:0 0 1.2vmin rgba(58,255,154,.25)}
.cost{background:rgba(63,240,255,.07);border:.15vmin solid rgba(63,240,255,.3);border-radius:.6vmin}
`;
