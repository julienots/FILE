'use strict';
// ====== Front de Fer — jeu : soldats, IA, armes, effets, sons, commandes, lobby ======
const G={state:'lobby',time:600,sA:0,sB:0,limit:40,diff:1,primary:'smg',sens:1,t:0,noise:null};
const DIFFS=[{acc:.55,react:.7,dmg:.7},{acc:1,react:1,dmg:1},{acc:1.5,react:1.6,dmg:1.25}];
const W={
 smg:{name:'Pistolet-mitrailleur',mag:32,res:160,rpm:560,dmg:27,head:2,spread:.024,ads:.007,rec:.011,reload:2.2,auto:true,fov:52,snd:'smg',icon:'M4 12h20l3-3h50v-3h4v3h30v4H84v3H58l-3 20h-7l2-20h-9l-5 9h-7l3-9H22l-12 8H4z'},
 rifle:{name:'Fusil semi-auto',mag:8,res:64,rpm:260,dmg:58,head:2.4,spread:.03,ads:.002,rec:.03,reload:2.4,auto:false,fov:46,snd:'rifle',icon:'M2 16l22-4h30l2-3h10l2 3h48v3H70l-2 4H44l-8 3-4 10h-8l3-11-23 3z'},
 sniper:{name:'Fusil à lunette',mag:5,res:35,rpm:48,dmg:115,head:3,spread:.06,ads:.0003,rec:.07,reload:3,auto:false,fov:17,scope:true,snd:'rifle',icon:'M2 20l22-4h30l2-3h10l2 3h48v3H70l-2 4H44l-8 3-4 10h-8l3-11-23 3zM40 6h34v6H40z'},
 pistol:{name:'Pistolet',mag:8,res:48,rpm:330,dmg:34,head:2,spread:.02,ads:.006,rec:.024,reload:1.6,auto:false,fov:58,snd:'pistol',icon:'M30 10h56v10H62l-3 5h-9l-6 13H32l5-18h-7z'}};
// ---------- sons (synthèse) ----------
const SND={ac:null};
function audioInit(){if(SND.ac){SND.ac.resume&&SND.ac.resume();return}try{const AC=window.AudioContext||window.webkitAudioContext,ac=SND.ac=new AC(),len=ac.sampleRate*2,buf=ac.createBuffer(1,len,ac.sampleRate),d=buf.getChannelData(0);
 for(let i=0;i<len;i++)d[i]=Math.random()*2-1;SND.noise=buf;SND.master=ac.createGain();SND.master.gain.value=.85;const cp=ac.createDynamicsCompressor();SND.master.connect(cp);cp.connect(ac.destination);
 const s=ac.createBufferSource();s.buffer=buf;s.loop=true;const f=ac.createBiquadFilter();f.type='lowpass';f.frequency.value=380;const g=ac.createGain();g.gain.value=.05;s.connect(f);f.connect(g);g.connect(SND.master);s.start()}catch(e){SND.ac=null}}
function nz(vol,freq,dur,o){const ac=SND.ac;if(!ac||vol<.003)return;o=o||{};const t=ac.currentTime+(o.delay||0),s=ac.createBufferSource();s.buffer=SND.noise;s.playbackRate.value=o.rate||1;
 const f=ac.createBiquadFilter();f.type=o.type||'lowpass';f.frequency.value=freq;f.Q.value=o.q||.7;const g=ac.createGain();g.gain.setValueAtTime(.0001,t);g.gain.exponentialRampToValueAtTime(Math.max(vol,.0002),t+(o.att||.002));g.gain.exponentialRampToValueAtTime(.0001,t+dur);
 s.connect(f);f.connect(g);g.connect(SND.master);s.start(t,Math.random()*1.5);s.stop(t+dur+.05)}
function tone(vol,f0,f1,dur,delay,type){const ac=SND.ac;if(!ac||vol<.003)return;const t=ac.currentTime+(delay||0),o=ac.createOscillator(),g=ac.createGain();o.type=type||'sine';o.frequency.setValueAtTime(f0,t);o.frequency.exponentialRampToValueAtTime(f1,t+dur);
 g.gain.setValueAtTime(Math.max(vol,.0002),t);g.gain.exponentialRampToValueAtTime(.0001,t+dur);o.connect(g);g.connect(SND.master);o.start(t);o.stop(t+dur+.02)}
function sGun(dist,kind){const near=1/(1+dist/14),v=Math.min(1,near*1.15),dl=Math.min(dist/340,.6),big=kind==='rifle'?1:kind==='pistol'?.6:.8;
 nz(v*big,Math.max(450,8500*near),.1+(1-near)*.3+(kind==='rifle'?.12:0),{delay:dl});if(dist<30)tone(v*.9*big,kind==='rifle'?150:kind==='pistol'?190:120,42,.16,dl);
 nz(v*.28*big,Math.max(300,2600*near),.55+(1-near)*.7,{delay:dl+.03,att:.03})}
function sExpl(dist){const near=1/(1+dist/22),dl=Math.min(dist/340,.6);nz(near*1.3,Math.max(250,1900*near),1.5,{delay:dl});tone(near*1.2,95,24,.75,dl);nz(near*.5,500,2.6,{delay:dl+.1,att:.2})}
const sWhiz=()=>nz(.35,R(2500,4500),.13,{type:'bandpass',q:6,rate:R(1.2,1.9)});
const sClick=(v,f)=>nz(v||.25,f||3200,.05,{type:'bandpass',q:3});
const sHit=()=>{nz(.3,1900,.06,{type:'bandpass',q:4});tone(.16,900,500,.07)};
const sImpact=d=>nz(.4/(1+d/6),R(1200,2600),.09,{type:'bandpass',q:1.5});
const sStep=()=>nz(.055,R(260,480),.09);
// ---------- effets ----------
const COL={dust:C(0x9a8262),smoke:C(0x57524a),dark:C(0x1d1b19),blood:C(0x74100e),fire:C(0xffa544),spark:C(0xffe2a8),white:C(0xffffff),wood:C(0xa08258),stone:C(0xb0aca2),steam:C(0xd8d4c8)};
const FXN=coarse?150:260,fx=[];let fxi=0;
for(let i=0;i<FXN;i++){const m=new THREE.SpriteMaterial({map:T.puff,transparent:true,depthWrite:false,opacity:0}),s=new THREE.Sprite(m);s.visible=false;scene.add(s);fx.push({s,m,life:0,max:1,v:new V3(),g:0,s0:1,s1:1,o:1,drag:0})}
function puff(map,x,y,z,vx,vy,vz,life,s0,s1,col,op,grav,add,drag){const p=fx[fxi=(fxi+1)%FXN];p.s.visible=true;p.s.position.set(x,y,z);p.v.set(vx,vy,vz);p.life=p.max=life;p.s0=s0;p.s1=s1;p.o=op;p.g=grav||0;p.drag=drag||0;
 p.m.map=map;p.m.color.copy(col);p.m.blending=add?THREE.AdditiveBlending:THREE.NormalBlending;p.m.rotation=Math.random()*6.28;p.s.scale.set(s0,s0,1);p.m.opacity=op}
function updFx(dt){for(let i=0;i<FXN;i++){const p=fx[i];if(p.life<=0)continue;p.life-=dt;if(p.life<=0){p.s.visible=false;continue}const k=1-p.life/p.max;p.s.position.addScaledVector(p.v,dt);p.v.y-=p.g*dt;if(p.drag)p.v.multiplyScalar(Math.max(0,1-p.drag*dt));
  if(p.g>3){const gy=H(p.s.position.x,p.s.position.z);if(p.s.position.y<gy){p.s.position.y=gy;p.v.set(0,0,0)}}const s=lerp(p.s0,p.s1,k);p.s.scale.set(s,s,1);p.m.opacity=p.o*(1-k*k)}}
const TRN=28,trc=[];let tri=0;{const g=new THREE.BoxGeometry(.03,.03,1);for(let i=0;i<TRN;i++){const m=new THREE.Mesh(g,new THREE.MeshBasicMaterial({color:0xffdc9a,transparent:true,opacity:.85,blending:THREE.AdditiveBlending,depthWrite:false,fog:false}));m.visible=false;scene.add(m);trc.push({m,a:new V3(),d:new V3(),len:0,head:0})}}
function tracer(a,b){const t=trc[tri=(tri+1)%TRN];t.a.copy(a);t.d.subVectors(b,a);t.len=t.d.length();if(t.len<2)return;t.d.divideScalar(t.len);t.head=0;t.m.visible=true}
const _v=new V3(),_v2=new V3(),_v3=new V3();
function updTr(dt){for(const t of trc){if(!t.m.visible)continue;t.head+=420*dt;const h=Math.min(t.head,t.len),ta=Math.max(0,t.head-14);if(ta>=t.len){t.m.visible=false;continue}
  t.m.position.copy(t.a).addScaledVector(t.d,(h+ta)/2);t.m.lookAt(_v.copy(t.a).addScaledVector(t.d,t.len+5));t.m.scale.set(1,1,Math.max(.1,h-ta))}}
