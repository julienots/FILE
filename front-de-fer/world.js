'use strict';
// ====== Front de Fer — monde : textures procédurales, terrain, décor, collisions ======
const V3=THREE.Vector3,$=id=>document.getElementById(id);
const C=h=>new THREE.Color(h).convertSRGBToLinear();
const clamp=(v,a,b)=>v<a?a:v>b?b:v,lerp=(a,b,t)=>a+(b-a)*t,sst=(a,b,x)=>{x=clamp((x-a)/(b-a),0,1);return x*x*(3-2*x)};
let seed=7;const rnd=()=>{seed=(seed*16807)%2147483647;return seed/2147483647},rr=(a,b)=>a+rnd()*(b-a);
const R=(a,b)=>a+Math.random()*(b-a);
const coarse=matchMedia('(pointer:coarse)').matches;
// ---------- bruit ----------
function hsh(x,y){const s=Math.sin(x*127.1+y*311.7)*43758.5453;return s-Math.floor(s)}
function vn(x,y,P){const xi=Math.floor(x),yi=Math.floor(y),xf=x-xi,yf=y-yi,u=xf*xf*(3-2*xf),v=yf*yf*(3-2*yf);
 const m=P?(n=>((n%P)+P)%P):(n=>n);const a=hsh(m(xi),m(yi)),b=hsh(m(xi+1),m(yi)),c=hsh(m(xi),m(yi+1)),d=hsh(m(xi+1),m(yi+1));
 return a+(b-a)*u+(c-a)*v+(a-b-c+d)*u*v}
function fbm(x,y,o,P){let t=0,a=.5,f=1,n=0;for(let i=0;i<o;i++){t+=a*vn(x*f,y*f,P?P*f:0);n+=a;f*=2;a*=.5}return t/n}
// ---------- textures ----------
function tex(S,px,after,srgb){const c=document.createElement('canvas');c.width=c.height=S;const g=c.getContext('2d');
 if(px){const id=g.createImageData(S,S),d=id.data;for(let y=0;y<S;y++)for(let x=0;x<S;x++){const o=px(x/S,y/S,x,y),i=(y*S+x)*4;d[i]=o[0];d[i+1]=o[1];d[i+2]=o[2];d[i+3]=o[3]===undefined?255:o[3]}g.putImageData(id,0,0)}
 if(after)after(g,S);const t=new THREE.CanvasTexture(c);t.wrapS=t.wrapT=THREE.RepeatWrapping;if(srgb!==false)t.encoding=THREE.sRGBEncoding;t.anisotropy=coarse?4:8;return t}
const T={};
T.ground=tex(512,(u,v,x,y)=>{const n=fbm(u*6,v*6,5,6),m=fbm(u*40,v*40,3,40);let l=150+n*70+m*45-(hsh(x,y)>.975?70:0);return[l,l,l]},
 (g,S)=>{for(let i=0;i<5000;i++){const x=rnd()*S,y=rnd()*S,l=4+rnd()*9,a=rr(-.9,.9),k=rnd()>.5?255:40;g.strokeStyle=`rgba(${k},${k},${k},${rr(.08,.22)})`;g.lineWidth=1;g.beginPath();g.moveTo(x,y);g.lineTo(x+Math.sin(a)*l,y-Math.cos(a)*l);g.stroke()}});
T.ground.repeat.set(76,76);
T.wood=tex(512,(u,v)=>{const row=Math.floor(v*6),rv=hsh(row,3),off=rv*7;const gr=fbm(u*3+off,v*90+off,4,0),gr2=fbm(u*60+off,v*240,2,0);
 let l=.55+rv*.3+(gr-.5)*.55+(gr2-.5)*.25;const fy=(v*6)%1;if(fy<.035||fy>.975)l*=.25;else if(fy<.07)l*=.75;
 const kx=(u*3+rv*5)%1,ky=fy-.5,kd=Math.hypot((kx-.5)*2.2,ky*1.2);if(kd<.09)l*=.45+kd*5;
 const moss=sst(.55,.75,fbm(u*5+9,v*5,3,5))*.5;return[lerp(118*l,70*l+20,moss),lerp(96*l,92*l+18,moss),lerp(70*l,48*l,moss)]});
T.bark=tex(256,(u,v)=>{const g=fbm(u*22,v*3,4,0);const l=.35+g*.9;return[92*l,74*l,56*l]});
T.sand=tex(256,(u,v,x,y)=>{const w=(Math.sin(x*1.6)*Math.sin(y*1.6))*.07,n=fbm(u*5,v*5,4,5);let l=.72+w+n*.35;if(hsh(x,y)>.97)l*=.8;return[176*l,158*l,118*l]});
T.cloth=tex(128,(u,v,x,y)=>{const w=(Math.sin(x*2.4)*Math.sin(y*2.4))*.06,n=fbm(u*6,v*6,3,6);const l=.8+w+n*.3;return[255*l,255*l,255*l]});
T.conc=tex(512,(u,v,x,y)=>{const n=fbm(u*5,v*5,5,5),s=fbm(u*2,v*26,3,0);let l=.6+n*.4-(s>.62?(s-.62)*1.2:0);const by=(v*5)%1;if(by<.02)l*=.72;if(hsh(x,y)>.985)l*=.7;return[150*l,148*l,138*l]});
T.stone=tex(512,(u,v,x,y)=>{const row=Math.floor(v*10),bx=(u*5+(row%2)*.5)%1,by=(v*10)%1,id=hsh(Math.floor(u*5+(row%2)*.5),row);
 let l=.62+id*.3+fbm(u*30,v*30,3,30)*.3;if(bx<.035||by<.07)l=.36+fbm(u*50,v*50,2,50)*.2;const st=sst(.55,.8,fbm(u*4+2,v*4,3,4));return[lerp(165,120,st)*l,lerp(150,118,st)*l,lerp(128,100,st)*l]});
