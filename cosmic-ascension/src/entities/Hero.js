import * as THREE from 'three';
import { toonMaterial, addOutline } from '../render/shaders/Toon.js';
import { EVOLUTIONS } from '../data/evolutions.js';
import { Assets } from '../assets/AssetLibrary.js';

// Héros évolutif. Si un GLB `hero_<stageId>` existe il remplace le corps procédural (même API).
// FABLE: pousser les formes (ailes, halo, cornes, cape d'énergie, particules) par stade.
export class Hero {
  constructor() {
    this.root = new THREE.Group(); this.body = new THREE.Group(); this.root.add(this.body);
    this.stage = -1; this.t = 0; this.speed = 0; this.parts = {}; this.baseScale = 1; this.scaleMul = 1; this.lift = 0; this.spin = 0; this._sp = 0; this._air = 0;
    this.glow = new THREE.PointLight('#7fd0ff', 1.2, 14); this.root.add(this.glow); this.glow.position.y = .6;
    this.setStage(0);
  }
  setStage(i) {
    if (i === this.stage) return; this.stage = i; const st = EVOLUTIONS[i];
    while (this.body.children.length) this.body.remove(this.body.children[0]);
    this.parts = {};
    const model = Assets.get('hero_' + st.id);
    if (model) { this.body.add(model); }
    else this._buildProcedural(st, i);
    this.baseScale = st.scale; this.root.scale.setScalar(st.scale); this.glow.color.set(st.glow); this.baseGlow = 1 + i * .5; this.glow.intensity = this.baseGlow; this.glow.distance = 10 + st.scale * 6;
  }
  _buildProcedural(st, i) {
    const B = this.body, P = this.parts, C = (c) => new THREE.Color(c);
    const mat = toonMaterial({ color: st.color, rim: st.glow, rimStrength: .85 }), light = toonMaterial({ color: C(st.color).lerp(C('#ffffff'), .55), rim: st.glow });
    const glow = new THREE.MeshBasicMaterial({ color: st.glow }), soft = new THREE.MeshBasicMaterial({ color: C(st.glow).lerp(C('#ffffff'), .45), transparent: true, opacity: .85 });
    const white = new THREE.MeshBasicMaterial({ color: '#ffffff' }), black = new THREE.MeshBasicMaterial({ color: '#141226' });
    const add = (geo, m, x, y, z, sx = 1, sy = sx, sz = sx, parent = B) => { const o = new THREE.Mesh(geo, m); o.position.set(x, y, z); o.scale.set(sx, sy, sz); parent.add(o); return o; };
    const S = (r, w = 16, h = 12) => new THREE.SphereGeometry(r, w, h);
    // corps, ventre, visage
    const torso = add(S(.5, 22, 16), mat, 0, .55, 0, 1, 1.08, .95); addOutline(torso, '#1a0f33', .035);
    add(S(.36), light, 0, .47, .2, 1, 1, .8);
    for (const s of [-1, 1]) {
      const eye = add(S(.14, 12, 10), white, .19 * s, .72, .37); add(S(.085, 10, 8), black, .01 * s, 0, .075, 1, 1.15, 1, eye); add(S(.03, 6, 6), white, .03, .04, .15, 1, 1, 1, eye);
      add(S(.07, 8, 6), new THREE.MeshBasicMaterial({ color: '#ff9ab8', transparent: true, opacity: .7 }), .33 * s, .56, .34, 1, .6, .5);
      P['foot' + s] = add(S(.15, 10, 8), mat, .2 * s, .1, .08, 1, .6, 1.35);
      if (i >= 1) { const arm = add(new THREE.CapsuleGeometry(.075 + i * .01, .26, 4, 8), mat, .52 * s, .52, 0); arm.rotation.z = -.6 * s; add(S(.1, 8, 6), light, 0, -.2, 0, 1, 1, 1, arm); P['arm' + s] = arm; }
    }
    add(new THREE.TorusGeometry(.07, .018, 6, 12, Math.PI), black, 0, .56, .47).rotation.z = Math.PI;            // sourire
    if (i === 0) { const stem = add(new THREE.CylinderGeometry(.02, .03, .4, 5), mat, 0, 1.25, 0); stem.rotation.z = .2; P.core = add(new THREE.OctahedronGeometry(.11), glow, -.05, 1.5, 0); }
    if (i === 1) { for (const s of [-1, 0, 1]) { const leaf = add(new THREE.ConeGeometry(.12, .5, 5), toonMaterial({ color: '#4fe08a', rim: '#d0ffd8' }), .14 * s, 1.25 - Math.abs(s) * .06, -.05, 1, 1, .45); leaf.rotation.z = -.5 * s; } P.core = add(new THREE.OctahedronGeometry(.1), glow, 0, .58, .47); }
    if (i >= 2) { const tail = new THREE.Group(); tail.position.set(0, .5, -.45); B.add(tail); P.tail = tail;
      for (let k = 0; k < 5; k++) add(S(.24 - k * .04, 10, 8), k ? soft : glow, 0, k * .07, -k * .26, 1, 1, 1.25, tail);
      for (const s of [-1, 1]) { const horn = add(new THREE.ConeGeometry(.07, .32 + i * .04, 6), glow, .24 * s, 1.14, .02); horn.rotation.z = -.35 * s; } }
    if (i >= 3) for (const s of [-1, 1]) { const wing = new THREE.Group(); wing.position.set(.34 * s, .78, -.26); B.add(wing); P['wing' + s] = wing;       // ailes: éventail de plumes de lumière
      const nF = 4 + Math.min(2, i - 3); for (let k = 0; k < nF; k++) { const len = (1.15 - k * .14) * (1 + (i - 3) * .12), f = add(new THREE.OctahedronGeometry(.5), k % 2 ? soft : glow, 0, 0, 0, .13, len, .3, wing); const a = (.55 + k * .3) * s; f.rotation.z = -a; f.position.set(Math.sin(a) * len * .5, Math.cos(a) * len * .5, -k * .03); } }
    if (i >= 3) { P.halo = add(new THREE.TorusGeometry(.4, .035, 8, 32), glow, 0, 1.42, 0); P.halo.rotation.x = Math.PI / 2; }
    if (i >= 4) { P.shards = []; for (const s of [-1, 1]) for (let k = 0; k < 2; k++) { const sh = add(new THREE.OctahedronGeometry(.16), glow, (.78 + k * .22) * s, .95 + k * .2, -.05, 1, 1.9, 1); sh.rotation.z = -.4 * s; P.shards.push(sh); }
      const ring = add(new THREE.TorusGeometry(.56, .025, 6, 36), soft, 0, .5, 0); ring.rotation.x = 1.2; P.belt = ring;
      for (let k = 0; k < 5; k++) { const a = (k - 2) * .34, sp = add(new THREE.ConeGeometry(.06, .34 - Math.abs(k - 2) * .05, 5), glow, Math.sin(a) * .34, 1.2 + Math.cos(a) * .12, .12); sp.rotation.z = -a; } }
    if (i >= 5) { P.orbit = new THREE.Group(); P.orbit.position.y = .6; B.add(P.orbit); const cols = ['#6ff2ff', '#ff8ad8', '#8affb0'];
      for (let k = 0; k < 3; k++) { const a = k * 2.094, pl = add(S(.1 + k * .02, 10, 8), toonMaterial({ color: cols[k], rim: '#ffffff', emissive: cols[k], emissiveIntensity: .4 }), Math.cos(a) * 1.05, Math.sin(k * 1.7) * .25, Math.sin(a) * 1.05, 1, 1, 1, P.orbit); if (k === 1) { const r = add(new THREE.TorusGeometry(.2, .015, 4, 20), soft, 0, 0, 0, 1, 1, 1, pl); r.rotation.x = 1.3; } }
      const r2 = add(new THREE.TorusGeometry(.75, .02, 6, 40), glow, 0, .6, 0); r2.rotation.x = Math.PI / 2 - .5; P.ring2 = r2;
      P.core = add(new THREE.IcosahedronGeometry(.16, 1), white, 0, .6, .44); }
  }
  // speed01: 0 immobile → 1 course. airborne: bool. Squash & stretch, inclinaison, pas, respiration; lift/spin/scaleMul pilotés par les cinématiques.
  animate(dt, speed01, airborne, vUp = 0) {
    this.t += dt; const t = this.t, b = this.body, k = Math.min(1, dt * 10);
    this._sp += (speed01 - this._sp) * k; const sp = this._sp;
    if (this._air && !airborne) this._land = 1;                       // écrasement à l'atterrissage
    this._air = airborne ? 1 : 0; this._land = Math.max(0, (this._land || 0) - dt * 5);
    const stride = t * (4 + sp * 10), bob = Math.abs(Math.sin(stride)) * sp * .12 + Math.sin(t * 2.2) * .02;
    const stretch = airborne ? THREE.MathUtils.clamp(vUp * .03, -.18, .25) : -this._land * .3 + bob * .5;
    b.position.y = (airborne ? .1 : bob) + this.lift / this.baseScale;
    b.scale.set(1 - stretch * .6, 1 + stretch, 1 - stretch * .6);
    b.rotation.set(sp * .28, this.spin, Math.sin(stride) * .07 * sp);
    for (const s of [-1, 1]) {
      const ph = stride + (s > 0 ? Math.PI : 0);
      const f = this.parts['foot' + s]; if (f) { f.position.z = .08 + Math.sin(ph) * .2 * sp; f.position.y = .1 + Math.max(0, Math.cos(ph)) * .14 * sp + (airborne ? -.05 : 0); }
      const a = this.parts['arm' + s]; if (a) { a.rotation.x = -Math.sin(ph) * .9 * sp; a.rotation.z = (-.6 - (airborne ? .9 : 0)) * s; }
      const w = this.parts['wing' + s]; if (w) { w.rotation.z = (Math.sin(t * (airborne ? 13 : 3.5)) * (airborne ? .45 : .16) - .1) * s; w.rotation.y = -.35 * s - sp * .3 * s; }
    }
    if (this.parts.halo) this.parts.halo.rotation.z = t * 2;
    if (this.parts.core) { this.parts.core.scale.setScalar(1 + Math.sin(t * 6) * .2); this.parts.core.rotation.y = t * 2; }
    if (this.parts.orbit) this.parts.orbit.rotation.y = t * 1.3; if (this.parts.ring2) this.parts.ring2.rotation.z = t * .8; if (this.parts.belt) this.parts.belt.rotation.z = -t * 1.1;
    if (this.parts.shards) for (let k = 0; k < this.parts.shards.length; k++) this.parts.shards[k].position.y = .95 + (k % 2) * .2 + Math.sin(t * 2.5 + k) * .06;
    if (this.parts.tail) { this.parts.tail.scale.z = 1 + sp * .7 + Math.sin(t * 12) * .08; this.parts.tail.rotation.y = Math.sin(t * 3) * .25; this.parts.tail.rotation.x = -.25 + Math.sin(t * 2.2) * .12; }
    this.root.scale.setScalar(this.baseScale * this.scaleMul);
    if (!this.lift) this.glow.intensity += (this.baseGlow - this.glow.intensity) * k;
  }
}