const flashL=new THREE.PointLight(C(0xffc070),0,16,2),explL=new THREE.PointLight(C(0xff9a48),0,45,2);scene.add(flashL,explL);
const DCN=40,dcs=[];let dci=0;for(let i=0;i<DCN;i++){const s=new THREE.Sprite(new THREE.SpriteMaterial({map:T.glow,color:0x000000,transparent:true,depthWrite:false,opacity:.75}));s.visible=false;s.scale.set(.09,.09,1);scene.add(s);dcs.push(s)}
function impact(x,y,z,mat,dist){const up=mat==='dirt'?1:.3;
 if(mat==='dirt'){for(let i=0;i<4;i++)puff(T.puff,x,y+.05,z,R(-.6,.6),R(1,2.6),R(-.6,.6),R(.4,.8),.15,R(.6,1.1),COL.dust,.6,2.5,false,1.5);for(let i=0;i<3;i++)puff(T.glow,x,y,z,R(-2,2),R(2,5),R(-2,2),.6,.05,.03,COL.dark,.9,12)}
 else{const c=mat==='wood'?COL.wood:mat==='sand'?COL.dust:COL.stone;for(let i=0;i<3;i++)puff(T.puff,x,y,z,R(-.8,.8),R(.1,1)*up+.3,R(-.8,.8),R(.3,.6),.1,R(.4,.8),c,.55,.5,false,2);
  for(let i=0;i<4;i++)puff(T.glow,x,y,z,R(-3,3),R(0,4),R(-3,3),.5,.045,.02,mat==='wood'?COL.wood:COL.dark,.9,12);
  if(mat==='metal'||mat==='stone')for(let i=0;i<5;i++)puff(T.glow,x,y,z,R(-6,6),R(-1,6),R(-6,6),R(.12,.3),.07,.02,COL.spark,1,9,true);
  const d=dcs[dci=(dci+1)%DCN];d.position.set(x,y,z);d.visible=true}
 if(dist<25)sImpact(dist)}
function blood(x,y,z,dx,dz){for(let i=0;i<5;i++)puff(T.puff,x,y,z,dx*R(.5,2.5)+R(-.7,.7),R(-.2,1.4),dz*R(.5,2.5)+R(-.7,.7),R(.25,.5),.08,R(.3,.55),COL.blood,.8,5,false,2)}
let shake=0;
function explode(p,owner){const d=cam.position.distanceTo(p);sExpl(d);shake=Math.max(shake,clamp(1.6-d/22,0,1.4));explL.position.copy(p).y+=1;explL.intensity=60;
 puff(T.glow,p.x,p.y+.6,p.z,0,0,0,.12,3,9,COL.spark,1,0,true);for(let i=0;i<10;i++)puff(T.puff,p.x+R(-.6,.6),p.y+R(.2,1),p.z+R(-.6,.6),R(-3,3),R(1,6),R(-3,3),R(.25,.5),.8,R(2.2,3.6),COL.fire,.95,0,true,3);
 for(let i=0;i<12;i++)puff(T.puff,p.x+R(-1,1),p.y+R(.2,1.2),p.z+R(-1,1),R(-2.2,2.2),R(1.5,5),R(-2.2,2.2),R(1.8,3.6),1,R(4,7),i%3?COL.smoke:COL.dark,.7,-.25,false,1.6);
 for(let i=0;i<18;i++)puff(T.glow,p.x,p.y+.2,p.z,R(-9,9),R(4,14),R(-9,9),R(.8,1.5),R(.07,.16),.05,COL.dark,1,16);
 for(let i=0;i<8;i++)puff(T.puff,p.x,p.y+.1,p.z,Math.cos(i*.785)*7,.4,Math.sin(i*.785)*7,.7,.6,2.6,COL.dust,.5,0,false,3.5);
 for(const e of ents){if(!e.alive||(e.team===owner.team&&e!==owner))continue;const ey=e.pos.y+1,dd=Math.hypot(e.pos.x-p.x,ey-p.y,e.pos.z-p.z);if(dd>8.5)continue;if(dd>1.5&&!los(p.x,p.y+.5,p.z,e.pos.x,ey,e.pos.z))continue;
  damage(e,150*Math.pow(1-dd/8.5,1.2),owner,false,p)}}
// ---------- soldats : squelette articulé à géométrie fusionnée ----------
const solMat=SM({vertexColors:true,map:T.cloth,roughness:.85});
function part(geo,col,x,y,z,rx,ry,rz,sx,sy,sz){const g=geo.index?geo.toNonIndexed():geo.clone();E.set(rx||0,ry||0,rz||0,'XYZ');Q.setFromEuler(E);M4.compose(new V3(x||0,y||0,z||0),Q,new V3(sx||1,sy||1,sz||1));g.applyMatrix4(M4);
 const n=g.attributes.position.count,c=new Float32Array(n*3);for(let i=0;i<n;i++){c[i*3]=col.r;c[i*3+1]=col.g;c[i*3+2]=col.b}g.setAttribute('color',new THREE.BufferAttribute(c,3));return g}
const cylG=(a,b,h,s)=>new THREE.CylinderGeometry(a,b,h,s||8),boxG=(w,h,d)=>new THREE.BoxGeometry(w,h,d),sphG=(r,a,b)=>new THREE.SphereGeometry(r,a||10,b||8);
const KIT={A:{jk:C(0x767a50),pt:C(0x857c5c),hm:C(0x545c3a),pk:C(0x8a8660),bt:C(0x4a3626),lt:C(0x5c4a34)},B:{jk:C(0x6f766d),pt:C(0x666c63),hm:C(0x4e5553),pk:C(0x7a6850),bt:C(0x252120),lt:C(0x2e2824)}};
const SKIN=C(0xc49878),SKD=C(0x8a6650),STEEL=C(0x2e3033),WOODC=C(0x6e4a2c),GEO={};
function soldierGeo(team){if(GEO[team])return GEO[team];const k=KIT[team],g={};
 g.hips=mergeG([part(cylG(.15,.155,.22,10),k.pt,0,-.02,0,0,0,0,1,1,.72),part(cylG(.167,.167,.055,10),k.lt,0,.085,0,0,0,0,1,1,.74),part(boxG(.09,.1,.05),k.lt,.09,.03,.125),part(boxG(.09,.1,.05),k.lt,-.09,.03,.125),part(sphG(.055,8,6),k.hm,-.16,-.03,-.07,0,0,0,.7,1.2,1)]);
 g.chest=mergeG([part(cylG(.185,.15,.46,10),k.jk,0,.25,0,0,0,0,1,1,.68),part(sphG(.078,8,6),k.jk,.2,.44,0),part(sphG(.078,8,6),k.jk,-.2,.44,0),part(cylG(.05,.062,.09,8),SKIN,0,.515,0),
  part(boxG(.035,.42,.02),k.lt,.09,.27,.123,0,0,-.1),part(boxG(.035,.42,.02),k.lt,-.09,.27,.123,0,0,.1),part(boxG(.3,.34,.13),k.pk,0,.27,-.175),part(cylG(.05,.05,.34,8),k.hm,0,.47,-.175,0,0,Math.PI/2),part(boxG(.3,.03,.14),k.lt,0,.2,-.175)]);
 const helm=team==='A'?[part(new THREE.SphereGeometry(.128,12,6,0,6.283,0,Math.PI/2),k.hm,0,.115,0,0,0,0,1,.92,1.12),part(cylG(.142,.142,.012,12),k.hm,0,.115,0,0,0,0,1,1,1.12)]
  :[part(new THREE.SphereGeometry(.13,12,6,0,6.283,0,Math.PI/2),k.hm,0,.125,0,0,0,0,1,1,1.1),part(new THREE.CylinderGeometry(.13,.158,.075,12,1,true),k.hm,0,.09,-.012,0,0,0,1,1,1.12),part(boxG(.2,.012,.05),k.hm,0,.122,.125)];
 g.head=mergeG([part(sphG(.1,10,8),SKIN,0,.09,0,0,0,0,.9,1.1,1),part(boxG(.022,.04,.03),SKIN,0,.075,.098),part(boxG(.115,.016,.02),SKD,0,.108,.088),part(boxG(.05,.012,.02),SKD,0,.03,.09)].concat(helm));
 g.arm=mergeG([part(cylG(.052,.045,.3),k.jk,0,-.15,0)]);g.fore=mergeG([part(cylG(.045,.037,.26),k.jk,0,-.13,0),part(sphG(.042,8,6),SKIN,0,-.285,0,0,0,0,.85,1.2,.7)]);
 g.thigh=mergeG([part(cylG(.085,.062,.44),k.pt,0,-.22,0)]);
 g.shin=mergeG([part(cylG(.06,.048,.4),k.pt,0,-.2,0),part(cylG(.054,.052,team==='B'?.26:.14),k.bt,0,team==='B'?-.29:-.35,0),part(boxG(.095,.09,.25),k.bt,0,-.43,.045)]);
 g.rifle=mergeG([part(boxG(.04,.1,.3),WOODC,0,-.045,-.16,-.12),part(boxG(.04,.06,.52),WOODC,0,0,.2),part(cylG(.011,.011,.5),STEEL,0,.018,.66,Math.PI/2),part(boxG(.044,.05,.18),STEEL,0,.03,.06),part(boxG(.008,.03,.008),STEEL,0,.04,.88)]);
 g.smg=mergeG([part(cylG(.022,.022,.36,8),STEEL,0,.02,.12,Math.PI/2),part(cylG(.011,.011,.22),STEEL,0,.02,.4,Math.PI/2),part(boxG(.022,.19,.035),STEEL,0,-.09,.22),part(boxG(.03,.09,.04),WOODC,0,-.05,0,.25),part(boxG(.02,.03,.26),STEEL,0,-.01,-.2),part(boxG(.03,.07,.02),STEEL,0,-.03,-.33)]);
 return GEO[team]=g}
