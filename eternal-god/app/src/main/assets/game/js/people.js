// Habitants : personnages animés (modèles Quaternius CC0) avec métiers, familles, routines horaires,
// déplacements par les rues, et réactions au dieu (s'agenouiller, prier, fuir, se cacher, attaquer,
// fêter). Ils peuvent mourir, reconstruire leurs maisons et avoir des enfants.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { MeshoptDecoder } from 'three/addons/libs/meshopt_decoder.module.js';
import * as SkeletonUtils from 'three/addons/utils/SkeletonUtils.js';
import { G, Q, WORLD, mulberry32, clamp, lerp, rand, pick, emit, dampAngle, angleDelta } from './core.js';

const BASES = { man: 'adventurer', woman: 'hooded_adventurer', chief: 'monarch' };
// tissus teintables par modèle (le reste : peau, yeux, cheveux, métal garde sa couleur)
const CLOTH = {
  adventurer: ['Green', 'LightGreen', 'Brown', 'Brown2', 'Grey'],
  hooded_adventurer: ['Brown2', 'LightBrown', 'DarkBrown', 'White', 'Brown'],
  monarch: ['Blue', 'Beige'],
};
const FABRICS = [0x6b4f2e, 0x8c6d46, 0x4d5a3a, 0x7a2e24, 0x3f4a5c, 0x9a8a6a, 0x5c3f5a, 0xb7a483, 0x2f3b2a, 0x8a5a2b, 0x6e6a5e, 0x3a2f28, 0x845c3c, 0x58633f];
const HAIRS = [0x2a1d14, 0x3b2a1b, 0x6a4a2a, 0x8b6b3e, 0xb89a62, 0x1a1512, 0x9a9590];
const MEN = ['Aubin', 'Bastien', 'Colin', 'Denis', 'Étienne', 'Firmin', 'Gaspard', 'Hugues', 'Jehan', 'Lucien', 'Mathis', 'Nicolas', 'Odon', 'Perrin', 'Renaud', 'Simon', 'Thibault', 'Urbain', 'Victor', 'Gautier', 'Anselme', 'Bertrand'];
const WOMEN = ['Agnès', 'Blanche', 'Clémence', 'Denise', 'Éloïse', 'Flore', 'Gisèle', 'Hélène', 'Isabeau', 'Jeanne', 'Margot', 'Ninon', 'Perrine', 'Rose', 'Sibylle', 'Typhaine', 'Ysolde', 'Aliénor', 'Bertille', 'Mahaut'];
const ROLE_FR = { farmer: 'fermier', smith: 'forgeron', merchant: 'marchand', guard: 'garde', priest: 'prêtre', innkeeper: 'aubergiste', fisher: 'pêcheur', woodcutter: 'bûcheron', shepherd: 'berger', crafter: 'artisan', child: 'enfant', chief: 'chef du village', miller: 'meunier', traveler: 'voyageur' };

const SAY = {
  kneel: ['Gloire à toi, Seigneur !', 'Le Dieu est parmi nous…', 'Bénis-nous, Éternel !', 'Pardonne-nous nos fautes.', 'Il est descendu du ciel !'],
  flee: ['Fuyez !', 'Au secours !', 'Il va tous nous tuer !', 'Mes enfants ! Où sont mes enfants ?', 'La colère divine !'],
  hide: ['Vite, à la maison !', 'Cachez-vous !'],
  attack: ['Pour le village !', 'Arrière, démon !', 'Nous ne plierons pas !'],
  celebrate: ['Merci, Seigneur !', 'Un miracle !', 'Regardez, il nous protège !'],
  wonder: ['Qu\'est-ce que c\'est… ?', 'Par tous les saints…', 'Un dieu… ici ?'],
  mourn: ['Non… pas lui…', 'Pourquoi ?', 'Il nous a abandonnés…'],
  rebuild: ['On rebâtira, pierre après pierre.', 'Au travail, tous !'],
};

export async function loadPeople(onEach) {
  const gl = new GLTFLoader(); gl.setMeshoptDecoder(MeshoptDecoder);
  const out = {};
  for (const k of Object.values(BASES)) {
    const g = await gl.loadAsync(`models/chars/${k}.glb`);
    g.scene.updateMatrixWorld(true);
    const bb = new THREE.Box3().setFromObject(g.scene);
    out[k] = { gltf: g, height: bb.max.y - bb.min.y, clips: Object.fromEntries(g.animations.map((c) => [c.name.split('|').pop(), c])) };
    if (onEach) onEach();
  }
  return out;
}

