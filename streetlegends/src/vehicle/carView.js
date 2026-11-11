// Lie un véhicule physique à son modèle 3D : attitude, roues, direction, feux, fumée, traces, déformations.
import { buildCar } from './carBuilder.js';
import { SURF } from './physics.js';

export function makeView(spec, cfg, opts) {
  return buildCar(spec, cfg, opts);
}
export function syncView(car, view, dt, fx, lightsOn) {
  const api = view.userData.car;
  view.position.set(car.x, car.y, car.z);
  view.rotation.y = car.yaw;
  const ch = api.chassis;
  if (ch) { ch.rotation.x = car.pitch; ch.rotation.z = car.roll; ch.position.y = car.heave + (car.airborne ? 0.03 : 0); }
  const W = api.wheels;
  for (let i = 0; i < W.length; i++) {
    const w = W[i];
    // 0 AVG(+x) 1 AVD 2 ARG 3 ARD → correspond à car.wy
    const gy = car.airborne ? car.y - 0.09 : car.wy[i];
    w.g.position.y = w.baseY + Math.max(-0.14, Math.min(0.14, gy - car.y));
    w.spin.rotation.x = w.front ? car.wheelRot : car.rearRot;
    if (w.front) w.g.rotation.y = car.steer;
  }
  if (api.steer && api.steer.children[0]) api.steer.children[0].rotation.z = -car.steer * 5.5;
  api.setLights(lightsOn, car.brakeOut > 0.1 && !car.reverse, car.reverse);
  if (api.blob) api.blob.material.opacity = 0.5;
  if (!fx) return;
  // fumée / poussière / traces aux roues arrière
  const s = Math.sin(car.yaw), c = Math.cos(car.yaw), SF = SURF[car.surf] || SURF.asphalt;
  const sk = car.skid || 0;
  for (let i = 2; i < 4; i++) {
    const sx = i === 2 ? car.trk : -car.trk;
    const wx = car.x + s * car.zr + c * sx, wz = car.z + c * car.zr - s * sx, wy = car.wy[i];
    const key = car.id * 4 + i;
    if (sk > 0.25 && !car.airborne) {
      if (SF.loose) { if (fx.rand() < 0.5) fx.particles.dust(wx, wy, wz, car.vx, car.vz, car.surf === 'sand'); }
      else {
        if (fx.rand() < sk * 0.7) fx.particles.smoke(wx, wy, wz, car.vx, car.vz, sk);
        fx.skids.add(key, wx, wy, wz, c, -s, 0.13, sk);
      }
      if (SF.loose) fx.skids.add(key, wx, wy, wz, c, -s, 0.13, sk * 0.5, 0.25);
    } else {
      fx.skids.stop(key);
      if (SF.dust > 0 && car.speed > 9 && !car.airborne && fx.rand() < SF.dust * 0.3) fx.particles.dust(wx, wy, wz, car.vx, car.vz, car.surf === 'sand');
      else if (fx.wet > 0.4 && car.speed > 14 && !car.airborne && !SF.loose && fx.rand() < 0.35) fx.particles.spray(wx, wy, wz, car.vx, car.vz);
    }
  }
  if (car.nitroOn && api.exhausts) for (const e of api.exhausts) {
    const ex = car.x + s * e.z + c * e.x, ez = car.z + c * e.z - s * e.x;
    fx.particles.flame(ex, car.y + e.y, ez, car.vx - s * 6, car.vz - c * 6);
  }
  if (car.landed > 3.5 && SF.dust === 0) fx.particles.spark(car.x, car.y, car.z, car.vx, car.vz);
}