function makeSoldier(team,wpn){const g=soldierGeo(team),mk=(geo,par,x,y,z)=>{const m=new THREE.Mesh(geo,solMat);m.position.set(x||0,y||0,z||0);m.castShadow=true;par.add(m);return m};
 const root=new THREE.Group(),body=new THREE.Group();root.rotation.order='YXZ';root.add(body);const s={root,body};
 s.hips=mk(g.hips,body,0,.98,0);s.chest=mk(g.chest,s.hips,0,.08,0);s.head=mk(g.head,s.chest,0,.56,0);
 s.aL=mk(g.arm,s.chest,.215,.44,0);s.fL=mk(g.fore,s.aL,0,-.3,0);s.aR=mk(g.arm,s.chest,-.215,.44,0);s.fR=mk(g.fore,s.aR,0,-.3,0);
 s.tL=mk(g.thigh,s.hips,.085,-.08,0);s.sL=mk(g.shin,s.tL,0,-.44,0);s.tR=mk(g.thigh,s.hips,-.085,-.08,0);s.sR=mk(g.shin,s.tR,0,-.44,0);
 s.gun=mk(wpn==='smg'?g.smg:g.rifle,s.chest);s.lh=wpn==='smg'?.2:.34;s.mz=wpn==='smg'?.52:.9;scene.add(root);return s}
const DN=new V3(0,-1,0),_q=new THREE.Quaternion(),_q2=new THREE.Quaternion(),_e=new V3(),_t=new V3(),_s=new V3();
function ik(up,lo,sx,sy,tx,ty,tz,hx,hy,hz){const a=.3,b=.29;_s.set(sx,sy,0);_t.set(tx,ty,tz).sub(_s);let d=_t.length();const dd=Math.min(d,a+b-.005);_t.divideScalar(d||1);const x=(a*a-b*b+dd*dd)/(2*dd),h=Math.sqrt(Math.max(0,a*a-x*x));
 _e.set(hx,hy,hz);_e.addScaledVector(_t,-_e.dot(_t)).normalize();_v.copy(_t).multiplyScalar(x).addScaledVector(_e,h);up.quaternion.setFromUnitVectors(DN,_v2.copy(_v).normalize());
 _v3.copy(_t).multiplyScalar(dd).sub(_v).normalize();_q.copy(up.quaternion).invert();_v3.applyQuaternion(_q);lo.quaternion.setFromUnitVectors(DN,_v3)}
function pose(s,st){const cr=st.cr,ph=st.ph,amt=clamp(st.speed/4,0,1.25),dead=st.dead,al=1-dead;
 s.body.position.y=(-cr*.4+Math.abs(Math.sin(ph))*.035*amt)*al+dead*.14;s.body.rotation.x=st.fall*1.52*dead;s.body.rotation.z=st.roll*dead;
 const sw=Math.sin(ph)*.78*amt;
 s.tL.rotation.x=lerp(-sw,-1.35,cr)*al+dead*st.j1;s.sL.rotation.x=lerp(Math.max(0,Math.cos(ph))*1.05*amt+.04,1.5,cr)*al+dead*.5;
 s.tR.rotation.x=lerp(sw,.25,cr)*al-dead*st.j1*.6;s.sR.rotation.x=lerp(Math.max(0,-Math.cos(ph))*1.05*amt+.04,1.75,cr)*al+dead*.2;s.tL.rotation.z=dead*.25;s.tR.rotation.z=-dead*.2;
 const lean=lerp(.13*amt,-st.pitch*.75,st.aim)+cr*.2;s.chest.rotation.x=lean*al;s.chest.rotation.z=st.flinch*.5*al;s.chest.rotation.y=Math.sin(ph)*.07*amt*(1-st.aim);s.head.rotation.x=(-lean*.6-cr*.1)*al+dead*.3;s.head.rotation.z=dead*st.roll*2;
 const a=st.aim,bob=Math.sin(ph*2)*.012*amt;s.gun.position.set(lerp(-.07,-.13,a),lerp(.2+bob,.375,a),lerp(.2,.2-st.kick*.04,a));s.gun.rotation.set(lerp(.5,-st.kick*.12,a),lerp(.5,0,a),0);
 if(dead<.02){s.gun.updateMatrix();_v.set(0,-.03,0).applyMatrix4(s.gun.matrix);ik(s.aR,s.fR,-.215,.44,_v.x,_v.y,_v.z,-.7,-1,-.3);_v.set(0,-.02,s.lh).applyMatrix4(s.gun.matrix);ik(s.aL,s.fL,.215,.44,_v.x,_v.y,_v.z,.5,-1,-.1)}
 else{s.aL.rotation.set(-2.6*dead*st.j2,0,.9*dead);s.aR.rotation.set(-.6*dead,0,-1.1*dead*st.j2);s.fL.rotation.set(-.5*dead,0,0);s.fR.rotation.set(-.8*dead,0,0);s.gun.position.set(-.3,.2,.3);s.gun.rotation.set(.2,1.2,1.4)}}
// ---------- combattants ----------
const ents=[],bots=[];
const P={isPlayer:true,team:'A',name:'Toi',pos:new V3(0,0,90),vel:new V3(),yaw:0,pitch:0,rc:0,vy:0,onGround:true,hp:100,armor:100,alive:false,crouch:false,ads:false,adsK:0,eye:1.62,camY:0,bobT:0,speed:0,
 w:[],cur:0,fireCD:0,reloadT:0,reloadMax:1,swapT:0,bloom:0,kick:0,gren:3,med:2,lastHit:-9,kills:0,deaths:0,deadT:0,safe:0,grenCD:0,lastFire:-9,killer:null};