const _v = new THREE.Vector3(), _q = new THREE.Quaternion(), _e = new THREE.Euler();

class Villager {
  constructor(sys, base, opts) {
    this.sys = sys;
    Object.assign(this, opts);
    const A = sys.assets[base];
    this.base = base;
    this.model = SkeletonUtils.clone(A.gltf.scene);
    const rnd = opts.rnd;
    const cloth = new Set(CLOTH[base]);
    const tint = new Map();
    this.model.traverse((o) => {
      if (!o.isMesh) return;
      o.castShadow = true; o.frustumCulled = false;
      if (o.name === 'Backpack' && rnd() < 0.85) o.visible = false;
      const mats = Array.isArray(o.material) ? o.material : [o.material];
      const nm = mats.map((m) => {
        if (cloth.has(m.name)) { if (!tint.has(m.name)) { const c = m.clone(); c.color = new THREE.Color(pick(FABRICS)).multiplyScalar(0.85 + rnd() * 0.3); tint.set(m.name, c); } return tint.get(m.name); }
        if (/^Hair/.test(m.name)) { if (!tint.has(m.name)) { const c = m.clone(); c.color = new THREE.Color(opts.old ? 0xc8c4bc : pick(HAIRS)); tint.set(m.name, c); } return tint.get(m.name); }
        return m;
      });
      o.material = Array.isArray(o.material) ? nm : nm[0];
      for (const m of nm) { m.roughness = Math.max(m.roughness ?? 0.8, 0.75); m.metalness = m.name && /Metal|Gold/.test(m.name) ? m.metalness : 0; }
    });
    { const sw = this.model.getObjectByName('Sword'); if (sw && opts.role !== 'guard') sw.visible = false; }
    this.height = opts.height;
    this.model.scale.setScalar(this.height / A.height);
    this.mixer = new THREE.AnimationMixer(this.model);
    this.actions = {};
    for (const [k, c] of Object.entries(A.clips)) this.actions[k] = this.mixer.clipAction(c);
    for (const k of ['Death', 'HitRecieve', 'Roll', 'Sword_Slash', 'Wave', 'Interact']) if (this.actions[k] && (k === 'Death')) { this.actions[k].setLoop(THREE.LoopOnce); this.actions[k].clampWhenFinished = true; }
    this.bones = {};
    this.model.traverse((o) => { if (o.isBone) this.bones[o.name] = o; });
    // position de repos des pieds par rapport aux tibias (les pieds du squelette sont des contrôleurs indépendants)
    this.model.updateMatrixWorld(true);
    this.footOff = {};
    for (const s of ['L', 'R']) { const ll = this.bones['LowerLeg' + s], f = this.bones['Foot' + s]; if (ll && f) this.footOff[s] = ll.worldToLocal(f.getWorldPosition(new THREE.Vector3())); }
    this.pos = new THREE.Vector3(opts.home.door.x, 0, opts.home.door.z);
    this.pos.y = G.terrain.heightAt(this.pos.x, this.pos.z);
    this.heading = rnd() * 6.28;
    this.state = 'idle'; this.stateT = 0; this.path = []; this.anim = null;
    this.poseW = 0; this.poseKind = null;
    this.vel = new THREE.Vector3();
    this.thinkT = rnd() * 0.3; this.sayT = 0;
    this.faith = rnd(); this.bravery = rnd();
    this.alive = true;
    this.play('Idle');
    this.mixer.update(rnd() * 2);
    sys.scene.add(this.model);
  }

  get village() { return this.sys.villages.villages[this.vid]; }

  play(name, fade = 0.25, speed = 1) {
    const a = this.actions[name] || this.actions.Idle;
    if (this.anim === a) { a.timeScale = speed; return; }
    a.reset(); a.timeScale = speed; a.enabled = true; a.setEffectiveWeight(1); a.play();
    if (this.anim) a.crossFadeFrom(this.anim, fade, false);
    this.anim = a;
  }

