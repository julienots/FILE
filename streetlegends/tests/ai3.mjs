import { CARS } from '../src/data/cars.js';
import { Car, collideStatic, resync } from '../src/vehicle/physics.js';
import { buildRoute } from '../src/ai/route.js';
import { AIDriver } from '../src/ai/driver.js';
const which=process.argv[2]||'track';
const R = which==='hwy'?buildRoute([[-1200,-940],[1200,-940],[2060,-60],[1300,720],[-1200,730],[-2060,-60]],{closed:true,allow:(e)=>e.type==='highway'}):which==='mtn'?buildRoute([[-1200,-940],[-800,-2150],[350,-2050],[1200,-940]],{allow:(e)=>e.type==='mountain'}):which==='track'?buildRoute([[2300,350],[2600,-200],[2650,300],[2600,850]],{closed:true,allow:(e)=>e.type==='track'}):buildRoute([[-300,-300],[300,-300],[300,300],[-300,300]],{closed:true});
const spec = CARS.find(c=>c.id===(process.argv[3]||'falcor')); const car = new Car(spec, spec.stats, {assist:{style:'ai'}});
const st = R.at(0,0); car.place(st.x, st.z, st.yaw); const ai = new AIDriver(car, R, process.argv[4]||'technical', 'hard', 3);
let t=0,n=0; const hist=[];
while(t<80&&n<4){ ai.update(1/60,{bodies:[]}); let hit=0; for(let k=0;k<2;k++){car.step(1/120,{grip:1}); const h=collideStatic(car); if(h>0)resync(car); hit=Math.max(hit,h);} t+=1/60;
 hist.push(`t ${t.toFixed(2)} i ${ai.i} v ${car.speed.toFixed(1)} vt ${ai.targetV.toFixed(1)} vprof ${ai.v[ai.i].toFixed(1)} k ${R.k[ai.i].toFixed(3)} lat ${R.lateral(car.x,car.z,ai.i).toFixed(1)} beta ${car.beta.toFixed(2)} in ${car.in.steer.toFixed(2)} th ${car.in.throttle.toFixed(2)} br ${car.in.brake.toFixed(2)} sF ${car.slipF.toFixed(2)} sR ${car.slipR.toFixed(2)} yr ${car.yawRate.toFixed(2)} surf ${car.surf} y ${car.y.toFixed(1)} air ${car.airborne} rev ${car.reverse} stuck ${ai.stuck.toFixed(1)} hb ${car.in.handbrake}`); if(hist.length>90)hist.shift();
 if(hit>2){n++;console.log('---- HIT',hit.toFixed(1));console.log(hist.filter((_,i)=>i%6===0).join('\n'));hist.length=0;} }