T.metal=tex(512,(u,v)=>{const n=fbm(u*7,v*7,5,7),st=fbm(u*30,v*3,3,0);let l=.7+n*.45-st*.2;const ru=sst(.6,.78,fbm(u*9+4,v*9+2,4,9));
 return[lerp(88*l,125*l,ru),lerp(96*l,78*l,ru),lerp(62*l,48*l,ru)]});
T.pine=tex(512,null,(g,S)=>{g.clearRect(0,0,S,S);g.strokeStyle='#3a2c20';g.lineWidth=7;g.beginPath();g.moveTo(S/2,S);g.lineTo(S/2,S*.06);g.stroke();
 for(let i=0;i<150;i++){const t=Math.pow(rnd(),.8),y=S*(.03+t*.82),w=(.05+t*.42)*S*rr(.65,1),dir=rnd()>.5?1:-1,droop=rr(.15,.45)*w;
  for(let k=0;k<26;k++){const f=k/26,bx=S/2+dir*w*f,by=y+droop*f*f+rr(-3,3),sh=rr(0,1),top=1-t*.45;
   g.strokeStyle=`rgba(${(22+sh*40)*top|0},${(46+sh*52)*top|0},${(26+sh*22)*top|0},.95)`;g.lineWidth=rr(1.2,2.6);
   for(let n=0;n<5;n++){const a=rr(0,6.28),l=rr(5,13);g.beginPath();g.moveTo(bx,by);g.lineTo(bx+Math.cos(a)*l,by+Math.sin(a)*l*.7+3);g.stroke()}}}});
T.grass=tex(256,null,(g,S)=>{g.clearRect(0,0,S,S);for(let i=0;i<150;i++){const x=rr(.08,.92)*S,h=rr(.3,.95)*S,b=rr(-.22,.22)*S,sh=rnd();
 g.strokeStyle=`rgb(${70+sh*90|0},${92+sh*72|0},${36+sh*30|0})`;g.lineWidth=rr(1.5,3.6);g.beginPath();g.moveTo(x,S);g.quadraticCurveTo(x+b*.2,S-h*.6,x+b,S-h);g.stroke()}});
T.cloud=tex(256,(u,v)=>{const d=Math.hypot(u-.5,(v-.5)*1.7)*2,n=fbm(u*5+3,v*5,5,0),a=clamp((n-.32-d*.42)*3.2,0,1),sh=clamp(.78+(.6-v)*.5+n*.2,0,1);return[255*sh,255*sh,255*sh,a*235]});
T.puff=tex(128,(u,v)=>{const d=Math.hypot(u-.5,v-.5)*2,n=fbm(u*6+1,v*6,4,0),a=clamp((1-d)*1.6-(1-n)*.7,0,1);return[255,255,255,a*255]});
T.glow=tex(128,(u,v)=>{const d=Math.hypot(u-.5,v-.5)*2,a=Math.pow(clamp(1-d,0,1),2.2);return[255,255,255,a*255]});
T.flash=tex(128,(u,v)=>{const x=u-.5,y=v-.5,d=Math.hypot(x,y)*2,an=Math.atan2(y,x),sp=Math.pow(Math.abs(Math.cos(an*2.5)),6),a=clamp(Math.pow(clamp(1-d,0,1),1.6)*(.55+sp*1.2),0,1);return[255,235-d*60,170-d*120,a*255]});
// ---------- rendu ----------
const renderer=new THREE.WebGLRenderer({canvas:$('c'),antialias:!coarse,powerPreference:'high-performance'});
renderer.setPixelRatio(Math.min(devicePixelRatio,coarse?1.35:2));renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;
renderer.outputEncoding=THREE.sRGBEncoding;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=.95;renderer.autoClear=false;
const scene=new THREE.Scene(),cam=new THREE.PerspectiveCamera(68,1,.08,1500);cam.rotation.order='YXZ';
const FOG=C(0xc3ccd0);scene.fog=new THREE.FogExp2(FOG,.0095);
const sunDir=new V3(-.52,.5,-.7).normalize();
const sky=new THREE.Mesh(new THREE.SphereGeometry(900,32,16),new THREE.ShaderMaterial({side:THREE.BackSide,depthWrite:false,fog:false,
 uniforms:{top:{value:C(0x3f73b8)},hor:{value:FOG},sd:{value:sunDir}},
 vertexShader:'varying vec3 d;void main(){d=position;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}',
 fragmentShader:'uniform vec3 top,hor,sd;varying vec3 d;void main(){vec3 n=normalize(d);float t=clamp(n.y,0.,1.);vec3 c=mix(hor,top,pow(t,.55));float s=max(dot(n,sd),0.);c+=vec3(1.,.9,.7)*(pow(s,900.)*6.+pow(s,24.)*.5+pow(s,4.)*.18);gl_FragColor=vec4(c,1.);}'}));