  say(kind, force = false) {
    if (!force && (this.sayT > 0 || Math.random() > 0.35)) return;
    this.sayT = rand(6, 12);
    const text = pick(SAY[kind]);
    this.sys.speech.push({ v: this, text, t: 3.5 });
  }

  // itinéraire par les rues : rue de départ → place → rue d'arrivée
  goTo(target, run = false, onArrive = null) {
    const v = this.village;
    this.path = [];
    const near = (p) => {
      let best = null, bd = 1e9;
      v.streetsPts.forEach((st, si) => st.forEach((q, qi) => { const d = Math.hypot(q.x - p.x, q.z - p.z); if (d < bd) { bd = d; best = [si, qi]; } }));
      return [best, bd];
    };
    const [a, da] = near(this.pos), [b, db] = near(target);
    const direct = Math.hypot(target.x - this.pos.x, target.z - this.pos.z) < 25 || !a || !b;
    if (!direct) {
      const [sa, ia] = a, [sb, ib] = b;
      if (sa === sb) {
        const step = ib >= ia ? 1 : -1;
        for (let i = ia; i !== ib + step; i += step) this.path.push(v.streetsPts[sa][i].clone());
      } else {
        for (let i = ia; i >= 0; i--) this.path.push(v.streetsPts[sa][i].clone());
        this.path.push(v.spots.plaza.clone().add(new THREE.Vector3(rand(-4, 4), 0, rand(-4, 4))));
        for (let i = 0; i <= ib; i++) this.path.push(v.streetsPts[sb][i].clone());
      }
      for (const p of this.path) { p.x += rand(-1.2, 1.2); p.z += rand(-1.2, 1.2); }
    }
    this.path.push(target.clone());
    this.running = run;
    this.onArrive = onArrive;
    this.setState('walk');
  }

  setState(s) { this.state = s; this.stateT = 0; }

  face(x, z, dt, k = 6) { this.heading = dampAngle(this.heading, Math.atan2(x - this.pos.x, z - this.pos.z), k, dt); }

  // ---------- routine selon l'heure et le métier ----------
  routine() {
    const h = G.env ? G.env.hour() : 12, v = this.village, R = this.role;
    const night = h < 5.8 || h > 21.5;
    if (night || (this.child && h > 20)) return { kind: 'home' };
    if (h < 7) return { kind: 'idleAt', at: this.home.door };
    if (h > 12 && h < 13.2 && !this.child) return v.tavern && v.tavern.state === 'intact' && this.vid === 0 ? { kind: 'idleAt', at: v.tavern.door, talk: true } : { kind: 'idleAt', at: this.home.door };
    if (h > 18.5) return { kind: 'idleAt', at: v.spots.plaza, talk: true, spread: 9 };
    if (this.child) return { kind: 'play' };
    if (this.role === 'traveler') return { kind: 'travel' };
    if (this.workAt) return { kind: 'work', at: this.workAt, anim: this.workAnim };
    return { kind: 'idleAt', at: v.spots.plaza, talk: true, spread: 12 };
  }

