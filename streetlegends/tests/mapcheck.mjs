import * as M from '../src/world/map.js';
console.log(M.worldStats());
// croisements involontaires
let cross=0;const segs=[];
for(const e of M.edges)for(let i=0;i<e.pts.length-1;i++)segs.push([e,e.pts[i],e.pts[i+1]]);
const ccw=(a,b,c)=>(c.z-a.z)*(b.x-a.x)>(b.z-a.z)*(c.x-a.x);
const bad=new Set();
for(let i=0;i<segs.length;i++)for(let j=i+1;j<segs.length;j++){const[A,a,b]=segs[i],[B,c,d]=segs[j];if(A===B)continue;
 if(Math.max(a.x,b.x)<Math.min(c.x,d.x)||Math.max(c.x,d.x)<Math.min(a.x,b.x)||Math.max(a.z,b.z)<Math.min(c.z,d.z)||Math.max(c.z,d.z)<Math.min(a.z,b.z))continue;
 const sh=[A.a,A.b].some(n=>n===B.a||n===B.b);
 if(ccw(a,c,d)!==ccw(b,c,d)&&ccw(a,b,c)!==ccw(a,b,d)){ if(sh){const ns=[A.a,A.b].filter(n=>n===B.a||n===B.b).map(n=>M.nodes[n]);const ix=(a.x+b.x)/2,iz=(a.z+b.z)/2;if(ns.some(n=>Math.hypot(n.x-ix,n.z-iz)<30))continue;}
  bad.add(A.id+'('+A.type+' '+Math.round(a.x)+','+Math.round(a.z)+') x '+B.id+'('+B.type+')');}}
console.log('crossings',bad.size,[...bad].slice(0,20));
const deg=M.nodes.filter(n=>n.edges.length===1);console.log('deadends',deg.map(n=>[n.x,n.z]));
// connectivité
const p=M.findPath(0,M.nodes.length-1);console.log('path',p&&p.length);
let unreachable=0;for(const n of M.nodes){if(!M.findPath(0,n.id))unreachable++;}console.log('unreachable',unreachable);
const kinds={};for(const b of M.buildings)kinds[b.zone]=(kinds[b.zone]||0)+1;console.log(kinds);
let maxg=0;for(const e of M.edges)for(let i=1;i<e.pts.length;i++){const g=Math.abs(e.pts[i].y-e.pts[i-1].y)/(e.pts[i].s-e.pts[i-1].s);if(g>maxg)maxg=g;}console.log('max grade',maxg);
let t=Date.now();let s=0;for(let i=0;i<200000;i++)s+=M.groundHeight((i*37)%5000-2500,(i*91)%4000-2400);console.log('gh ms/200k',Date.now()-t);
const gt={};for(const e of M.edges)for(let i=1;i<e.pts.length;i++){const g=Math.abs(e.pts[i].y-e.pts[i-1].y)/(e.pts[i].s-e.pts[i-1].s);gt[e.type]=Math.max(gt[e.type]||0,g);}console.log(gt);
let my=0;for(const e of M.edges)for(const p of e.pts)my=Math.max(my,p.y);console.log('max road y',my);
let md=0;for(const e of M.edges)for(const p of e.pts)md=Math.max(md,Math.abs(p.y-M.baseHeight(p.x,p.z)));console.log('max cut/fill',md);