ents.push(P);
function spawnPos(team,out){for(let i=0;i<40;i++){const x=R(-52,52),z=(team==='A'?1:-1)*R(84,96);out.set(x,H(x,z),z);_v.copy(out);collide(_v,.6,out.y);if(Math.abs(_v.x-x)+Math.abs(_v.z-z)<.01)break}return out}
class Bot{constructor(team,name,wpn){this.team=team;this.name=name;this.wpn=wpn;this.m=makeSoldier(team,wpn);this.pos=new V3();this.dest=new V3();this.last=new V3();this.kills=0;this.deaths=0;this.alive=false;
  this.st={speed:0,ph:Math.random()*6,cr:0,aim:0,pitch:0,dead:0,fall:1,roll:0,j1:.3,j2:1,flinch:0,kick:0};this.m.root.visible=false;ents.push(this);bots.push(this)}
 spawn(){spawnPos(this.team,this.pos);this.hp=100;this.alive=true;this.yaw=this.team==='A'?Math.PI:0;this.target=null;this.crouch=false;this.react=0;this.fireCD=R(.5,1.5);this.burst=0;this.thinkT=Math.random()*.3;this.strafeT=0;this.strafe=0;
  this.stuckT=0;this.grenCD=R(8,22);this.lastFire=-9;this.alertT=-9;this.safe=2;this.speed=0;this.deadT=0;this.st.dead=0;this.st.cr=0;this.st.aim=0;this.last.copy(this.pos);this.newDest();this.m.root.visible=true}
 newDest(side){const s=this.team==='A'?-1:1,back=Math.random()<.22;this.dest.set(clamp(this.pos.x+(side?R(-25,25):R(-32,32)),-90,90),0,clamp(this.pos.z+(back?-s:s)*R(8,42),-64,64));this.run=Math.random()<.65;this.stuckT=0}
 eyeY(){return this.pos.y+(this.crouch?1.12:1.62)}
 alert(by){this.alertT=G.t;if(by&&by.alive&&by.team!==this.team&&(!this.target||Math.random()<.6)){this.target=by;this.react=Math.min(this.react,.25)}}
 think(){let best=null,bd=1e9;const sy=Math.sin(this.yaw),cy=Math.cos(this.yaw),alerted=G.t-this.alertT<3;
  for(const e of ents){if(e.team===this.team||!e.alive)continue;const dx=e.pos.x-this.pos.x,dz=e.pos.z-this.pos.z,d=Math.hypot(dx,dz);if(d>100)continue;
   if(e!==this.target&&!alerted&&d>14&&(dx*sy+dz*cy)/d<-.15)continue;const sc=d*(e===this.target?.6:1);if(sc>=bd)continue;
   if(los(this.pos.x,this.eyeY(),this.pos.z,e.pos.x,e.pos.y+(e.crouch?1:1.4),e.pos.z)){bd=sc;best=e}}
  if(best!==this.target){this.target=best;if(best)this.react=R(.4,.85)/DIFFS[G.diff].react}}
 shoot(t,d){const smg=this.wpn==='smg',df=this.team==='B'?DIFFS[G.diff]:DIFFS[1];if(this.burst<=0)this.burst=smg?3+(Math.random()*3|0):1;this.burst--;this.fireCD=this.burst>0?.105:(smg?R(.7,1.5):R(1.1,2));
  const p=clamp(.52-d*.0044,.06,.5)*df.acc*(t.speed>2?.6:1)*(t.crouch?.7:1)*(this.speed>.5?.55:1)*(smg?.75:1.3)*(t.safe>0?0:1);const hit=Math.random()<p;
  const mz=this.m.gun.localToWorld(_v.set(0,.02,this.mz)).clone(),ty=t.pos.y+(t.crouch?.75:1.2);_v2.set(t.pos.x,ty,t.pos.z);
  if(!hit){const k=R(.5,1.6)*(1+d*.02);_v2.x+=R(-1,1)*k;_v2.y+=R(-.6,1)*k;_v2.z+=R(-1,1)*k}
  _v3.subVectors(_v2,mz);const L=_v3.length();_v3.divideScalar(L);const w=rayWorld(mz.x,mz.y,mz.z,_v3.x,_v3.y,_v3.z,hit?L-.4:L+60,1);let end;
  if(w){end=_v2.copy(mz).addScaledVector(_v3,w.t);impact(end.x,end.y,end.z,w.mat,cam.position.distanceTo(end))}else{end=_v2.copy(mz).addScaledVector(_v3,hit?L:L+60);
   if(hit){const head=Math.random()<.1;blood(t.pos.x,ty,t.pos.z,_v3.x,_v3.z);damage(t,(smg?13:36)*(head?2:1)*(t.isPlayer?df.dmg:1),this,head,mz)}}
  if(!hit&&t.isPlayer&&d>4)sWhiz();tracer(mz,end);puff(T.flash,mz.x,mz.y,mz.z,0,0,0,.055,.55,.75,COL.white,1,0,true);puff(T.puff,mz.x,mz.y,mz.z,_v3.x,.4,_v3.z,.5,.15,.6,COL.steam,.25,0,false,1);
  sGun(cam.position.distanceTo(mz),smg?'smg':'rifle');this.lastFire=G.t;this.st.kick=1}
 die(from){this.st.fall=from&&((from.x-this.pos.x)*Math.sin(this.yaw)+(from.z-this.pos.z)*Math.cos(this.yaw))>0?-1:1;this.st.roll=R(-.25,.25);this.st.j1=R(.1,.7);this.st.j2=R(.4,1);this.deadT=0;this.target=null}
 update(dt){const st=this.st;if(!this.alive){this.deadT+=dt;st.dead=Math.min(1,st.dead+dt*2.6);st.speed=0;if(st.dead<1||this.deadT<.6){this.pos.y=Math.max(H(this.pos.x,this.pos.z),this.pos.y-dt*3);this.place()}if(this.deadT>6)this.spawn();return}
  this.safe-=dt;this.thinkT-=dt;if(this.thinkT<=0){this.thinkT=.22+Math.random()*.14;this.think()}
  const t=this.target;let mx=0,mz=0,sp=0,wantYaw=this.yaw,aim=0;
  if(t&&t.alive){const dx=t.pos.x-this.pos.x,dz=t.pos.z-this.pos.z,d=Math.hypot(dx,dz);wantYaw=Math.atan2(dx,dz);aim=1;st.pitch=Math.atan2(t.pos.y+(t.crouch?.75:1.2)-(this.pos.y+1.4),d);
   this.strafeT-=dt;if(this.strafeT<=0){this.strafeT=R(.9,2.4);this.strafe=Math.random()<.4?0:(Math.random()<.5?-1:1);this.crouch=Math.random()<(d>30?.6:.25)}
   if(this.strafe&&!this.crouch){mx=Math.cos(this.yaw)*this.strafe;mz=-Math.sin(this.yaw)*this.strafe;sp=2.1}
   else if(d>60&&!this.crouch){mx=dx/d;mz=dz/d;sp=3}
   this.react-=dt;this.fireCD-=dt;let dy=wantYaw-this.yaw;dy=Math.atan2(Math.sin(dy),Math.cos(dy));
   if(this.react<=0&&this.fireCD<=0&&Math.abs(dy)<.25)this.shoot(t,d);
   this.grenCD-=dt;if(this.grenCD<=0&&d>12&&d<32&&this.react<=0){this.grenCD=R(16,30);if(Math.random()<.5){const v=Math.sqrt(d*13/Math.sin(1.2));throwGren(_v.set(this.pos.x,this.pos.y+1.5,this.pos.z),_v2.set(dx/d*v*.825,v*.565,dz/d*v*.825),this)}}}
  else{this.target=null;this.crouch=false;const dx=this.dest.x-this.pos.x,dz=this.dest.z-this.pos.z,d=Math.hypot(dx,dz);if(d<2.5)this.newDest();else{mx=dx/d;mz=dz/d;sp=this.run?4.7:3;wantYaw=Math.atan2(dx,dz)}
   this.stuckT+=dt;if(this.stuckT>1.2){if(this.pos.distanceTo(this.last)<.7)this.newDest(true);this.last.copy(this.pos);this.stuckT=0}}
  let dy=wantYaw-this.yaw;dy=Math.atan2(Math.sin(dy),Math.cos(dy));this.yaw+=clamp(dy,-dt*(aim?7:4.5),dt*(aim?7:4.5));
  if(sp){this.pos.x+=mx*sp*dt;this.pos.z+=mz*sp*dt;for(const o of bots){if(o===this||!o.alive)continue;const ex=this.pos.x-o.pos.x,ez=this.pos.z-o.pos.z,ed=Math.hypot(ex,ez);if(ed<.8&&ed>.01){this.pos.x+=ex/ed*(.8-ed)*.5;this.pos.z+=ez/ed*(.8-ed)*.5}}
   collide(this.pos,.35,this.pos.y,true)}
  const gy=H(this.pos.x,this.pos.z);this.pos.y+=clamp(gy-this.pos.y,-dt*7,dt*4.5);this.speed=sp;
  st.speed+=(sp-st.speed)*Math.min(1,dt*8);st.ph+=st.speed*dt*2.3;st.cr+=((this.crouch?1:0)-st.cr)*Math.min(1,dt*7);st.aim+=(aim-st.aim)*Math.min(1,dt*7);st.flinch*=Math.max(0,1-dt*7);st.kick*=Math.max(0,1-dt*14);this.place()}
 place(){this.m.root.position.copy(this.pos);this.m.root.rotation.y=this.yaw;pose(this.m,this.st)}}
const NAMES={A:['Moreau','Lefèvre','Garnier','O’Brien','Kowalski'],B:['Krüger','Vogel','Brandt','Hartmann','Seidel','Lange']};
NAMES.A.forEach((n,i)=>new Bot('A',n,i%3===1?'rifle':'smg'));NAMES.B.forEach((n,i)=>new Bot('B',n,i%3===1?'rifle':'smg'));
const show=[['A','smg',-1.3,88.2,2.6,0,0],['A','rifle',.5,87.3,3.3,0,1],['A','smg',2,88.6,3.9,1,1]].map(a=>{const m=makeSoldier(a[0],a[1]);m.root.position.set(a[2],H(a[2],a[3]),a[3]);m.root.rotation.y=a[4];
 m.st={speed:0,ph:0,cr:a[5],aim:a[6],pitch:0,dead:0,fall:1,roll:0,j1:0,j2:0,flinch:0,kick:0};pose(m,m.st);return m});
// ---------- dégâts ----------
function damage(e,amt,by,head,from){if(!e.alive||G.state!=='play'||e.safe>0)return;
 if(e.isPlayer){let a=amt;if(P.armor>0){const ab=Math.min(P.armor,a*.6);P.armor-=ab;a-=ab}P.hp-=a;P.lastHit=G.t;shake=Math.max(shake,.35);hurtFx(from||by.pos);nz(.3,700,.12);if(P.hp<=0)kill(P,by,head,from)}
 else{e.hp-=amt;e.st.flinch=R(-1,1);e.alert(by);if(e.hp<=0)kill(e,by,head,from)}}
function kill(v,k,head,from){v.alive=false;v.deaths++;if(k!==v){k.kills++;if(k.team==='A')G.sA++;else G.sB++}feed(k,v,head);
 if(v.isPlayer){P.hp=0;P.deadT=5;P.killer=k;P.ads=false;$('deathBy').textContent=k===P?'Ta propre grenade.':'Abattu par '+k.name+'.';$('deathOv').hidden=false}else v.die(from||k.pos);
 if(k.isPlayer&&v!==P)note(head?'Tir à la tête':'Ennemi abattu',v.name+(head?' · +150':' · +100'));
 if(G.sA>=G.limit||G.sB>=G.limit)endMatch()}
// ---------- grenades ----------
const grens=[];{const g=new THREE.SphereGeometry(.055,8,6);g.scale(1,1.3,1);for(let i=0;i<8;i++){const m=new THREE.Mesh(g,SM({color:C(0x3c4428),roughness:.6}));m.visible=false;m.castShadow=true;scene.add(m);grens.push({m,v:new V3(),fuse:0,on:false,owner:null})}}
function throwGren(p,v,owner){const g=grens.find(x=>!x.on);if(!g)return;g.on=true;g.m.visible=true;g.m.position.copy(p);g.v.copy(v);g.fuse=2.6;g.owner=owner}
function updGren(dt){for(const g of grens){if(!g.on)continue;const p=g.m.position;g.fuse-=dt;if(g.fuse<=0){g.on=false;g.m.visible=false;explode(p,g.owner);continue}
  g.v.y-=13*dt;const sp=g.v.length();if(sp>.5){const w=rayWorld(p.x,p.y,p.z,g.v.x/sp,g.v.y/sp,g.v.z/sp,sp*dt+.08,.3);if(w&&w.mat!=='dirt'){g.v.x*=-.35;g.v.z*=-.35;sClick(.2,900)}}
  p.addScaledVector(g.v,dt);const gy=H(p.x,p.z)+.06;if(p.y<gy){p.y=gy;if(g.v.y<-2)sClick(.2,700);g.v.y=Math.abs(g.v.y)>1.2?-g.v.y*.35:0;g.v.x*=.62;g.v.z*=.62}g.m.rotation.x+=sp*dt*3}}
