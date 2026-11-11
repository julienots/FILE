import { CARS } from '../src/data/cars.js';
import { Car } from '../src/vehicle/physics.js';
for (const [id,style,th] of [['shiro','sport',0],['shiro','sport',1],['kestrel','sport',0],['kestrel','arcade',0.5],['brawler','sport',1],['pico','sport',0.5]]) {
  const spec = CARS.find(c=>c.id===id); const car = new Car(spec, spec.stats, {assist:{style}});
  car.place(2600,-195, Math.PI/2, 33); // autodrome: grande zone plate ? on reste sur l'asphalte quelques secondes
  let out=[];
  for(let i=0;i<120*4;i++){ const t=i/120; car.in.throttle=th; car.in.steer = t<0.4?0.5:0; car.step(1/120,{grip:1}); if(i%60===0) out.push(`${t.toFixed(1)}:b${car.beta.toFixed(2)},r${car.yawRate.toFixed(2)},v${car.speed.toFixed(0)},${car.surf[0]}`); }
  console.log(id,style,th,out.join(' '));
}