  think(dt) {
    const sys = this.sys, god = G.god;
    const v = this.village;
    // ---- danger : événements de destruction récents
    let danger = 0, dSrc = null;
    for (const e of sys.events) {
      const d = Math.hypot(e.x - this.pos.x, e.z - this.pos.z);
      const reach = e.r + 25 + e.power * 0.1;
      if (d < reach && e.kind !== 'bless') { const w = (1 - d / reach) * e.power * (1 - e.age / 10); if (w > danger) { danger = w; dSrc = e; } }
    }
    const godNear = god && god.pos.distanceTo(this.pos) < 42 && god.pos.y - G.terrain.groundAt(god.pos.x, god.pos.z) < 18;
    const devotion = v.worship - v.fear * 1.3 + (this.faith - 0.5) * 0.35;
    if (danger > 14 && !['flee', 'hide', 'attack', 'hidden'].includes(this.state)) {
      if (this.role === 'guard' && this.bravery > 0.4 && devotion < 0.1 && god) {
        this.setState('attack'); this.say('attack', true);
      } else if (this.home.state === 'intact' && this.home.door.distanceTo(this.pos) < 40 && Math.random() < 0.5) {
        this.goTo(this.home.door, true, () => { this.setState('hidden'); this.model.visible = false; });
        this.state = 'hide'; this.say('hide');
      } else {
        const away = new THREE.Vector3(this.pos.x - dSrc.x, 0, this.pos.z - dSrc.z).normalize().multiplyScalar(rand(45, 80)).add(this.pos);
        away.x = clamp(away.x, -WORLD.half + 30, WORLD.half - 30); away.z = clamp(away.z, -WORLD.half + 30, WORLD.half - 30);
        this.path = [away]; this.running = true; this.onArrive = () => this.setState('cower'); this.state = 'flee'; this.stateT = 0;
        this.say('flee');
      }
      return;
    }
    if (['flee', 'hide', 'attack', 'flung', 'dead', 'hidden', 'cower'].includes(this.state)) {
      // retour au calme
      if (this.state === 'hidden' && danger < 2 && this.stateT > 20) { this.model.visible = true; this.setState('idle'); }
      if (this.state === 'cower' && danger < 2 && this.stateT > 12) this.setState('idle');
      if (this.state === 'attack' && (!godNear || devotion > 0.2) && this.stateT > 6) this.setState('idle');
      return;
    }
    if (godNear) {
      const already = ['kneel', 'pray', 'celebrate', 'wonder'].includes(this.state);
      if (!already) {
        if (devotion > 0.05) { this.setState(this.child ? 'celebrate' : 'kneel'); this.say(this.child ? 'celebrate' : 'kneel'); }
        else { this.setState('wonder'); this.say('wonder'); }
      }
      return;
    }
    if (['kneel', 'pray', 'celebrate', 'wonder'].includes(this.state) && this.stateT > 3) this.setState('idle');
    // chantier de reconstruction
    if (this.buildTarget) {
      if (this.buildTarget.state !== 'ruin') { this.buildTarget = null; this.setState('idle'); }
      else if (this.state !== 'build' && this.state !== 'walk') this.goTo(this.buildTarget.door, false, () => this.setState('build'));
      return;
    }
    // routine
    if (this.state === 'walk') return;
    const r = this.routine();
    const key = r.kind + (r.at ? r.at.x.toFixed(0) : '');
    if (key !== this.planKey || (this.state === 'idle' && this.stateT > 4)) {
      this.planKey = key;
      if (r.kind === 'travel') {
        const rt = this.sys.roads && this.sys.roads.randomRoute();
        if (rt) {
          const fwd = this.pos.distanceTo(rt.pts[0]) < this.pos.distanceTo(rt.pts[rt.pts.length - 1]);
          const pts = (fwd ? rt.pts : [...rt.pts].reverse()).map((p) => p.clone().add(new THREE.Vector3(rand(-1, 1), 0, rand(-1, 1))));
          this.path = [pts[0], ...pts.filter((_, i) => i % 3 === 0), pts[pts.length - 1]];
          this.running = false; this.onArrive = () => { this.setState('talk'); this.planKey = null; }; this.state = 'walk'; this.stateT = -600;
        }
      } else if (r.kind === 'home') {
        if (this.home.state === 'intact') this.goTo(this.home.door, false, () => { this.setState('hidden'); this.model.visible = false; });
        else this.goTo(v.spots.plaza, false, () => this.setState('sleepOut'));
      } else if (r.kind === 'play') {
        const a = Math.random() * 6.28, d = rand(3, 14);
        this.goTo(v.spots.plaza.clone().add(new THREE.Vector3(Math.cos(a) * d, 0, Math.sin(a) * d)), Math.random() < 0.7, () => this.setState('idle'));
      } else {
        const at = r.at.clone();
        if (r.spread) { const a = Math.random() * 6.28, d = rand(2, r.spread); at.x += Math.cos(a) * d; at.z += Math.sin(a) * d; }
        else { at.x += rand(-1.5, 1.5); at.z += rand(-1.5, 1.5); }
        this.goTo(at, false, () => this.setState(r.kind === 'work' ? 'work' : (r.talk ? 'talk' : 'idle')));
      }
    }
  }