// ---------- armes en vue subjective ----------
const vScene=new THREE.Scene(),vCam=new THREE.PerspectiveCamera(52,1,.01,10);
vScene.add(new THREE.HemisphereLight(C(0xcfe0f5),C(0x8a8068),1.0));const vSun=new THREE.DirectionalLight(C(0xffe9c4),2.2);vScene.add(vSun);
const VM={};
{const steel=SM({color:C(0x3a3d41),metalness:.45,roughness:.42}),worn=SM({color:C(0x6a6d70),metalness:.5,roughness:.33}),bake=SM({color:C(0x3a2218),roughness:.55}),wood=SM({map:T.wood,color:C(0xc89868),roughness:.6}),
  skin=SM({color:C(0xb88766),roughness:.62}),cloth=SM({map:T.cloth,color:C(0x6f7a4a),roughness:1}),glass=SM({color:C(0x1a2630),roughness:.1,metalness:.6});
 const cz=(a,b,l,s)=>new THREE.CylinderGeometry(a,b,l,s||14).rotateX(Math.PI/2);
 const hand=(grp,x,y,z,rx,ry,rz,wrap)=>{const h=new THREE.Group();h.rotation.order='YXZ';h.position.set(x,y,z);h.rotation.set(rx,ry,rz);const a=(g,m,px,py,pz,ax,ay,az)=>{const o=new THREE.Mesh(g,m);o.position.set(px,py,pz);o.rotation.set(ax||0,ay||0,az||0);h.add(o);return o};
  a(new THREE.BoxGeometry(.078,.03,.085),skin,0,0,.01);a(new THREE.SphereGeometry(.04,10,8),skin,0,-.002,.05).scale.set(1,.5,.9);
  for(let i=0;i<4;i++){const fx=-.03+i*.02;a(new THREE.CylinderGeometry(.0095,.0105,.042,8),skin,fx,-.012,-.045,1.05);a(new THREE.CylinderGeometry(.0085,.0095,.04,8),skin,fx,-.012-wrap*.034,-.066+wrap*.012,2.35+wrap*.4)}
  a(new THREE.CylinderGeometry(.0105,.012,.055,8),skin,.045,-.012,-.005,1.1,0,-.75);a(new THREE.CylinderGeometry(.036,.041,.07,12),skin,0,0,.085,Math.PI/2);
  a(new THREE.CylinderGeometry(.043,.058,.5,14),cloth,0,-.004,.36,Math.PI/2);a(new THREE.CylinderGeometry(.047,.047,.03,14),cloth,0,-.002,.125,Math.PI/2);grp.add(h);return h};
 const mk=(grp,g,m,x,y,z,rx,ry,rz)=>{const o=new THREE.Mesh(g,m);o.position.set(x,y,z);o.rotation.set(rx||0,ry||0,rz||0);grp.add(o);return o};
 const fin=(id,grp,o)=>{grp.visible=false;const fl=new THREE.Sprite(new THREE.SpriteMaterial({map:T.flash,blending:THREE.AdditiveBlending,depthWrite:false,transparent:true}));fl.position.copy(o.mz);fl.scale.set(o.fs,o.fs,1);fl.visible=false;grp.add(fl);vScene.add(grp);VM[id]=Object.assign({g:grp,fl},o)};
 // pistolet-mitrailleur
 let g=new THREE.Group();mk(g,cz(.021,.021,.36),steel,0,0,-.03);mk(g,cz(.0225,.0225,.05),worn,0,0,.13);mk(g,new THREE.BoxGeometry(.03,.045,.24),steel,0,-.03,.03);
 mk(g,cz(.0105,.0115,.25),worn,0,.001,-.33);mk(g,cz(.0155,.0155,.035),steel,0,.001,-.455);mk(g,new THREE.BoxGeometry(.005,.028,.01),steel,0,.022,-.44);mk(g,new THREE.TorusGeometry(.012,.0028,6,14),steel,0,.034,-.44);
 mk(g,new THREE.BoxGeometry(.012,.016,.03),steel,0,.027,0);mk(g,new THREE.BoxGeometry(.034,.07,.05),steel,0,-.05,-.175);mk(g,new THREE.BoxGeometry(.024,.21,.036),worn,0,-.185,-.175);
 mk(g,new THREE.BoxGeometry(.036,.035,.2),bake,0,-.045,-.02);mk(g,new THREE.BoxGeometry(.03,.115,.046),bake,0,-.105,.115,.32);mk(g,cz(.006,.006,.03),worn,.026,0,.02,0,Math.PI/2);mk(g,new THREE.SphereGeometry(.009,8,8),worn,.045,0,.02);
 mk(g,new THREE.BoxGeometry(.012,.012,.3),steel,0,-.07,.27,.1);mk(g,new THREE.BoxGeometry(.03,.08,.014),steel,0,-.07,.42);
 hand(g,.034,-.1,.125,.75,-.25,-1.45,1);hand(g,-.012,-.12,-.17,.35,-.85,2.9,.6);fin('smg',g,{hip:new V3(.125,-.112,-.4),ads:new V3(0,-.036,-.52),mz:new V3(0,0,-.5),fs:.2,ej:new V3(.03,.01,.02)});
 // fusil (semi-auto / lunette)
 for(const id of['rifle','sniper']){g=new THREE.Group();mk(g,new THREE.BoxGeometry(.04,.055,.56),wood,0,-.02,-.16);mk(g,new THREE.BoxGeometry(.038,.075,.3),wood,0,-.045,.24,-.16);mk(g,new THREE.BoxGeometry(.036,.1,.04),wood,0,-.065,.385,-.1);
  mk(g,cz(.0095,.0105,.66),steel,0,.012,-.42);mk(g,new THREE.BoxGeometry(.04,.045,.2),steel,0,.012,.03);mk(g,cz(.014,.014,.1),worn,0,.022,.06);mk(g,cz(.005,.005,.05),worn,.03,.02,.07,0,Math.PI/2);mk(g,new THREE.SphereGeometry(.009,8,8),worn,.055,.02,.07);
  mk(g,new THREE.BoxGeometry(.005,.025,.008),steel,0,.03,-.72);mk(g,new THREE.BoxGeometry(.016,.014,.02),steel,0,.04,-.02);mk(g,new THREE.TorusGeometry(.02,.003,6,12,Math.PI),steel,0,-.05,.1,0,Math.PI/2,Math.PI);mk(g,cz(.016,.016,.05),steel,0,-.01,-.45);
  if(id==='sniper'){mk(g,cz(.016,.016,.24),steel,0,.075,.02);mk(g,cz(.022,.018,.05),steel,0,.075,-.12);mk(g,cz(.02,.02,.04),steel,0,.075,.15);mk(g,cz(.019,.019,.004),glass,0,.075,.171);mk(g,new THREE.BoxGeometry(.012,.04,.02),steel,0,.05,-.04);mk(g,new THREE.BoxGeometry(.012,.04,.02),steel,0,.05,.09)}
  hand(g,.03,-.075,.2,.6,-.25,-1.45,1);hand(g,-.006,-.062,-.26,.3,-.8,3.0,.5);fin(id,g,{hip:new V3(.13,-.105,-.36),ads:new V3(0,id==='sniper'?-.075:-.047,-.26),mz:new V3(0,.012,-.78),fs:.26,ej:new V3(.03,.03,.05)})}
 // pistolet
 g=new THREE.Group();mk(g,new THREE.BoxGeometry(.026,.032,.18),steel,0,0,-.02);mk(g,cz(.0075,.0075,.03),worn,0,0,-.12);mk(g,new THREE.BoxGeometry(.024,.02,.13),worn,0,-.022,-.03);mk(g,new THREE.BoxGeometry(.028,.105,.042),bake,0,-.07,.045,.22);
 mk(g,new THREE.BoxGeometry(.004,.01,.006),steel,0,.02,-.1);mk(g,new THREE.BoxGeometry(.012,.01,.008),steel,0,.02,.06);mk(g,new THREE.TorusGeometry(.014,.0025,6,12,Math.PI),steel,0,-.03,-.005,0,Math.PI/2,Math.PI);
 hand(g,.03,-.07,.055,.7,-.2,-1.45,1);hand(g,-.03,-.085,.06,.6,.5,1.45,1);fin('pistol',g,{hip:new V3(.09,-.095,-.34),ads:new V3(0,-.026,-.4),mz:new V3(0,0,-.15),fs:.13,ej:new V3(.02,.02,0)})}
