import { CARS } from '../src/data/cars.js';
import { Car } from '../src/vehicle/physics.js';
for (const id of ['pico','kestrel','brawler','falcor','aeon','nomad','nyx']) {
  const spec = CARS.find(c=>c.id===id); const car = new Car(spec, spec.stats, {assist:{style:'sport'}});
  car.place(-2055, 0, 0); // périph ouest, direction +z
  // trouver cap de la route
  car.in.throttle=1; let t=0,t100=null,vm=0; 
  // ligne droite artificielle : on force la position pour rester sur l'asphalte
  for(let i=0;i<120*60;i++){ car.step(1/120,{grip:1}); car.x=-2055; car.z=0; car.yaw=0; car.vx=0; car.vz=Math.hypot(car.vx,car.vz)||car.vf; t+=1/120; if(!t100&&car.kmh>=100)t100=t; vm=Math.max(vm,car.kmh);}
  console.log(id, 'a100 spec',spec.stats.a100,'sim',t100?.toFixed(2),'top spec',spec.stats.top,'sim',vm.toFixed(0), 'gear',car.gear, 'surf', car.surf);
}