  update(dt, near) {
    this.stateT += dt; this.sayT -= dt;
    this.thinkT -= dt;
    if (this.thinkT <= 0 && this.alive) { this.thinkT = 0.3 + Math.random() * 0.2; this.think(dt); }
    const T = G.terrain, god = G.god;
    let pose = null;
    switch (this.state) {
      case 'walk': case 'flee': case 'hide': {
        const tgt = this.path[0];
        if (!tgt) { const cb = this.onArrive; this.onArrive = null; if (this.state === 'walk') this.setState('idle'); if (cb) cb(); break; }
        const dx = tgt.x - this.pos.x, dz = tgt.z - this.pos.z, d = Math.hypot(dx, dz);
        const sp = this.running || this.state !== 'walk' ? (this.child ? 4.2 : 4.6) : (this.child ? 1.5 : 1.35);
        if (d < 0.8) { this.path.shift(); break; }
        this.face(tgt.x, tgt.z, dt, 8);
        this.pos.x += Math.sin(this.heading) * sp * dt; this.pos.z += Math.cos(this.heading) * sp * dt;
        if (this.sys.villages.pushOut(this.pos, 0.45)) this.heading += 0.3;
        this.play(sp > 3 ? 'Run' : 'Walk', 0.25, sp > 3 ? 1 : 1.05);
        if (this.state === 'walk' && this.stateT > 120) { this.path = []; }
        break;
      }
      case 'work': this.play(this.workAnim || 'Interact', 0.3, this.workAnim === 'Sword_Slash' ? 0.8 : 1); if (this.workFace) this.face(this.workFace.x, this.workFace.z, dt); break;
      case 'build': this.play('Interact'); this.face(this.buildTarget.x, this.buildTarget.z, dt); this.sys.villages.work(this.buildTarget, dt * 0.006); if (this.stateT < 0.1) this.say('rebuild'); break;
      case 'talk': {
        this.play(this.stateT % 9 < 1.6 && Math.random() < 0.01 ? 'Wave' : 'Idle_Neutral');
        const o = this.talkTo; if (o) this.face(o.pos.x, o.pos.z, dt, 3);
        if (!o || this.stateT > 30) { this.talkTo = this.sys.nearest(this, 6); if (this.stateT > 30) this.setState('idle'); }
        break;
      }
      case 'kneel': case 'pray': {
        if (god) this.face(god.pos.x, god.pos.z, dt, 4);
        this.play('Idle_Neutral');
        pose = this.stateT > 1.2 || this.state === 'pray' ? 'pray' : 'kneel';
        break;
      }
      case 'celebrate': this.play('Wave'); if (god) this.face(god.pos.x, god.pos.z, dt, 4); break;
      case 'wonder': case 'cower': {
        this.play(this.state === 'cower' ? 'Idle' : 'Idle_Neutral');
        if (god) this.face(god.pos.x, god.pos.z, dt, 3);
        if (this.state === 'cower') pose = 'cower';
        break;
      }
      case 'attack': {
        if (!god) { this.setState('idle'); break; }
        const d = Math.hypot(god.pos.x - this.pos.x, god.pos.z - this.pos.z);
        this.face(god.pos.x, god.pos.z, dt, 8);
        if (d > 3.5) { this.pos.x += Math.sin(this.heading) * 4.4 * dt; this.pos.z += Math.cos(this.heading) * 4.4 * dt; this.play('Run'); }
        else { this.play('Sword_Slash'); if (G.fx && Math.random() < dt * 2) G.fx.sparks(god.pos.clone().add(new THREE.Vector3(0, 2, 0)), 6); }
        this.sys.villages.pushOut(this.pos, 0.45);
        break;
      }
      case 'flung': {
        this.vel.y -= 25 * dt;
        this.pos.addScaledVector(this.vel, dt);
        this.model.rotation.x += dt * 6;
        const gy = T.heightAt(this.pos.x, this.pos.z);
        if (this.pos.y <= gy && this.vel.y < 0) { this.pos.y = gy; this.model.rotation.x = 0; this.die(); }
        break;
      }
      case 'dead': {
        if (this.stateT > 7) { const s = 1 - (this.stateT - 7) / 2; if (s <= 0) { this.remove(); return; } this.model.position.y -= dt * 0.3; }
        break;
      }
      case 'sleepOut': this.play('Idle'); pose = 'cower'; break;
      case 'hidden': break;
      default: this.play(this.state === 'idle' ? 'Idle' : 'Idle');
    }
    if (this.state !== 'flung') this.pos.y = Math.max(T.heightAt(this.pos.x, this.pos.z), this.village.port && this.onDock ? WORLD.water + 1.1 : -1e9);
    // mise à jour visuelle (plus rare au loin)
    if (this.state === 'hidden' || this.state === 'dead' && this.stateT > 9) return;
    this.model.position.copy(this.pos);
    if (this.state !== 'dead' && this.state !== 'flung') this.model.rotation.y = this.heading;
    this.animAcc = (this.animAcc || 0) + dt;
    if (near || this.animAcc > 0.12) { this.mixer.update(this.animAcc); this.animAcc = 0; this.applyPose(pose, dt); }
  }