const casings=[];{const cg=new THREE.CylinderGeometry(.004,.004,.016,6),cm=SM({color:C(0xc8a040),metalness:.7,roughness:.35});for(let i=0;i<6;i++){const m=new THREE.Mesh(cg,cm);m.visible=false;vScene.add(m);casings.push({m,v:new V3(),life:0})}}let csi=0;
function vm(){return VM[P.w[P.cur].id]}
function setWeapon(i){P.cur=i;P.swapT=.35;P.reloadT=0;P.ads=false;for(const k in VM)VM[k].g.visible=false;vm().g.visible=true;$('wpath').setAttribute('d',W[P.w[i].id].icon);hudDirty=true}
function reload(){const cw=P.w[P.cur],w=W[cw.id];if(P.reloadT>0||cw.ammo>=w.mag||cw.res<=0||!P.alive)return;P.reloadT=P.reloadMax=w.reload;P.ads=false;sClick(.3,1800);setTimeout(()=>sClick(.3,1200),w.reload*450);setTimeout(()=>sClick(.4,2600),w.reload*900)}
const _d=new V3(),_o=new V3(),_r=new V3(),_u=new V3();
function raySph(o,d,cx,cy,cz,r){const x=o.x-cx,y=o.y-cy,z=o.z-cz,b=x*d.x+y*d.y+z*d.z,c=x*x+y*y+z*z-r*r,q=b*b-c;if(q<0)return-1;const t=-b-Math.sqrt(q);return t>0?t:-1}
const ZS=[[1.7,.15,0],[1.3,.27,1],[.95,.27,1],[.45,.24,.7]],ZC=[[1.27,.15,0],[.95,.27,1],[.6,.27,1],[.25,.26,.7]];
function fire(){const cw=P.w[P.cur],w=W[cw.id];if(P.reloadT>0||P.swapT>0||P.fireCD>0||!P.alive)return;if(cw.ammo<=0){sClick(.25,2400);P.fireCD=.25;reload();return}
 cw.ammo--;P.fireCD=60/w.rpm;hudDirty=true;const sp=(P.adsK>.8?w.ads:w.spread)*(1+P.speed*.22)*(P.crouch?.7:1)*(P.onGround?1:2.2)+P.bloom;P.bloom=Math.min(.035,P.bloom+w.rec*.22);
 cam.getWorldDirection(_d);_r.crossVectors(_d,cam.up).normalize();_u.crossVectors(_r,_d);const a=Math.random()*6.283,rd=Math.sqrt(Math.random())*sp;_d.addScaledVector(_r,Math.cos(a)*rd).addScaledVector(_u,Math.sin(a)*rd).normalize();
 _o.copy(cam.position);const wh=rayWorld(_o.x,_o.y,_o.z,_d.x,_d.y,_d.z,300,.8);let best=wh?wh.t:300,he=null,zone=1;
 for(const e of ents){if(!e.alive||e.team==='A')continue;const dx=e.pos.x-_o.x,dz=e.pos.z-_o.z,pr=dx*_d.x+dz*_d.z;if(pr<-1||pr>best+1)continue;for(const z of(e.crouch?ZC:ZS)){const t=raySph(_o,_d,e.pos.x,e.pos.y+z[0],e.pos.z,z[1]);if(t>0&&t<best){best=t;he=e;zone=z[2]}}}
 const end=_v2.copy(_o).addScaledVector(_d,best);
 if(he){blood(end.x,end.y,end.z,_d.x,_d.z);const head=zone===0,was=he.alive;damage(he,w.dmg*(head?w.head:zone)*(best>60&&!w.scope?.75:1),P,head,_o);hitMark(was&&!he.alive);sHit()}else if(wh)impact(end.x,end.y,end.z,wh.mat,best);
 const v=vm();_v.copy(_o).addScaledVector(_r,.11).addScaledVector(_u,-.09).addScaledVector(_d,.7);if(best>3)tracer(_v,end);
 if(!(w.scope&&P.adsK>.8)){v.fl.visible=true;v.fl.material.rotation=Math.random()*6.28;v.flT=.045}flashL.position.copy(_v);flashL.intensity=9;
 puff(T.puff,_v.x,_v.y,_v.z,_d.x*2,.5,_d.z*2,.5,.12,.7,COL.steam,.22,0,false,2);
 sGun(0,w.snd);P.pitch=Math.min(1.45,P.pitch+w.rec*.45*(P.adsK>.5?.75:1));P.rc+=w.rec*.55;P.yaw+=R(-1,1)*w.rec*.3;P.kick=1;shake=Math.max(shake,w.rec*4);
 const c=casings[csi=(csi+1)%6];c.m.visible=true;c.m.position.copy(v.g.position).add(v.ej);c.v.set(R(.5,.9),R(.5,1),R(-.1,.25));c.life=.6;
 P.lastFire=G.t;G.noise=G.t;for(const b of bots)if(b.alive&&b.team==='B'&&b.pos.distanceTo(P.pos)<48)b.alertT=G.t;if(cw.ammo<=0&&cw.res>0)setTimeout(reload,250)}
// ---------- HUD ----------
let hudDirty=true,noteT=0,hitT=0,dmgA=0;const dirs=[];
function feed(k,v,head){const f=$('feed'),d=document.createElement('div');const c=e=>e.isPlayer?'me':e.team;d.innerHTML=`<b class="${c(k)}"></b> ${head?'◎':'▸'} <b class="${c(v)}"></b>`;d.children[0].textContent=k.name;d.children[1].textContent=v.name;f.prepend(d);while(f.children.length>5)f.lastChild.remove();setTimeout(()=>d.remove(),6000);hudDirty=true}
function note(a,b){const m=$('msg');m.textContent=a;const s=document.createElement('small');s.textContent=b;m.appendChild(s);noteT=1.8}
function hitMark(k){const h=$('hitm');h.classList.toggle('k',k);hitT=.25}
function hurtFx(from){dmgA=Math.min(1,dmgA+.55);const el=document.createElement('i');$('dirs').appendChild(el);dirs.push({el,x:from.x,z:from.z,t:1.2})}
const mg=$('mini').getContext('2d');let miniT=0;
function drawMini(){mg.save();mg.fillStyle='#4f5a38';mg.fillRect(0,0,256,256);mg.translate(128,128);mg.rotate(P.yaw);mg.scale(1.7,1.7);mg.translate(-(P.pos.x+120)*2,-(P.pos.z+120)*2);mg.drawImage(mapCv,0,0);
 for(const b of bots){if(!b.alive)continue;if(b.team==='B'&&G.t-b.lastFire>2.5)continue;mg.fillStyle=b.team==='A'?'#5aa8ff':'#ff5546';mg.beginPath();mg.arc((b.pos.x+120)*2,(b.pos.z+120)*2,3.6,0,7);mg.fill();mg.lineWidth=1;mg.strokeStyle='#fff';mg.stroke()}
 mg.restore();mg.fillStyle='#fff';mg.strokeStyle='#000';mg.lineWidth=1.5;mg.beginPath();mg.moveTo(128,114);mg.lineTo(138,140);mg.lineTo(128,134);mg.lineTo(118,140);mg.closePath();mg.fill();mg.stroke()}
function updHud(dt){if(hudDirty){hudDirty=false;const cw=P.w[P.cur];$('sA').textContent=G.sA;$('sB').textContent=G.sB;$('ammo').textContent=cw.ammo+' / '+cw.res;$('gN').textContent=$('gN2').textContent=P.gren;$('mN').textContent=$('mN2').textContent=P.med}
 const hp=Math.max(0,Math.ceil(P.hp)),ar=Math.max(0,Math.ceil(P.armor));if(updHud.hp!==hp){updHud.hp=hp;$('hpN').textContent=hp;$('hpF').style.transform=`scaleX(${hp/100})`}if(updHud.ar!==ar){updHud.ar=ar;$('arN').textContent=ar;$('arF').style.transform=`scaleX(${ar/100})`}
 const s=Math.max(0,Math.ceil(G.time)),ts=(s/60|0)+':'+String(s%60).padStart(2,'0');if(updHud.ts!==ts){updHud.ts=ts;$('timer').textContent=ts}
 const w=W[P.w[P.cur].id],gap=6+((P.adsK>.8?w.ads:w.spread)*(1+P.speed*.22)+P.bloom)*520;$('cross').style.setProperty('--g',gap+'px');$('cross').style.opacity=P.adsK>.6||P.reloadT>0?0:1;
 if(noteT>0){noteT-=dt;$('msg').style.opacity=Math.min(1,noteT*3)}if(hitT>0){hitT-=dt;$('hitm').style.opacity=Math.max(0,hitT*4)}
 dmgA=Math.max(0,dmgA-dt*.9);$('dmgv').style.opacity=Math.min(1,dmgA+(P.alive?Math.max(0,(45-P.hp)/60):.8));
 for(let i=dirs.length-1;i>=0;i--){const d=dirs[i];d.t-=dt;if(d.t<=0){d.el.remove();dirs.splice(i,1);continue}const a=Math.atan2(d.x-P.pos.x,-(d.z-P.pos.z))+P.yaw;d.el.style.transform=`rotate(${a}rad)`;d.el.style.opacity=Math.min(1,d.t*2)}
 miniT-=dt;if(miniT<=0){miniT=.09;drawMini()}}
// ---------- commandes ----------
const I={jx:0,jy:0,fire:false,edge:false,jump:false,joyId:-1,jcx:0,jcy:0,lookId:-1,last:new Map(),fireIds:new Set()},K={};
const cv=$('c');if(!coarse)document.body.classList.add('desk');
function sensK(){const w=W[P.w[P.cur].id];return G.sens*(1-P.adsK*(w.scope?.8:.5))}
function look(dx,dy,k){if(!P.alive)return;P.yaw-=dx*k*sensK();P.pitch=clamp(P.pitch-dy*k*sensK(),-1.45,1.45)}
function act(a,on,e){if(G.state!=='play')return;if(a==='pause'){if(on)setPause(true);return}if(!P.alive)return;
 if(a==='fire'){I.fire=on;if(on){I.edge=true;if(e){I.fireIds.add(e.pointerId);I.last.set(e.pointerId,[e.clientX,e.clientY])}}else if(e)I.fireIds.delete(e.pointerId);return}if(!on)return;
 if(a==='aim'){P.ads=!P.ads&&P.reloadT<=0}else if(a==='crouch')P.crouch=!P.crouch;else if(a==='jump')I.jump=true;else if(a==='reload')reload();else if(a==='swap')setWeapon(1-P.cur);
 else if(a==='gren'){if(P.gren>0&&P.grenCD<=0){P.gren--;P.grenCD=1;cam.getWorldDirection(_d);throwGren(_v.copy(cam.position).addScaledVector(_d,.5),_v2.copy(_d).multiplyScalar(17).add(_v3.set(P.vel.x,3.5,P.vel.z)),P);hudDirty=true}}
 else if(a==='heal'){if(P.med>0&&(P.hp<100||P.armor<100)){P.med--;P.hp=100;P.armor=Math.min(100,P.armor+50);sClick(.3,900);note('Soins appliqués','');hudDirty=true}}
 $('bAim').classList.toggle('on',P.ads);$('bCrouch').classList.toggle('on',P.crouch)}
