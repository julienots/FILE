import { CARS } from '../src/data/cars.js';
import { Car, collideStatic, resync } from '../src/vehicle/physics.js';
import { buildRoute } from '../src/ai/route.js';
import { AIDriver } from '../src/ai/driver.js';
const R = buildRoute([[-300,-300],[300,-300],[300,300],[-300,300]],{closed:true});
const spec = CARS.find(c=>c.id==='kestrel'); const car = new Car(spec, spec.stats, {assist:{style:'sport'}});
const st = R.at(0,0); car.place(st.x, st.z, st.yaw); const ai = new AIDriver(car, R, 'technical', 'hard', 3);
let t=0,n=0;
console.log('kmax',Math.max(...R.k).toFixed(3),'minv',Math.min(...ai.v).toFixed(1));
while(t<60){ ai.update(1/60,{bodies:[]}); for(let k=0;k<2;k++){car.step(1/120,{grip:1}); const h=collideStatic(car); if(h>0)resync(car); if(h>2&&n++<6)console.log('HIT t',t.toFixed(1),'v',car.speed.toFixed(1),'i',ai.i,'lat',R.lateral(car.x,car.z,ai.i).toFixed(1),'hw',R.p[ai.i].hw,'k',R.k[ai.i].toFixed(3));} t+=1/60;
 if(Math.abs(R.k[ai.i])>0.01 && (Math.round(t*60)%10===0)) console.log('t',t.toFixed(1),'i',ai.i,'v',car.speed.toFixed(1),'vt',ai.targetV.toFixed(1),'k',R.k[ai.i].toFixed(3),'lat',R.lateral(car.x,car.z,ai.i).toFixed(1),'beta',car.beta.toFixed(2),'steer',car.in.steer.toFixed(2),'slipF',car.slipF.toFixed(2),'slipR',car.slipR.toFixed(2),'vf',car.vf.toFixed(1),'rev',car.reverse,'yaw',car.yaw.toFixed(2),'yr',car.yawRate.toFixed(2),'th',car.in.throttle.toFixed(2),'br',car.in.brake.toFixed(2),'dsteer',car.steer.toFixed(2)); }