scene.add(sky);
scene.add(new THREE.HemisphereLight(C(0xc4d6ee),C(0x8f8668),1.15),new THREE.AmbientLight(C(0xdfe6f0),.22));
const sun=new THREE.DirectionalLight(C(0xffe6bc),2.3);sun.castShadow=true;{const sc=sun.shadow.camera;sc.left=-65;sc.right=65;sc.top=65;sc.bottom=-65;sc.near=1;sc.far=320}
sun.shadow.mapSize.set(coarse?2048:4096,coarse?2048:4096);sun.shadow.bias=-.0004;sun.shadow.normalBias=.04;scene.add(sun,sun.target);
const SM=o=>new THREE.MeshStandardMaterial(Object.assign({roughness:.9,metalness:0},o));
// ---------- tranchées et relief ----------
const MAP=100,HW=1.3,DEPTH=1.3;
const TR=[[[-90,-4],[-68,5],[-46,-3],[-24,5],[-4,-2],[18,5],[40,-3],[62,4],[90,-2]],
 [[-55,46],[-30,52],[-6,45],[20,52],[46,45]],[[-50,-48],[-26,-54],[0,-46],[24,-54],[50,-47]],
 [[-6,45],[-8,22],[-4,-2]],[[24,-54],[22,-26],[18,5]]];
const SEG=[];TR.forEach(pl=>{for(let i=0;i<pl.length-1;i++){const[ax,az]=pl[i],[bx,bz]=pl[i+1],len=Math.hypot(bx-ax,bz-az);SEG.push({ax,az,bx,bz,len,dx:(bx-ax)/len,dz:(bz-az)/len})}});
function segD(s,x,z){const t=clamp(((x-s.ax)*s.dx+(z-s.az)*s.dz),0,s.len);return Math.hypot(x-s.ax-s.dx*t,z-s.az-s.dz*t)}
function trD(x,z,skip){let m=1e9;for(const s of SEG){if(s===skip)continue;const d=segD(s,x,z);if(d<m)m=d}return m}
const CR=[];for(let i=0;i<46;i++){const x=rr(-88,88),z=rr(-72,72);if(trD(x,z)>6)CR.push({x,z,r:rr(2,4.5)})}
function baseH(x,z){return(fbm(x*.011+3,z*.011+7,4)-.5)*9+(fbm(x*.06,z*.06,2)-.5)*.7}
function Hraw(x,z){let h=baseH(x,z);const d=trD(x,z);if(d<HW+.5)h-=DEPTH*sst(HW+.5,HW,d);
 for(const c of CR){const q=Math.hypot(x-c.x,z-c.z)/c.r;if(q<1.5)h+=q<1?-(1-q*q)*c.r*.26:Math.sin((q-1)*6.283)*.22*(1.5-q)}return h}
const GN=481,GS=.5,GO=-120,HG=new Float32Array(GN*GN);
for(let j=0;j<GN;j++)for(let i=0;i<GN;i++)HG[j*GN+i]=Hraw(GO+i*GS,GO+j*GS);
function H(x,z){const fx=clamp((x-GO)/GS,0,GN-1.001),fz=clamp((z-GO)/GS,0,GN-1.001),i=fx|0,j=fz|0,u=fx-i,v=fz-j,k=j*GN+i;
 return(HG[k]*(1-u)+HG[k+1]*u)*(1-v)+(HG[k+GN]*(1-u)+HG[k+GN+1]*u)*v}
{const S=240,N=coarse?320:420,g=new THREE.PlaneGeometry(S,S,N,N);g.rotateX(-Math.PI/2);const p=g.attributes.position,col=new Float32Array(p.count*3);
 const grass=C(0x6e7a3e),dry=C(0x9a8f58),mud=C(0x5b4a36),wet=C(0x3d3226),tmp=new THREE.Color();
 for(let i=0;i<p.count;i++){const x=p.getX(i),z=p.getZ(i);p.setY(i,Hraw(x,z));const d=trD(x,z);
  tmp.copy(grass).lerp(dry,sst(.45,.7,fbm(x*.09+20,z*.09,3)));tmp.multiplyScalar(.8+fbm(x*.3,z*.3,2)*.45);
  let m=1-sst(1.7,4.2+fbm(x*.2,z*.2,2)*3,d);for(const c of CR){const q=Math.hypot(x-c.x,z-c.z)/c.r;if(q<1.6)m=Math.max(m,1-sst(.9,1.6,q))}
  m=Math.max(m,sst(.6,.75,fbm(x*.05+50,z*.05+9,3))*.8);tmp.lerp(mud,m);if(d<HW)tmp.lerp(wet,.75);tmp.toArray(col,i*3)}
 g.setAttribute('color',new THREE.BufferAttribute(col,3));g.computeVertexNormals();
 const m=new THREE.Mesh(g,SM({map:T.ground,bumpMap:T.ground,bumpScale:.05,vertexColors:true,roughness:.95}));m.receiveShadow=true;scene.add(m);
 const og=new THREE.PlaneGeometry(2400,2400,80,80);og.rotateX(-Math.PI/2);const op=og.attributes.position;
 for(let i=0;i<op.count;i++){const x=op.getX(i),z=op.getZ(i),r=Math.hypot(x,z);op.setY(i,baseH(x,z)-.6-(Math.max(Math.abs(x),Math.abs(z))<100?9:0)+Math.max(0,r-200)*.11*(.4+fbm(x*.004,z*.004,3)*1.6))}og.computeVertexNormals();
 scene.add(new THREE.Mesh(og,new THREE.MeshLambertMaterial({color:C(0x56633a)})))}
// ---------- collisions et lignes de tir ----------
const colBoxes=[],colCirc=[],losBoxes=[],reserved=[];
function solid(x,y,z,hx,hy,hz,ry,o){o=o||{};const b={x,y,z,hx,hy,hz,c:Math.cos(ry||0),s:Math.sin(ry||0),top:y+hy,bot:y-hy,mat:o.mat||'wood',trench:!!o.trench,rad:Math.hypot(hx,hz)};
 if(o.col!==false)colBoxes.push(b);if(o.los!==false)losBoxes.push(b);return b}