const gm=$('game');
gm.addEventListener('pointerdown',e=>{if(G.state!=='play')return;const b=e.target.closest('[data-act]');
 if(e.pointerType==='mouse'){if(b){act(b.dataset.act,true);return}if(document.pointerLockElement!==cv){try{const p=cv.requestPointerLock();if(p&&p.catch)p.catch(()=>{})}catch(x){}return}if(e.button===0){I.fire=true;I.edge=true}else if(e.button===2)P.ads=P.reloadT<=0;return}
 e.preventDefault();if(b){act(b.dataset.act,true,e);return}
 if(e.clientX<innerWidth*.42&&I.joyId<0){I.joyId=e.pointerId;I.jcx=e.clientX;I.jcy=e.clientY}else if(I.lookId<0){I.lookId=e.pointerId;I.last.set(e.pointerId,[e.clientX,e.clientY])}});
addEventListener('pointermove',e=>{if(e.pointerType==='mouse')return;if(e.pointerId===I.joyId){const dx=(e.clientX-I.jcx)/52,dy=(e.clientY-I.jcy)/52,m=Math.hypot(dx,dy),k=m>1?1/m:1;I.jx=dx*k;I.jy=dy*k;$('knob').style.transform=`translate(${I.jx*30}px,${I.jy*30}px)`}
 else if(e.pointerId===I.lookId||I.fireIds.has(e.pointerId)){const l=I.last.get(e.pointerId);if(l){look(e.clientX-l[0],e.clientY-l[1],.0052);l[0]=e.clientX;l[1]=e.clientY}}});
function pUp(e){if(e.pointerType==='mouse'){if(e.button===0)I.fire=false;else if(e.button===2)P.ads=false;return}if(e.pointerId===I.joyId){I.joyId=-1;I.jx=I.jy=0;$('knob').style.transform=''}if(e.pointerId===I.lookId)I.lookId=-1;
 if(I.fireIds.has(e.pointerId)){I.fireIds.delete(e.pointerId);if(!I.fireIds.size)I.fire=false}I.last.delete(e.pointerId)}
addEventListener('pointerup',pUp);addEventListener('pointercancel',pUp);
addEventListener('mousemove',e=>{if(document.pointerLockElement===cv&&G.state==='play')look(e.movementX,e.movementY,.0021)});
addEventListener('contextmenu',e=>e.preventDefault());
addEventListener('keydown',e=>{K[e.code]=true;if(G.state!=='play'||e.repeat)return;const m={KeyR:'reload',KeyG:'gren',KeyH:'heal',KeyC:'crouch',ControlLeft:'crouch',Space:'jump',KeyQ:'swap',Digit1:'swap',Digit2:'swap',KeyP:'pause',Escape:'pause'}[e.code];if(m){act(m,true);if(e.code==='Space')e.preventDefault()}});
addEventListener('keyup',e=>{K[e.code]=false});
document.addEventListener('pointerlockchange',()=>{if(document.pointerLockElement!==cv&&G.state==='play'&&!coarse&&!window.__auto)setPause(true)});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&G.state==='play')setPause(true)});
// ---------- joueur ----------
function respawnPlayer(){spawnPos('A',P.pos);P.vel.set(0,0,0);P.yaw=0;P.pitch=0;P.rc=0;P.hp=100;P.armor=100;P.alive=true;P.crouch=false;P.ads=false;P.onGround=true;P.vy=0;P.safe=2.5;P.gren=3;P.med=2;P.camY=P.pos.y+1.62;P.eye=1.62;
 P.w=[{id:G.primary,ammo:W[G.primary].mag,res:W[G.primary].res},{id:'pistol',ammo:8,res:48}];setWeapon(0);P.swapT=0;$('deathOv').hidden=true;$('bAim').classList.remove('on');$('bCrouch').classList.remove('on');hudDirty=true}
let stepAcc=0;
function updPlayer(dt){const w=W[P.w[P.cur].id];
 if(!P.alive){P.deadT-=dt;$('deathT').textContent='Redéploiement dans '+Math.max(1,Math.ceil(P.deadT))+' s';P.camY+=(P.pos.y+.3-P.camY)*Math.min(1,dt*5);cam.position.set(P.pos.x,P.camY,P.pos.z);cam.rotation.set(P.pitch,P.yaw,Math.min(.9,(5-P.deadT)*2));if(P.deadT<=0)respawnPlayer();return}
 P.safe-=dt;let f=(K.KeyW||K.ArrowUp?1:0)-(K.KeyS||K.ArrowDown?1:0)-I.jy,s=(K.KeyD||K.ArrowRight?1:0)-(K.KeyA||K.ArrowLeft?1:0)+I.jx;const m=Math.hypot(f,s);if(m>1){f/=m;s/=m}
 const sprint=(K.ShiftLeft||Math.hypot(I.jx,I.jy)>.93)&&f>.6&&!P.crouch&&!P.ads;
 const sp=P.crouch?2.1:P.ads?2.4:sprint?6.3:4.2,sy=Math.sin(P.yaw),cy=Math.cos(P.yaw),tx=(-sy*f+cy*s)*sp,tz=(-cy*f-sy*s)*sp,k=Math.min(1,dt*(P.onGround?11:2.5));
 P.vel.x+=(tx-P.vel.x)*k;P.vel.z+=(tz-P.vel.z)*k;P.pos.x+=P.vel.x*dt;P.pos.z+=P.vel.z*dt;collide(P.pos,.35,P.pos.y);
 const gy=H(P.pos.x,P.pos.z);if(P.onGround){if(gy<P.pos.y-.4){P.onGround=false;P.vy=0}else P.pos.y=gy}
 if(I.jump&&P.onGround){P.vy=5.1;P.onGround=false;P.crouch=false;$('bCrouch').classList.remove('on')}I.jump=false;
 if(!P.onGround){P.vy-=13*dt;P.pos.y+=P.vy*dt;if(P.pos.y<=gy){if(P.vy<-6)shake=Math.max(shake,.3);P.pos.y=gy;P.onGround=true;P.vy=0;sStep()}}
 P.speed=Math.hypot(P.vel.x,P.vel.z);P.eye+=((P.crouch?1.08:1.62)-P.eye)*Math.min(1,dt*10);P.bobT+=P.speed*dt*1.9;stepAcc+=P.speed*dt;if(stepAcc>2.1&&P.onGround){stepAcc=0;sStep()}
 const ty=P.pos.y+P.eye;P.camY=P.onGround?P.camY+(ty-P.camY)*Math.min(1,dt*16):ty;
 if(G.t-P.lastHit>6&&P.hp<100)P.hp=Math.min(100,P.hp+dt*10);
 P.fireCD-=dt;P.swapT-=dt;P.grenCD-=dt;P.bloom=Math.max(0,P.bloom-dt*.06);P.rc*=Math.max(0,1-dt*8);P.kick*=Math.max(0,1-dt*13);
 if(P.reloadT>0){P.reloadT-=dt;if(P.reloadT<=0){const cw=P.w[P.cur],n=Math.min(w.mag-cw.ammo,cw.res);cw.ammo+=n;cw.res-=n;hudDirty=true}}
 if(I.fire&&(w.auto||I.edge)&&!sprint)fire();I.edge=false;
 const wantAds=P.ads&&P.reloadT<=0&&!sprint?1:0;P.adsK+=(wantAds-P.adsK)*Math.min(1,dt*11);
 const fov=lerp(sprint?72:68,w.fov,P.adsK);if(Math.abs(cam.fov-fov)>.05){cam.fov+=(fov-cam.fov)*Math.min(1,dt*14);cam.updateProjectionMatrix()}
 const bob=Math.sin(P.bobT*2)*.024*Math.min(1,P.speed/4.2)*(1-P.adsK*.8),sh=shake*.02;
 cam.position.set(P.pos.x,P.camY+bob,P.pos.z);cam.rotation.set(P.pitch+P.rc+R(-1,1)*sh,P.yaw+R(-1,1)*sh,Math.sin(P.bobT)*.006*Math.min(1,P.speed/4)+R(-1,1)*sh*.5);
 // arme
 const v=vm(),g=v.g,a=P.adsK,rl=P.reloadT>0?Math.sin((1-P.reloadT/P.reloadMax)*Math.PI):0,sw=P.swapT>0?P.swapT/.35:0,bx=Math.sin(P.bobT)*.008*(1-a)*Math.min(1,P.speed/4),by=Math.abs(Math.cos(P.bobT))*.007*(1-a)*Math.min(1,P.speed/4);
 g.position.set(lerp(v.hip.x,v.ads.x,a)+bx,lerp(v.hip.y,v.ads.y,a)-by-rl*.09-sw*.2-(sprint?.03:0),lerp(v.hip.z,v.ads.z,a)+P.kick*.035*(1-a*.5));
 g.rotation.set(lerp(.03,0,a)+P.kick*.05*(1-a*.6)-rl*.5+sw*.6,lerp(.1,0,a)+(sprint?.5:0)*(1-a),rl*.45);
 const scoped=!!w.scope&&a>.85;g.visible=!scoped;$('scopeOv').hidden=!scoped;if(v.flT>0){v.flT-=dt;if(v.flT<=0)v.fl.visible=false}
 for(const c of casings){if(c.life<=0)continue;c.life-=dt;if(c.life<=0){c.m.visible=false;continue}c.v.y-=6*dt;c.m.position.addScaledVector(c.v,dt);c.m.rotation.x+=dt*25;c.m.rotation.z+=dt*17}}
