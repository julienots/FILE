import { CARS } from '../src/data/cars.js';
import { Car } from '../src/vehicle/physics.js';
import * as MAP from '../src/world/map.js';
for (const id of ['falcor','aeon','kestrel']) for (const v0 of [20,40,60,90]) { let line=id+' v'+v0+' μg='+(CARS.find(c=>c.id===id).stats.grip*9.81).toFixed(1)+' :';
 for (const inp of [0.1,0.2,0.4,0.7,1]) {
  const spec = CARS.find(c=>c.id===id); const car = new Car(spec, spec.stats, {assist:{style:'sport'}});
  car.place(0,0,0,v0); let a=0,b=0;
  for(let i=0;i<120*3;i++){ car.in.steer=inp; car.in.throttle=0.5; car.step(1/120,{grip:1});
    // tapis roulant : on garde la voiture sur l'asphalte plat du centre-ville en la recentrant sans changer sa dynamique
    car.x=0; car.z=0; const sp=Math.hypot(car.vx,car.vz); if(sp>0){car.vx*=v0/sp;car.vz*=v0/sp;} }
  line+=` in${inp}:alat${(Math.abs(car.yawRate)*v0).toFixed(1)},β${car.beta.toFixed(2)},δ${car.steer.toFixed(3)}`; }
 console.log(line); }