function reserve(x,z,r){reserved.push({x,z,r})}
function isFree(x,z,pad){if(trD(x,z)<3.2+(pad||0))return false;for(const r of reserved)if(Math.hypot(x-r.x,z-r.z)<r.r+(pad||0))return false;return true}
function collide(pos,rad,feet,noTrench){
 for(let i=0;i<colBoxes.length;i++){const b=colBoxes[i];const dx=pos.x-b.x,dz=pos.z-b.z;if(Math.abs(dx)>b.rad+rad||Math.abs(dz)>b.rad+rad)continue;
  if(b.top<feet+.6||b.bot>feet+1.7||(noTrench&&b.trench))continue;
  const lx=dx*b.c-dz*b.s,lz=dx*b.s+dz*b.c;if(Math.abs(lx)>b.hx+rad||Math.abs(lz)>b.hz+rad)continue;
  const cx=clamp(lx,-b.hx,b.hx),cz=clamp(lz,-b.hz,b.hz);let ox=lx-cx,oz=lz-cz;const d=Math.hypot(ox,oz);
  if(d<1e-5){const px=b.hx+rad-Math.abs(lx),pz=b.hz+rad-Math.abs(lz);if(px<pz){ox=(lx<0?-1:1)*px;oz=0}else{ox=0;oz=(lz<0?-1:1)*pz}}
  else if(d<rad){const k=(rad-d)/d;ox*=k;oz*=k}else continue;
  pos.x+=ox*b.c+oz*b.s;pos.z+=-ox*b.s+oz*b.c}
 for(let i=0;i<colCirc.length;i++){const c=colCirc[i],dx=pos.x-c.x,dz=pos.z-c.z,rs=c.r+rad;if(Math.abs(dx)>rs||Math.abs(dz)>rs)continue;const d=Math.hypot(dx,dz);if(d<rs&&d>1e-5){const k=(rs-d)/d;pos.x+=dx*k;pos.z+=dz*k}}
 pos.x=clamp(pos.x,-MAP,MAP);pos.z=clamp(pos.z,-MAP,MAP)}
function rayOBB(ox,oy,oz,dx,dy,dz,b,maxT){const px=ox-b.x,py=oy-b.y,pz=oz-b.z;
 const lo=[px*b.c-pz*b.s,py,px*b.s+pz*b.c],ld=[dx*b.c-dz*b.s,dy,dx*b.s+dz*b.c],h=[b.hx,b.hy,b.hz];let t0=0,t1=maxT;
 for(let a=0;a<3;a++){if(Math.abs(ld[a])<1e-8){if(Math.abs(lo[a])>h[a])return-1}else{let u=(-h[a]-lo[a])/ld[a],w=(h[a]-lo[a])/ld[a];if(u>w){const s=u;u=w;w=s}if(u>t0)t0=u;if(w<t1)t1=w;if(t0>t1)return-1}}return t0}
function rayWorld(ox,oy,oz,dx,dy,dz,maxT,step){let best=maxT,hit=null;
 for(let i=0;i<losBoxes.length;i++){const b=losBoxes[i];const cx=b.x-ox,cz=b.z-oz,pr=cx*dx+cz*dz;if(pr<-b.rad-1||pr>best+b.rad)continue;
  const t=rayOBB(ox,oy,oz,dx,dy,dz,b,best);if(t>=0&&t<best){best=t;hit=b}}
 step=step||1;for(let t=step;t<best;t+=step){if(oy+dy*t<H(ox+dx*t,oz+dz*t)){let a=t-step,c=t;for(let i=0;i<5;i++){const m=(a+c)/2;if(oy+dy*m<H(ox+dx*m,oz+dz*m))c=m;else a=m}return{t:c,mat:'dirt'}}}
 return hit?{t:best,mat:hit.mat}:null}
function los(ax,ay,az,bx,by,bz){const dx=bx-ax,dy=by-ay,dz=bz-az,l=Math.hypot(dx,dy,dz);if(l<.01)return true;return!rayWorld(ax,ay,az,dx/l,dy/l,dz/l,l-.3,2.2)}
// ---------- géométrie fusionnée ----------
const B={},M4=new THREE.Matrix4(),Q=new THREE.Quaternion(),E=new THREE.Euler(),ONE=new V3(1,1,1);
function put(b,geo,x,y,z,ry,rx,rz,sx,sy,sz){E.set(rx||0,ry||0,rz||0,'YXZ');Q.setFromEuler(E);M4.compose(new V3(x,y,z),Q,new V3(sx||1,sy||1,sz||1));const g=geo.index?geo.toNonIndexed():geo.clone();g.applyMatrix4(M4);(B[b]=B[b]||[]).push(g)}
function box(b,w,h,d,x,y,z,ry,rx,rz,ts,sol){const g=new THREE.BoxGeometry(w,h,d),uv=g.attributes.uv,s=ts||2,dm=[[d,h],[d,h],[w,d],[w,d],[w,h],[w,h]],ox=rnd(),oy=rnd();
 for(let f=0;f<6;f++)for(let k=0;k<4;k++){const i=f*4+k;uv.setXY(i,uv.getX(i)*dm[f][0]/s+ox,uv.getY(i)*dm[f][1]/s+oy)}put(b,g,x,y,z,ry,rx,rz);if(sol)solid(x,y,z,w/2,h/2,d/2,ry,sol)}