  // poses procédurales ajoutées à l'animation : génuflexion, prière, recroquevillé
  applyPose(kind, dt) {
    const target = kind ? 1 : 0;
    this.poseW = lerp(this.poseW, target, 1 - Math.exp(-5 * dt));
    if (kind) this.poseKind = kind;
    const w = this.poseW;
    if (w < 0.01) return;
    const B = this.bones, k = this.poseKind;
    const rot = (name, x, y, z) => { const b = B[name.replace('.', '')]; if (!b) return; _e.set(x * w, y * w, z * w); _q.setFromEuler(_e); b.quaternion.multiply(_q); };
    const sc = this.height / 1.75;
    if (k === 'kneel') {
      rot('UpperLeg.L', -0.15, 0, 0); rot('LowerLeg.L', 1.55, 0, 0); rot('UpperLeg.R', -1.45, 0, 0); rot('LowerLeg.R', 1.55, 0, 0);
      rot('Torso', 0.15, 0, 0);
      this.model.position.y -= 0.5 * sc * w;
    } else if (k === 'pray') {
      rot('UpperLeg.L', -0.1, 0, 0); rot('LowerLeg.L', 1.6, 0, 0); rot('UpperLeg.R', -0.1, 0, 0); rot('LowerLeg.R', 1.6, 0, 0);
      rot('Torso', 0.6, 0, 0); rot('Head', 0.4, 0, 0);
      this.model.position.y -= 0.6 * sc * w;
    } else if (k === 'cower') {
      rot('UpperLeg.L', -0.1, 0, 0); rot('LowerLeg.L', 1.6, 0, 0); rot('UpperLeg.R', -0.1, 0, 0); rot('LowerLeg.R', 1.6, 0, 0);
      rot('Torso', 0.9, 0, 0); rot('Head', 0.55, 0, 0);
      this.model.position.y -= 0.6 * sc * w;
    }
    // les pieds suivent les tibias
    this.model.updateMatrixWorld(true);
    for (const s of ['L', 'R']) {
      const off = this.footOff[s], ll = B['LowerLeg' + s], f = B['Foot' + s];
      if (!off || !ll || !f) continue;
      const tgt = f.parent.worldToLocal(ll.localToWorld(off.clone()));
      f.position.lerp(tgt, w);
    }
  }

  fling(from, power) {
    if (!this.alive) return;
    this.alive = false;
    const d = new THREE.Vector3(this.pos.x - from.x, 0, this.pos.z - from.z).normalize();
    this.vel.set(d.x * power * 0.35, 6 + power * 0.25, d.z * power * 0.35);
    this.model.visible = true;
    this.setState('flung');
  }

  die() {
    this.alive = false;
    this.setState('dead');
    this.play('Death', 0.1);
    this.sys.onDeath(this);
  }

  remove() {
    this.sys.scene.remove(this.model);
    this.removed = true;
    if (G.fx) G.fx.spirit(this.pos.clone().add(new THREE.Vector3(0, 1, 0)));
  }
}

export class People {
  constructor(scene, assets, villages) {
    this.scene = scene;
    this.assets = assets;
    this.villages = villages;
    this.list = [];
    this.events = [];
    this.speech = [];
    this.birthT = 40;
    this.rnd = mulberry32(4242);
    this.deaths = 0;
  }

  spawn(v, home, role, opts = {}) {
    const rnd = this.rnd;
    const female = opts.female ?? rnd() < 0.5;
    const child = role === 'child';
    const base = role === 'chief' ? BASES.chief : female ? BASES.woman : BASES.man;
    const height = child ? 0.95 + rnd() * 0.35 : (female ? 1.58 + rnd() * 0.12 : 1.68 + rnd() * 0.14);
    const name = pick(female ? WOMEN : MEN);
    const p = new Villager(this, base, { rnd, vid: v.id, home, role, child, female, height, name, old: opts.old || role === 'chief' || role === 'priest' });
    home.residents.push(p);
    this.assignWork(p, v);
    this.list.push(p);
    return p;
  }

