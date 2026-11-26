// Caméra troisième personne : orbite au doigt, suivi doux, évitement du relief, recul en vol rapide,
// tremblements, et mode photo (caméra libre).
import * as THREE from 'three';
import { G, clamp, damp, dampAngle, settings } from './core.js';

export class CameraRig {
  constructor(camera) {
    this.camera = camera;
    this.yaw = Math.PI * 0.15; this.pitch = 0.18;
    this.dist = 22; this.target = new THREE.Vector3();
    this.lastInput = 0;
    this.photo = false;
    this.photoPos = new THREE.Vector3();
    this.shakeOff = new THREE.Vector3();
  }
  rotate(dx, dy) {
    // sensibilité rapportée à la taille de l'écran (même ressenti sur tous les téléphones),
    // avec une légère accélération pour les gestes rapides
    const base = 5.6 / Math.max(320, Math.min(innerWidth, innerHeight * 2));
    const accel = 1 + Math.min(1.2, Math.hypot(dx, dy) / 35);
    const s = base * accel * settings.sensitivity;
    this.yaw -= dx * s;
    this.pitch = clamp(this.pitch + dy * s * 0.8 * (settings.invertY ? -1 : 1), -1.2, 1.35);
    this.lastInput = G.time;
  }
  zoom(f) { this.zoomMul = clamp((this.zoomMul || 1) * f, 0.45, 2.4); }

  update(dt) {
    const god = G.god, cam = this.camera, T = G.terrain;
    if (this.photo) {
      const r = (this.zoomMul || 1) * 20;
      const focus = god.pos.clone().add(new THREE.Vector3(0, 3, 0)).add(this.photoPos);
      cam.position.set(focus.x + Math.sin(this.yaw) * Math.cos(this.pitch) * r, focus.y + Math.sin(this.pitch) * r, focus.z + Math.cos(this.yaw) * Math.cos(this.pitch) * r);
      cam.position.y = Math.max(cam.position.y, T.groundAt(cam.position.x, cam.position.z) + 0.6);
      cam.lookAt(focus);
      return;
    }
    const flying = god.state === 'fly';
    const sp = god.vel.length();
    // suit doucement l'arrière du dieu quand il se déplace et que le joueur ne touche pas la caméra
    if (G.time - this.lastInput > 5 && sp > 8) this.yaw = dampAngle(this.yaw, god.heading + Math.PI, flying ? 0.5 : 0.3, dt);
    const hs = god.human ? 0.42 : 1; // forme humaine : caméra plus proche
    const want = (flying ? 26 + Math.min(sp, 80) * 0.22 : 19 + god.anim.run * 4) * (this.zoomMul || 1) * hs;
    this.dist = damp(this.dist, want, 2.5, dt);
    this.target.lerp(god.pos.clone().add(new THREE.Vector3(0, (flying ? 3.2 : 4.2) * (god.human ? 0.42 : 1), 0)), 1 - Math.exp(-(flying ? 8 : 12) * dt));
    const cp = Math.cos(this.pitch);
    const pos = new THREE.Vector3(this.target.x + Math.sin(this.yaw) * cp * this.dist, this.target.y + Math.sin(this.pitch) * this.dist, this.target.z + Math.cos(this.yaw) * cp * this.dist);
    // ne passe jamais sous le sol ni sous l'eau
    const floor = T.groundAt(pos.x, pos.z) + 1.4;
    if (pos.y < floor) pos.y = floor;
    cam.position.copy(pos);
    // tremblement
    const tr = G.fx ? G.fx.trauma : 0, k = tr * tr;
    if (k > 0) {
      const t = G.time * 40;
      this.shakeOff.set(Math.sin(t * 1.1) * k * 1.6, Math.sin(t * 1.7 + 1) * k * 1.2, Math.sin(t * 1.3 + 2) * k * 1.6);
      cam.position.add(this.shakeOff);
    }
    cam.lookAt(this.target);
    const fov = 58 + (flying ? Math.min(sp, 90) * 0.12 : 0);
    if (Math.abs(cam.fov - fov) > 0.05) { cam.fov = damp(cam.fov, fov, 3, dt); cam.updateProjectionMatrix(); }
  }

  // point visé au centre de l'écran (terrain, monstre)
  aim(maxDist = 900) {
    const cam = this.camera;
    const dir = new THREE.Vector3(); cam.getWorldDirection(dir);
    const origin = cam.position.clone().addScaledVector(dir, this.dist + 2);
    const hit = G.terrain.raycast(origin, dir, maxDist);
    const p = hit ? hit.point.clone() : origin.clone().addScaledVector(dir, Math.min(maxDist, 300));
    for (const e of G.enemies || []) {
      if (!e.alive) continue;
      const m = e.center();
      const toM = m.clone().sub(origin), along = toM.dot(dir);
      if (along > 0 && along < (hit ? hit.distance : maxDist)) {
        const closest = origin.clone().addScaledVector(dir, along);
        if (closest.distanceTo(m) < e.radius * 1.4) return { point: closest, monster: true, enemy: e, dir };
      }
    }
    return { point: p, monster: false, dir, ground: !!hit };
  }
}