function cyl(b,r1,r2,a,c,seg){const d=new V3().subVectors(c,a),l=d.length(),g=new THREE.CylinderGeometry(r1,r2,l,seg||10);Q.setFromUnitVectors(new V3(0,1,0),d.normalize());
 M4.compose(new V3().addVectors(a,c).multiplyScalar(.5),Q,ONE);const n=g.toNonIndexed();n.applyMatrix4(M4);(B[b]=B[b]||[]).push(n)}
function mergeG(gs){let n=0;gs.forEach(g=>n+=g.attributes.position.count);const P=new Float32Array(n*3),N=new Float32Array(n*3),U=new Float32Array(n*2);let k=0,hasC=!!gs[0].attributes.color,Cc=hasC?new Float32Array(n*3):null;
 gs.forEach(g=>{P.set(g.attributes.position.array,k*3);N.set(g.attributes.normal.array,k*3);if(g.attributes.uv)U.set(g.attributes.uv.array,k*2);if(hasC)Cc.set(g.attributes.color.array,k*3);k+=g.attributes.position.count});
 const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.BufferAttribute(P,3));g.setAttribute('normal',new THREE.BufferAttribute(N,3));g.setAttribute('uv',new THREE.BufferAttribute(U,2));if(hasC)g.setAttribute('color',new THREE.BufferAttribute(Cc,3));return g}
function build(b,mat){const gs=B[b];if(!gs)return;const m=new THREE.Mesh(mergeG(gs),mat);m.castShadow=true;m.receiveShadow=true;scene.add(m);delete B[b];return m}
// ---------- tranchées : planches, rondins, sacs de sable ----------
const bagG=new THREE.SphereGeometry(1,8,5);
function bags(x0,z0,x1,z1,rows,y0,sol){const len=Math.hypot(x1-x0,z1-z0),ry=Math.atan2(-(z1-z0),x1-x0),n=Math.max(1,Math.floor(len/.56));
 for(let r=0;r<rows;r++)for(let i=0;i<n-(r%2);i++){const f=(i+.5+(r%2)*.5)/n,x=lerp(x0,x1,f)+rr(-.03,.03),z=lerp(z0,z1,f)+rr(-.03,.03);
  put('sand',bagG,x,(y0===undefined?H(x,z):y0)+.1+r*.185,z,ry+rr(-.12,.12),rr(-.06,.06),rr(-.06,.06),.31,.115,.19)}
 if(sol){const mx=(x0+x1)/2,mz=(z0+z1)/2,by=H(mx,mz);solid(mx,by+rows*.09,mz,len/2,rows*.1+.05,.2,ry,{mat:'sand'});reserve(mx,mz,len/2+.5)}}
SEG.forEach(s=>{const n=Math.ceil(s.len/1.5),pl=s.len/n,nx=-s.dz,nz=s.dx,ry=Math.atan2(-s.dz,s.dx);
 for(let i=0;i<n;i++){const t=(i+.5)*pl,cx=s.ax+s.dx*t,cz=s.az+s.dz*t,fl=baseH(cx,cz)-DEPTH;
  if(trD(cx,cz,s)>HW+.3)box('duck',pl+.02,.05,.85,cx,fl+.05,cz,ry+rr(-.03,.03),0,0,1.6);
  for(const sd of[-1,1]){const wx=cx+nx*HW*sd,wz=cz+nz*HW*sd;if(trD(wx,wz,s)<HW-.05)continue;const top=baseH(wx,wz)+.12,hh=(top-fl+.15)/2;
   if((i+(sd>0?3:0))%9===4)continue; // passage pour sortir de la tranchée
   for(let k=0;k<2;k++)box('wood',pl+.03,hh-.012,.05,wx+nx*sd*rr(0,.02),fl-.15+hh*(k+.5),wz+nz*sd*rr(0,.02),ry,0,rr(-.012,.012),1.7);
   solid(wx,fl-.15+hh,wz,pl/2+.02,hh,.06,ry,{trench:true,mat:'wood'});
   const px=wx-nx*sd*.07-s.dx*pl/2,pz=wz-nz*sd*.07-s.dz*pl/2;cyl('post',.055,.065,new V3(px,fl-.2,pz),new V3(px+rr(-.03,.03),top+rr(.05,.3),pz+rr(-.03,.03)),6);
   if(hsh(Math.floor(t/7)+sd*3,s.ax+s.az)>.6){const ox=wx+nx*sd*.3,oz=wz+nz*sd*.3;bags(ox-s.dx*pl/2,oz-s.dz*pl/2,ox+s.dx*pl/2,oz+s.dz*pl/2,2)}}}});
// ---------- décor ----------
function hedgehog(x,z){if(!isFree(x,z))return;const y=H(x,z)+.55,a=rr(0,3);for(const[rx,rz]of[[.9,.6],[-.9,.6],[0,1.2]])box('steel',.1,1.55,.1,x,y-.1,z,a,rx,rz,1.2);
 colCirc.push({x,z,r:.55});solid(x,y,z,.45,.6,.45,a,{col:false,mat:'metal'});reserve(x,z,1.2)}
