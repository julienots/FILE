// Caméras : poursuite éloignée, poursuite rapprochée, première personne (pare-chocs), cockpit, capot, cinématique.
import * as THREE from 'three';
import { clamp, lerp, damp, wrapAngle } from '../core/util.js';
import * as MAP from '../world/map.js';

export const CAM_MODES = [
  { id: 'far', name: 'Poursuite éloignée' },
  { id: 'near', name: 'Poursuite rapprochée' },
  { id: 'bumper', name: 'Première personne' },
  { id: 'cockpit', name: 'Cockpit' },
  { id: 'hood', name: 'Capot' },
  { id: 'cine', name: 'Cinématique' },
];
const V = new THREE.Vector3(), T = new THREE.Vector3(), Q = new THREE.Quaternion(), E = new THREE.Euler(0, 0, 0, 'YXZ');

export class CameraRig {
  constructor(camera) {
    this.cam = camera; this.mode = 'far'; this.pos = new THREE.Vector3(); this.look = new THREE.Vector3();
    this.heading = 0; this.shake = 0; this.blend = 0; this.fromPos = new THREE.Vector3(); this.fromQ = new THREE.Quaternion();
    this.cineT = 0; this.cinePos = new THREE.Vector3(); this.fov = 62; this.lookBack = false; this.init = false; this.baseFov = 62;
  }
  setMode(m) {
    if (m === this.mode) return;
    this.fromPos.copy(this.cam.position); this.fromQ.copy(this.cam.quaternion); this.blend = 1; this.mode = m; this.cineT = 0;
  }
  next() { const i = CAM_MODES.findIndex((m) => m.id === this.mode); this.setMode(CAM_MODES[(i + 1) % CAM_MODES.length].id); return this.mode; }
  snap() { this.init = false; this.blend = 0; }
  update(dt, car, view) {
    const api = view.userData.car, d = api.dims, cam = this.cam;
    const sp = car.speed, vr = clamp(sp / Math.max(30, car.vmax), 0, 1.2);
    let fovT = this.baseFov + 16 * vr * vr + (car.nitroOn ? 9 : 0);
    const s = Math.sin(car.yaw), c = Math.cos(car.yaw);
    if (car.impact > 2) this.shake = Math.max(this.shake, clamp(car.impact * 0.035, 0, 0.6));
    if (car.landed > 2) this.shake = Math.max(this.shake, clamp(car.landed * 0.05, 0, 0.5));
    this.shake = damp(this.shake, car.nitroOn ? 0.06 : 0, 5, dt);
    const interior = this.mode === 'cockpit' || this.mode === 'bumper' || this.mode === 'hood';
    if (interior) {
      const p = this.mode === 'cockpit' ? api.cockpit : this.mode === 'hood' ? api.hoodCam : V.set(0, Math.max(0.55, d.belt * 0.75), d.zf + 0.75).clone();
      const m = api.chassis.matrixWorld;
      api.chassis.updateWorldMatrix(true, false);
      T.copy(p).applyMatrix4(m);
      cam.position.copy(T);
      E.set(-car.pitch * 0.6 + (this.mode === 'cockpit' ? -0.02 : -0.03), car.yaw + Math.PI + (this.lookBack ? Math.PI : 0) + car.steer * (this.mode === 'cockpit' ? 0.35 : 0.12), -car.roll * 0.7);
      cam.quaternion.setFromEuler(E);
      fovT += this.mode === 'cockpit' ? 6 : 10;
      cam.near = 0.12;
    } else if (this.mode === 'cine') {
      this.cineT -= dt;
      const far = this.cinePos.distanceTo(view.position);
      if (this.cineT <= 0 || far > 140) {
        // nouveau point de vue en bord de trajectoire, devant la voiture
        const ahead = 35 + sp * 1.6, side = (Math.random() < 0.5 ? 1 : -1) * (9 + Math.random() * 10);
        const x = car.x + s * ahead + c * side, z = car.z + c * ahead - s * side;
        this.cinePos.set(x, MAP.groundHeight(x, z) + 1.6 + Math.random() * 7, z);
        this.cineT = 3.5 + Math.random() * 2.5; this.blend = 0;
      }
      cam.position.copy(this.cinePos);
      this.look.lerp(V.set(car.x, car.y + 0.8, car.z), 1 - Math.exp(-10 * dt));
      cam.lookAt(this.look);
      fovT = clamp(46 - far * 0.18, 14, 46);
      cam.near = 0.5;
    } else {
      const far = this.mode === 'far';
      const dist = (far ? 6.6 : 4.5) + d.L * 0.55 + vr * 1.6, hgt = (far ? 2.5 : 1.55) + d.H * 0.35;
      // la caméra suit un mélange cap / direction de déplacement : le dérapage se lit à l'écran
      let tgt = car.yaw;
      if (sp > 5 && car.vf > 0) tgt = car.yaw + clamp(wrapAngle(Math.atan2(car.vx, car.vz) - car.yaw), -0.6, 0.6) * 0.55;
      if (car.reverse && car.vf < -2) tgt = car.yaw + Math.PI;
      if (this.lookBack) tgt = car.yaw + Math.PI;
      if (!this.init) { this.heading = tgt; }
      this.heading += wrapAngle(tgt - this.heading) * (1 - Math.exp(-(this.lookBack ? 14 : 4.2 + vr * 3) * dt));
      const hs = Math.sin(this.heading), hc = Math.cos(this.heading);
      T.set(car.x - hs * dist, car.y + hgt + Math.max(0, -car.gPitch) * dist * 0.4, car.z - hc * dist);
      const gy = MAP.groundHeight(T.x, T.z) + 0.9;
      if (T.y < gy) T.y = gy;
      if (!this.init) { this.pos.copy(T); this.init = true; }
      this.pos.x = T.x; this.pos.z = T.z; this.pos.y = damp(this.pos.y, T.y, 8, dt);
      cam.position.copy(this.pos);
      this.look.set(car.x + hs * 3, car.y + d.H * 0.75 + 0.2, car.z + hc * 3);
      cam.lookAt(this.look);
      cam.near = 0.4;
    }
    if (this.shake > 0.003) { cam.position.x += (Math.random() - 0.5) * this.shake; cam.position.y += (Math.random() - 0.5) * this.shake; cam.rotateZ((Math.random() - 0.5) * this.shake * 0.05); }
    if (this.blend > 0) {
      this.blend = Math.max(0, this.blend - dt * 2.2);
      const k = this.blend * this.blend * (3 - 2 * this.blend);
      cam.position.lerp(this.fromPos, k);
      Q.copy(cam.quaternion); cam.quaternion.slerp(this.fromQ, k);
    }
    this.fov = damp(this.fov, fovT, 4, dt);
    if (Math.abs(cam.fov - this.fov) > 0.05) { cam.fov = this.fov; }
    cam.updateProjectionMatrix();
  }
}