  assignWork(p, v) {
    const r = p.role;
    const at = (b) => b && b.state === 'intact' ? b.door : null;
    if (r === 'smith' && v.forge) { p.workAt = v.forge.forge ? v.forge.forge.clone().setY(0) : v.forge.door; p.workAnim = 'Interact'; p.workFace = v.forge.forge; }
    else if (r === 'innkeeper') { p.workAt = at(v.tavern); p.workAnim = 'Idle_Neutral'; }
    else if (r === 'priest') { p.workAt = v.temple ? v.temple.door.clone().lerp(new THREE.Vector3(v.temple.x, 0, v.temple.z), 0.25) : null; p.workAnim = 'Idle_Neutral'; }
    else if (r === 'merchant' && v.stalls && v.stalls.length) { const s = pick(v.stalls); p.workAt = s.door; p.workAnim = 'Wave'; p.workFace = v.spots.plaza; }
    else if (r === 'farmer' && v.fields && v.fields.length) { const f = pick(v.fields); p.workAt = new THREE.Vector3(f.x + rand(-f.w / 3, f.w / 3), 0, f.z + rand(-f.d / 3, f.d / 3)); p.workAnim = 'Interact'; }
    else if (r === 'guard') { p.workAt = v.spots.plaza.clone().add(new THREE.Vector3(rand(-v.r * 0.6, v.r * 0.6), 0, rand(-v.r * 0.6, v.r * 0.6))); p.workAnim = 'Idle_Sword'; }
    else if (r === 'fisher' && v.spots.dock) { p.workAt = v.spots.dock; p.workAnim = 'Idle_Neutral'; p.onDock = true; }
    else if (r === 'woodcutter') {
      const t = G.veg && G.veg.trees.reduce((best, t) => { const d = Math.hypot(t.x - v.x, t.z - v.z); return t.state === 0 && d > v.r * 0.7 && (!best || d < best.d) ? { t, d } : best; }, null);
      if (t) { p.workAt = new THREE.Vector3(t.t.x + 1.6, 0, t.t.z); p.workAnim = 'Sword_Slash'; p.workFace = new THREE.Vector3(t.t.x, 0, t.t.z); }
    } else if (r === 'miller' && v.mill) { p.workAt = v.mill.door; p.workAnim = 'Interact'; }
    if (!p.workAt) { p.workAt = p.home.door; p.workAnim = 'Interact'; }
  }

  populate() {
    const scale = Q().label === 'Bas' ? 0.5 : Q().label === 'Moyen' ? 0.75 : 1;
    for (const v of this.villages.villages) {
      const homes = v.buildings.filter((b) => b.type === 'house' || b.type === 'farmhouse' || b.type === 'tavern');
      const jobs = [];
      if (v.main) jobs.push('chief', 'smith', 'innkeeper', 'priest', 'miller', 'guard', 'guard', 'guard', 'merchant', 'merchant', 'merchant', 'merchant');
      const local = v.port ? 'fisher' : v.forest ? 'woodcutter' : v.mountain ? 'shepherd' : 'farmer';
      for (const h of homes) {
        if (this.rnd() > scale) continue;
        const adults = h.type === 'tavern' ? 2 : 1 + (this.rnd() < 0.7 ? 1 : 0);
        for (let a = 0; a < adults; a++) {
          const role = jobs.length ? jobs.shift() : (this.rnd() < 0.6 ? local : (this.rnd() < 0.5 ? 'farmer' : 'crafter'));
          this.spawn(v, h, role, { female: a === 1 ? true : undefined });
        }
        const kids = this.rnd() < 0.3 ? 1 : 0;
        for (let c = 0; c < kids; c++) this.spawn(v, h, 'child');
      }
      v.capacity = Math.round(homes.length * 3.2 * scale);
    }
  }

  nearest(p, r) {
    let best = null, bd = r;
    for (const o of this.list) { if (o === p || !o.alive || o.state === 'hidden') continue; const d = o.pos.distanceTo(p.pos); if (d < bd) { bd = d; best = o; } }
    return best;
  }