for(let i=0;i<34;i++){const sd=i%2?1:-1;hedgehog(rr(-85,85),sd*rr(14,32))}
function wire(x0,z0,x1,z1){const L=Math.hypot(x1-x0,z1-z0),n=Math.floor(L/2.2);for(let i=0;i<=n;i++){const f=i/n,x=lerp(x0,x1,f),z=lerp(z0,z1,f),y=H(x,z);
  cyl('post',.04,.05,new V3(x,y-.2,z),new V3(x+rr(-.08,.08),y+1.05,z+rr(-.08,.08)),6);
  if(i<n){const x2=lerp(x0,x1,(i+1)/n),z2=lerp(z0,z1,(i+1)/n),y2=H(x2,z2),pts=[];for(let k=0;k<=50;k++){const u=k/50,a=u*Math.PI*2*5;pts.push(new V3(lerp(x,x2,u)+Math.cos(a)*.05,lerp(y,y2,u)+.45+Math.sin(a)*.38,lerp(z,z2,u)+Math.cos(a)*.38))}
   (B.wire=B.wire||[]).push(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts),90,.012,3).toNonIndexed())}}
 const mx=(x0+x1)/2,mz=(z0+z1)/2;solid(mx,H(mx,mz)+.45,mz,L/2,.45,.4,Math.atan2(-(z1-z0),x1-x0),{los:false});for(let i=0;i<=n;i++)reserve(lerp(x0,x1,i/n),lerp(z0,z1,i/n),1.6)}
wire(30,12,48,14);wire(-48,13,-30,11);wire(-20,-13,-38,-15);wire(52,-12,70,-14);wire(-76,-12,-62,-11);wire(68,13,82,11);
function crate(x,z,s,ry,y){const by=(y===undefined?H(x,z):y);box('crate',s,s*.62,s*.7,x,by+s*.31,z,ry,0,0,1.1,{mat:'wood'});reserve(x,z,s)}
function crates(x,z){if(!isFree(x,z,1))return;const a=rr(0,3);crate(x,z,1.2,a);crate(x+1.25,z+.3,1,a+.4);crate(x+.2,z+.1,.85,a+.2,H(x,z)+.74);if(rnd()>.5)crate(x-1.1,z+.9,.9,a-.5)}
[[0,88],[-18,84],[22,90],[-40,90],[45,86],[0,-88],[-20,-85],[20,-90],[-45,-88],[42,-86],[-30,24],[34,-22],[60,-30],[-62,30],[4,26],[-6,-28],[72,34],[-74,-34],[-52,-8],[50,9]].forEach(p=>crates(p[0],p[1]));
// murets de sacs de sable (abris)
[[-14,14,0],[12,16,.3],[36,22,-.2],[-36,18,.2],[58,16,0],[-60,14,.1],[-12,-16,.1],[14,-18,-.2],[38,-14,.2],[-34,-22,0],[62,-18,.1],[-58,-20,-.1],[0,70,0],[-28,68,.2],[30,70,-.2],[0,-70,0],[-30,-70,.1],[28,-68,-.1],[80,0,1.5],[-80,2,1.5]].forEach(p=>{
 const x=p[0],z=p[1];if(!isFree(x,z,1.5))return;const L=rr(3,5)/2,c=Math.cos(p[2]),s=Math.sin(p[2]);bags(x-c*L,z+s*L,x+c*L,z-s*L,4,undefined,true)});
// bunkers
function bunker(x,z,ry){const y=H(x,z)-.2,cs=Math.cos(ry),sn=Math.sin(ry);const L=(lx,ly,lz)=>[x+lx*cs+lz*sn,y+ly,z-lx*sn+lz*cs];const so={mat:'stone'};
 let p=L(0,.6,2.4);box('conc',8,1.2,.7,p[0],p[1],p[2],ry,0,0,3,so);p=L(0,2.05,2.4);box('conc',8,.9,.7,p[0],p[1],p[2],ry,0,0,3,so);
 for(const sx of[-3.65,3.65]){p=L(sx,1.25,0);box('conc',.7,2.5,5.4,p[0],p[1],p[2],ry,0,0,3,so)}
 for(const sx of[-2.6,2.6]){p=L(sx,1.25,-2.4);box('conc',2.8,2.5,.6,p[0],p[1],p[2],ry,0,0,3,so)}p=L(0,2.25,-2.4);box('conc',2.4,.5,.6,p[0],p[1],p[2],ry,0,0,3,so);
 p=L(0,2.75,0);box('conc',8.8,.55,6.4,p[0],p[1],p[2],ry,0,0,3,so);
 p=L(-2,3.1,0);const q=L(2,3.1,0);bags(p[0],p[2],q[0],q[2],2,y+3.0);reserve(x,z,7)}
bunker(-17,-32,.35);bunker(26,30,Math.PI+.2);
// chars
const smokers=[];
function tank(x,z,ry,burn){const y=H(x,z)-.1,cs=Math.cos(ry),sn=Math.sin(ry);const L=(lx,ly,lz)=>new V3(x+lx*cs+lz*sn,y+ly,z-lx*sn+lz*cs);
 const bx=(b,w,h,d,lx,ly,lz,rx)=>{const p=L(lx,ly,lz);box(b,w,h,d,p.x,p.y,p.z,ry,rx||0,0,3)};
 bx('tank',2.2,.75,6.1,0,.85,0);bx('tank',3.3,.55,5.2,0,1.5,-.2);bx('tank',3.3,.5,1.3,0,1.3,2.75,-.6);bx('tank',3.3,.06,6.5,0,1.24,0);
 for(const s of[-1,1]){bx('track',.58,.2,6.5,s*1.42,1.02,0);bx('track',.58,.14,5.4,s*1.42,.07,0);bx('track',.58,.14,1.1,s*1.42,.5,3.05,-.9);bx('track',.58,.14,1.1,s*1.42,.5,-3.05,.9);
  for(let i=0;i<6;i++)cyl('track',.44,.44,L(s*1.15,.5,-2.5+i),L(s*1.7,.5,-2.5+i),12)}
 const tc=L(0,2.12,-.3);put('tank',new THREE.CylinderGeometry(1.25,1.4,.72,18),tc.x,tc.y,tc.z,ry+.5,0,0,1,1,1.15);
 const t2=L(.2,2.6,-.5);put('tank',new THREE.CylinderGeometry(.32,.36,.25,12),t2.x,t2.y,t2.z);
 const ga=L(.75,2.15,.9),gb=L(2.6,2.05,4.2);cyl('tank',.085,.11,ga,gb,10);cyl('tank',.13,.13,gb,gb.clone().lerp(ga,-.05),10);
 solid(x,y+.9,z,1.72,.95,3.3,ry,{mat:'metal'});solid(tc.x,tc.y+.1,tc.z,1.3,.45,1.4,ry,{mat:'metal',col:false});reserve(x,z,5);if(burn)smokers.push(L(.3,2.6,-.8))}
