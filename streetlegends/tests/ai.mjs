import { CARS } from '../src/data/cars.js';
import { Car, collideStatic, resync } from '../src/vehicle/physics.js';
import { buildRoute } from '../src/ai/route.js';
import { AIDriver } from '../src/ai/driver.js';
import * as MAP from '../src/world/map.js';
function run(name, wps, opts, carId, persona, secs=200){
  const R = buildRoute(wps, opts); if(!R){console.log(name,'NO ROUTE');return;}
  const spec = CARS.find(c=>c.id===carId); const car = new Car(spec, spec.stats, {assist:{style:'ai'}});
  const st = R.at(0,0); car.place(st.x, st.z, st.yaw);
  const ai = new AIDriver(car, R, persona, 'hard', 3);
  let t=0, hits=0, maxOff=0, resp=0, laps=[], lastLap=0, vmax=0; const or=ai.respawn.bind(ai); ai.respawn=()=>{resp++;or();};
  while(t<secs){ ai.update(1/60,{bodies:[]}); for(let k=0;k<2;k++){car.step(1/120,{grip:1}); const h=collideStatic(car); if(h>0)resync(car); if(h>2)hits++;} t+=1/60;
    maxOff=Math.max(maxOff,Math.abs(R.lateral(car.x,car.z,ai.i))-R.p[ai.i].hw); vmax=Math.max(vmax,car.kmh);
    if(ai.lap>laps.length){laps.push((t-lastLap).toFixed(1));lastLap=t;}
    if(!R.closed && ai.i>=R.n-2){laps.push(t.toFixed(1));break;} }
  console.log(name, carId, persona, 'len',R.len.toFixed(0),'laps/time',laps.join(','),'hits',hits,'maxOffRoad',maxOff.toFixed(1),'respawns',resp,'vmax',vmax.toFixed(0),'prog',ai.prog.toFixed(0));
}
const track=(e)=>e.type==='track';
run('autodrome',[[2300,350],[2600,-200],[2650,300],[2600,850]],{closed:true,allow:track},'falcor','technical');
run('autodrome',[[2300,350],[2600,-200],[2650,300],[2600,850]],{closed:true,allow:track},'kitsune','drifter');
run('downtown loop',[[-300,-300],[300,-300],[300,300],[-300,300]],{closed:true},'kestrel','aggressive');
run('mountain',[[-1200,-940],[-800,-2150],[350,-2050],[1200,-940]],{allow:(e)=>e.type==='mountain'},'shiro','technical',400);
run('highway',[[-1200,-940],[1200,-940],[2060,-60],[1300,720],[-1200,730],[-2060,-60]],{closed:true,allow:(e)=>e.type==='highway'},'aeon','speedster',300);
run('oldtown',[[-1500,-100],[-900,-100],[-900,500],[-1500,500]],{closed:true},'pico','prudent');
run('dirt',[[-1050,-1350],[-150,-1450]],{allow:(e)=>e.type==='dirt'},'gravel','technical');