  // ---------- API pour les pouvoirs et la destruction ----------
  notify(kind, x, z, r, power) { this.events.push({ kind, x, z, r, power, age: 0 }); }

  hitArea(x, z, r, power, from) {
    let n = 0;
    for (const p of this.list) {
      if (!p.alive) continue;
      const d = Math.hypot(p.pos.x - x, p.pos.z - z);
      if (d > r) continue;
      if (p.state === 'hidden') { if (p.home.state === 'intact') continue; }
      p.fling(from || new THREE.Vector3(x, 0, z), power * (1 - d / r) + 10);
      n++;
    }
    return n;
  }

  bless(x, z, r) {
    let n = 0;
    for (const p of this.list) {
      if (!p.alive || p.pos.distanceTo(new THREE.Vector3(x, p.pos.y, z)) > r) continue;
      p.setState('celebrate'); p.say('celebrate', n < 3); n++;
      if (G.fx) G.fx.glow(p.pos.clone().add(new THREE.Vector3(0, 1.2, 0)), 1.2);
    }
    this.notify('bless', x, z, r, 0);
    return n;
  }

  onDeath(p) {
    this.deaths++;
    const v = p.village;
    v.fear = Math.min(1, v.fear + 0.04);
    v.worship = Math.max(-1, v.worship - 0.02);
    emit('villagerDied', { p, village: v });
    const w = this.nearest(p, 25); if (w) w.say('mourn', true);
  }

  update(dt) {
    for (const e of this.events) e.age += dt;
    this.events = this.events.filter((e) => e.age < 10);
    for (const s of this.speech) s.t -= dt;
    this.speech = this.speech.filter((s) => s.t > 0 && !s.v.removed);
    const cam = G.camera.position;
    for (const p of this.list) {
      if (p.removed) continue;
      const d = p.pos.distanceTo(cam);
      const vis = d < 190 && p.state !== 'hidden';
      if (p.state !== 'hidden') p.model.visible = vis;
      p.update(dt, d < 70);
    }
    this.list = this.list.filter((p) => !p.removed);
    // reconstruction et naissances, village par village
    this.birthT -= dt;
    for (const v of this.villages.villages) {
      const calm = !this.events.some((e) => e.kind !== 'bless' && Math.hypot(e.x - v.x, e.z - v.z) < v.r + 60);
      if (!calm) continue;
      const ruins = v.buildings.filter((b) => b.state === 'ruin');
      const pop = this.list.filter((p) => p.vid === v.id && p.alive);
      if (ruins.length && pop.length >= 2 && v.fear < 0.85) {
        const builders = pop.filter((p) => p.buildTarget).length;
        if (builders < Math.min(4, pop.length - 1)) {
          const p = pop.find((q) => !q.buildTarget && !q.child && q.state !== 'hidden');
          if (p) p.buildTarget = pick(ruins);
        }
      }
      if (this.birthT <= 0 && pop.length && pop.length < (v.capacity || 0) && v.worship >= v.fear) {
        const homes = v.buildings.filter((b) => (b.type === 'house' || b.type === 'farmhouse') && b.state === 'intact');
        if (homes.length) { const p = this.spawn(v, pick(homes), 'child'); emit('journal', { icon: '👶', text: `Un enfant est né à ${v.name} : ${p.name}.` }); }
      }
    }
    if (this.birthT <= 0) this.birthT = rand(90, 160);
  }

  // voyageurs et marchands ambulants sur les routes
  addTravelers(roads, n) {
    this.roads = roads;
    for (let i = 0; i < n && roads.routes.length; i++) {
      const rt = roads.routes[i % roads.routes.length];
      const v = rt.from;
      const home = v.buildings.find((b) => b.type === 'house') || v.buildings[0];
      const p = this.spawn(v, home, 'traveler');
      const k = Math.floor(Math.random() * rt.pts.length);
      p.pos.copy(rt.pts[k]); p.pos.y = G.terrain.heightAt(p.pos.x, p.pos.z);
      p.workAt = null;
    }
  }

  countIn(vid) { return this.list.filter((p) => p.vid === vid && p.alive).length; }
}

export { ROLE_FR };