tank(15,-24,-.75,true);tank(-42,22,2.1,true);tank(66,-52,.4);tank(-70,56,-2.4);
// ruines
function ruin(x,z,ry,w,d){const y=H(x,z)-.3,cs=Math.cos(ry),sn=Math.sin(ry);const L=(lx,lz)=>[x+lx*cs+lz*sn,z-lx*sn+lz*cs];
 const run=(ax,az,bx,bz,rot)=>{const len=Math.hypot(bx-ax,bz-az),n=Math.round(len/1.6);for(let i=0;i<n;i++){const f=(i+.5)/n,lx=lerp(ax,bx,f),lz=lerp(az,bz,f),p=L(lx,lz),k=hsh(i+ax*3,bz+az);
   if(k<.18)continue;const h=k<.4?rr(.7,1.3):rr(1.9,4.2);box('stone',len/n+.02,h,.4,p[0],y+h/2,p[1],ry+rot,0,0,2.2,{mat:'stone'});
   if(h>2.4&&k>.7){box('stone',len/n+.02,.9,.42,p[0],y+h+.9,p[1],ry+rot,0,0,2.2,{mat:'stone',col:false})}}};
 run(-w/2,d/2,w/2,d/2,0);run(-w/2,-d/2,w/2,-d/2,0);run(-w/2,-d/2,-w/2,d/2,Math.PI/2);run(w/2,-d/2,w/2,d/2,Math.PI/2);
 for(let i=0;i<14;i++){const p=L(rr(-w/2-1.5,w/2+1.5),rr(-d/2-1.5,d/2+1.5)),s=rr(.2,.55);box('stone',s*1.6,s,s,p[0],H(p[0],p[1])+s*.3,p[1],rr(0,3),rr(-.3,.3),rr(-.3,.3),2.2)}reserve(x,z,Math.max(w,d)*.75+1)}
ruin(-62,-30,.3,9,7);ruin(58,26,-.4,10,7);ruin(-32,74,.1,8,6);ruin(40,-76,.2,9,6);ruin(78,-8,1.2,7,6);ruin(-82,10,-1.1,7,6);
// arbres, herbe, rochers
function cards(n){const gs=[];for(let i=0;i<n;i++){const g=new THREE.PlaneGeometry(1,1);g.translate(0,.5,0);g.rotateY(i*Math.PI/n);gs.push(g.toNonIndexed());const g2=g.clone();g2.rotateY(Math.PI);gs.push(g2.toNonIndexed())}
 const g=mergeG(gs),nr=g.attributes.normal;for(let i=0;i<nr.count;i++)nr.setXYZ(i,0,1,0);return g}