// ---------- avions et fumées ----------
const planes=[];{const pm=new THREE.MeshLambertMaterial({color:C(0x4a5238)}),gs=[part(cylG(.55,.25,7,8),COL.white,0,0,0,Math.PI/2),part(boxG(10.5,.14,1.7),COL.white,0,-.1,-.6),part(boxG(3.4,.1,.9),COL.white,0,.1,3),part(boxG(.1,1.1,.9),COL.white,0,.55,3.1),part(cylG(.62,.62,.5,10),COL.white,0,0,-3.6,Math.PI/2)];
 const pg=mergeG(gs);for(let i=0;i<3;i++){const m=new THREE.Mesh(pg,pm);m.visible=false;scene.add(m);planes.push(m)}}
let planeT=14,planeOn=false,planeX=0,planeZ=0,planeDir=1,planeOsc=null;
function updAmb(dt){planeT-=dt;if(!planeOn&&planeT<=0){planeOn=true;planeDir=Math.random()<.5?1:-1;planeX=-650*planeDir;planeZ=R(-80,60);
  if(SND.ac){const ac=SND.ac,o=ac.createOscillator(),f=ac.createBiquadFilter(),g=ac.createGain();o.type='sawtooth';o.frequency.value=82;f.type='lowpass';f.frequency.value=420;g.gain.value=0;o.connect(f);f.connect(g);g.connect(SND.master);o.start();planeOsc={o,g}}}
 if(planeOn){planeX+=85*dt*planeDir;planes.forEach((m,i)=>{m.visible=true;m.position.set(planeX-i*14*planeDir,95+i*4,planeZ+i*16);m.rotation.y=planeDir>0?-Math.PI/2:Math.PI/2;m.rotation.z=Math.sin(G.t+i)*.05});
  if(planeOsc){const d=Math.hypot(planeX-cam.position.x,90);planeOsc.g.gain.value=.22/(1+d/110);planeOsc.o.frequency.value=82+clamp(-(planeX-cam.position.x)*planeDir*.03,-12,12)}
  if(Math.abs(planeX)>660){planeOn=false;planeT=R(35,60);planes.forEach(m=>m.visible=false);if(planeOsc){planeOsc.o.stop();planeOsc=null}}}
 updAmb.s=(updAmb.s||0)-dt;if(updAmb.s<=0){updAmb.s=.22;for(const p of smokers)puff(T.puff,p.x+R(-.3,.3),p.y,p.z+R(-.3,.3),R(.5,1.1),R(1.6,2.6),R(-.3,.1),R(5,8),1,R(6,9),Math.random()<.3?COL.smoke:COL.dark,.55,-.05)}}
// ---------- déroulement ----------
function setPause(p){if(p&&G.state==='play'){G.state='pause';$('pauseOv').hidden=false;I.fire=false;if(document.exitPointerLock)document.exitPointerLock()}else if(!p&&G.state==='pause'){G.state='play';$('pauseOv').hidden=true;last=performance.now()}}
function startMatch(){audioInit();G.sA=G.sB=0;G.time=600;G.t=0;P.kills=P.deaths=0;for(const b of bots){b.kills=b.deaths=0;b.spawn()}show.forEach(m=>m.root.visible=false);respawnPlayer();$('feed').textContent='';
 $('lobby').hidden=true;$('endOv').hidden=true;$('hud').hidden=false;G.state='play';last=performance.now();fitHud();
 if(coarse){try{const p=document.documentElement.requestFullscreen&&document.documentElement.requestFullscreen();if(p&&p.catch)p.catch(()=>{})}catch(e){}}}
function toLobby(){G.state='lobby';for(const b of bots){b.alive=false;b.m.root.visible=false}for(const g of grens){g.on=false;g.m.visible=false}show.forEach(m=>m.root.visible=true);for(const k in VM)VM[k].g.visible=false;
 $('hud').hidden=true;$('pauseOv').hidden=true;$('endOv').hidden=true;$('deathOv').hidden=true;$('scopeOv').hidden=true;$('lobby').hidden=false;$('dmgv').style.opacity=0;cam.fov=50;cam.updateProjectionMatrix();if(document.exitPointerLock)document.exitPointerLock()}
function endMatch(){if(G.state!=='play')return;G.state='end';I.fire=false;if(document.exitPointerLock)document.exitPointerLock();const win=G.sA>G.sB,draw=G.sA===G.sB;$('endT').textContent=draw?'Égalité':win?'Victoire':'Défaite';$('endS').textContent=`Alliés ${G.sA} — ${G.sB} Axe · tes éliminations : ${P.kills}, tes morts : ${P.deaths}`;
 const eb=$('endB');eb.textContent='';for(const tm of['A','B']){const t=document.createElement('table');t.className='sb';t.innerHTML='<tr><th>'+(tm==='A'?'Alliés':'Axe')+'</th><th>É</th><th>M</th></tr>';
  ents.filter(e=>e.team===tm).sort((a,b)=>b.kills-a.kills).forEach(e=>{const r=t.insertRow();r.className=tm+(e.isPlayer?' me':'');r.insertCell().textContent=e.name;r.insertCell().textContent=e.kills;r.insertCell().textContent=e.deaths});eb.appendChild(t)}
 $('deathOv').hidden=true;$('scopeOv').hidden=true;$('endOv').hidden=false}
function fitHud(){const z=clamp(innerHeight/430,.62,1.15);$('hud').style.zoom=z}
function resize(){const w=innerWidth,h=innerHeight;renderer.setSize(w,h,false);cam.aspect=vCam.aspect=w/h;cam.updateProjectionMatrix();vCam.updateProjectionMatrix();fitHud()}
addEventListener('resize',resize);resize();
// lobby
{const li=(n,s,me)=>{const l=document.createElement('li');l.textContent=n;const sp=document.createElement('span');sp.textContent=s;l.appendChild(sp);if(me)l.style.color='#ffd76a';return l};
 const a=$('rosA'),b=$('rosB');a.innerHTML='<li><b>ALLIÉS</b></li>';b.innerHTML='<li><b>AXE</b></li>';a.appendChild(li('Toi','joueur',true));bots.forEach(o=>(o.team==='A'?a:b).appendChild(li(o.name,o.wpn==='smg'?'PM':'fusil')));
 $('help').textContent=coarse?'Joystick à gauche pour avancer (pousse à fond pour courir), glisse à droite pour viser. Boutons : tir, visée, grenade, soin, saut, accroupi, recharge. Touche l’arme pour passer au pistolet.'
  :'ZQSD ou WASD pour bouger, souris pour viser, clic gauche pour tirer, clic droit pour épauler. Maj : courir · C : accroupi · Espace : saut · R : recharger · G : grenade · H : soin · Q : changer d’arme · Échap : pause.';
 try{const s=localStorage.getItem('fdf');if(s){const o=JSON.parse(s);if(W[o.w]&&o.w!=='pistol')G.primary=o.w;if(o.d>=0&&o.d<3)G.diff=o.d;if(o.s)G.sens=clamp(+o.s,.4,2.2)}}catch(e){}
 const save=()=>{try{localStorage.setItem('fdf',JSON.stringify({w:G.primary,d:G.diff,s:G.sens}))}catch(e){}};
 const sync=()=>{document.querySelectorAll('#optW .opt').forEach(o=>o.classList.toggle('sel',o.dataset.w===G.primary));document.querySelectorAll('#optD .opt').forEach(o=>o.classList.toggle('sel',+o.dataset.d===G.diff));$('sens').value=G.sens};sync();
 $('optW').addEventListener('click',e=>{const o=e.target.closest('.opt');if(o){G.primary=o.dataset.w;sync();save()}});$('optD').addEventListener('click',e=>{const o=e.target.closest('.opt');if(o){G.diff=+o.dataset.d;sync();save()}});
 $('sens').addEventListener('input',e=>{G.sens=+e.target.value;save()});$('deploy').addEventListener('click',startMatch);$('resume').addEventListener('click',()=>setPause(false));$('quit').addEventListener('click',toLobby);$('again').addEventListener('click',toLobby);
 $('load').hidden=true;$('deploy').hidden=false;cam.fov=50;cam.updateProjectionMatrix()}
// ---------- boucle ----------
let last=performance.now();
function frame(now){requestAnimationFrame(frame);const dt=Math.min(.05,(now-last)/1000);last=now;
 if(G.state==='play'){G.t+=dt;G.time-=dt;updPlayer(dt);for(const b of bots)b.update(dt);updGren(dt);updHud(dt);if(G.time<=0)endMatch()}
 else if(G.state==='lobby'){const a=now*.00012+.4;cam.position.set(.4+Math.sin(a)*4.6,H(.4,88)+1.5,88+Math.cos(a)*4.6-1.2);cam.lookAt(.4,H(.4,88)+1.15,88)}
 if(G.state!=='pause'){updFx(dt);updTr(dt);updAmb(dt);shake=Math.max(0,shake-dt*3);flashL.intensity*=Math.max(0,1-dt*30);explL.intensity*=Math.max(0,1-dt*7)}
 sky.position.copy(cam.position);sun.position.copy(cam.position).addScaledVector(sunDir,160);sun.target.position.copy(cam.position);vSun.position.copy(sunDir).applyQuaternion(_q2.copy(cam.quaternion).invert());
 renderer.clear();renderer.render(scene,cam);if(G.state==='play'||G.state==='pause'){renderer.clearDepth();renderer.render(vScene,vCam)}window.__ready=true}
requestAnimationFrame(frame);