const D=new THREE.Object3D(),tcol=new THREE.Color();
{const pts=[];const want=coarse?430:640;for(let i=0;i<9000&&pts.length<want;i++){const x=rr(-340,340),z=rr(-340,340),ax=Math.abs(x),az=Math.abs(z),inside=ax<MAP+8&&az<MAP+8;
  let dens=inside?sst(72,100,ax)*.9+(fbm(x*.03,z*.03,2)>.62?.45:.03):.8;if(inside&&az>78&&ax<64)dens*=.15;if(Math.max(ax,az)>230)dens*=.5;
  if(rnd()>dens)continue;if(inside&&!isFree(x,z,2))continue;pts.push([x,z,rr(11,20)])}
 const pin=new THREE.InstancedMesh(cards(3),new THREE.MeshLambertMaterial({map:T.pine,alphaTest:.4}),pts.length),trk=new THREE.InstancedMesh(new THREE.CylinderGeometry(.5,1,1,7).translate(0,.5,0),new THREE.MeshLambertMaterial({map:T.bark}),pts.length);
 pts.forEach((p,i)=>{const far=Math.max(Math.abs(p[0]),Math.abs(p[1]))>118,y=(far?baseH(p[0],p[1])-.6+Math.max(0,Math.hypot(p[0],p[1])-200)*.1:H(p[0],p[1]))-.3,h=p[2];
  D.position.set(p[0],y+h*.08,p[1]);D.rotation.set(0,rnd()*6,0);D.scale.set(h*.52,h,h*.52);D.updateMatrix();pin.setMatrixAt(i,D.matrix);const k=rr(.5,.9);pin.setColorAt(i,tcol.setRGB(k,k*rr(.95,1.1),k*.9));
  D.position.y=y;D.scale.set(h*.019,h*.6,h*.019);D.updateMatrix();trk.setMatrixAt(i,D.matrix);
  if(!far){colCirc.push({x:p[0],z:p[1],r:h*.017});solid(p[0],y+4,p[1],h*.014,4.5,h*.014,0,{col:false,mat:'wood'});reserve(p[0],p[1],1)}});
 pin.castShadow=trk.castShadow=true;pin.customDepthMaterial=new THREE.MeshDepthMaterial({depthPacking:THREE.RGBADepthPacking,map:T.pine,alphaTest:.4});scene.add(pin,trk);
 const N=coarse?9000:20000,gr=new THREE.InstancedMesh(cards(2),new THREE.MeshLambertMaterial({map:T.grass,alphaTest:.35}),N);let k=0;
 for(let i=0;i<N*3&&k<N;i++){const x=rr(-112,112),z=rr(-112,112);if(trD(x,z)<HW+.75)continue;let cr=0;for(const c of CR)if(Math.hypot(x-c.x,z-c.z)<c.r*1.2){cr=1;break}if(cr)continue;
  if(fbm(x*.05+50,z*.05+9,3)>.66)continue;const s=rr(.35,.95)*(fbm(x*.15,z*.15,2)+.4);D.position.set(x,H(x,z)-.03,z);D.rotation.set(0,rnd()*3,0);D.scale.set(s*1.2,s*.75,s*1.2);D.updateMatrix();gr.setMatrixAt(k,D.matrix);
  const d=fbm(x*.09+20,z*.09,3),q=rr(.36,.56);gr.setColorAt(k,tcol.setRGB(q*(.8+d*.5),q,q*.7));k++}gr.count=k;gr.receiveShadow=true;scene.add(gr);
 const rg=new THREE.IcosahedronGeometry(1,2),rp=rg.attributes.position;for(let i=0;i<rp.count;i++){const x=rp.getX(i),y=rp.getY(i),z=rp.getZ(i),f=.72+fbm(x*1.3+5,y*1.3+z*1.7,3)*.6;rp.setXYZ(i,x*f,y*f*.7,z*f)}rg.computeVertexNormals();
 const RN=70,rk=new THREE.InstancedMesh(rg,SM({map:T.conc,bumpMap:T.conc,bumpScale:.05,roughness:.95,color:C(0xb0aa9c)}),RN);
 for(let i=0;i<RN;i++){let x,z,n=0;do{x=rr(-95,95);z=rr(-80,80)}while(!isFree(x,z,1)&&n++<30);const s=rr(.4,1.7);const y=H(x,z)-s*.15;D.position.set(x,y,z);D.rotation.set(rnd(),rnd()*6,rnd());D.scale.set(s,s,s);D.updateMatrix();rk.setMatrixAt(i,D.matrix);
  if(s>.6){colCirc.push({x,z,r:s*.78});solid(x,y,z,s*.62,s*.55,s*.62,0,{col:false,mat:'stone'})}reserve(x,z,s)}rk.castShadow=rk.receiveShadow=true;scene.add(rk)}
for(let i=0;i<34;i++){const s=new THREE.Sprite(new THREE.SpriteMaterial({map:T.cloud,fog:false,depthWrite:false,opacity:rr(.75,.98)}));const a=rr(0,6.28),r=rr(250,800);s.position.set(Math.sin(a)*r,rr(120,260)+r*.12,Math.cos(a)*r);const w=rr(160,420);s.scale.set(w,w*rr(.3,.45),1);scene.add(s)}
build('wood',SM({map:T.wood,bumpMap:T.wood,bumpScale:.03}));build('duck',SM({map:T.wood,color:C(0xd0c2b0)}));build('crate',SM({map:T.wood,color:C(0xdde0c4)}));
build('post',SM({map:T.bark,color:C(0xb8a890)}));build('sand',SM({map:T.sand,bumpMap:T.sand,bumpScale:.02,roughness:1}));build('conc',SM({map:T.conc,bumpMap:T.conc,bumpScale:.04}));
build('stone',SM({map:T.stone,bumpMap:T.stone,bumpScale:.05}));build('steel',SM({map:T.metal,color:C(0xd8b090),metalness:.5,roughness:.75}));
build('wire',SM({color:C(0x3b332c),metalness:.6,roughness:.6}));build('tank',SM({map:T.metal,bumpMap:T.metal,bumpScale:.02,metalness:.35,roughness:.7}));build('track',SM({color:C(0x2c2a26),metalness:.5,roughness:.8}));
// carte statique pour la mini-carte (1 px = 0,5 m)
const mapCv=document.createElement('canvas');mapCv.width=mapCv.height=480;{const g=mapCv.getContext('2d'),id=g.createImageData(480,480),d=id.data;
 for(let j=0;j<480;j++)for(let i=0;i<480;i++){const x=-120+i*.5,z=-120+j*.5,n=fbm(x*.05+50,z*.05+9,3),k=(j*480+i)*4,m=sst(.55,.75,n),td=trD(x,z);
  let r=lerp(96,120,m),gg=lerp(108,100,m),b=lerp(62,72,m);if(td<1.6){r=58;gg=46;b=34}else if(td<2.6){r=140;gg=126;b=96}d[k]=r;d[k+1]=gg;d[k+2]=b;d[k+3]=255}g.putImageData(id,0,0);
 const px=(x,z)=>[(x+120)*2,(z+120)*2];for(const b of losBoxes){if(b.trench||b.hx<.5)continue;const p=px(b.x,b.z);g.save();g.translate(p[0],p[1]);g.rotate(-Math.atan2(b.s,b.c));g.fillStyle=b.mat==='stone'?'#8f8d84':b.mat==='metal'?'#3c4428':b.mat==='sand'?'#b9a777':'#6b5a3c';g.fillRect(-b.hx*2,-b.hz*2,b.hx*4,b.hz*4);g.restore()}
 g.fillStyle='#2f4526';for(const c of colCirc)if(c.r<.4){const p=px(c.x,c.z);g.beginPath();g.arc(p[0],p[1],3,0,7);g.fill()}}
