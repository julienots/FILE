// Forme humaine du dieu : un homme (≈ 1,9 m) animé par de vraies animations de personnage
// (marche, course, coups de poing, coups de pied, roulade, salut) mélangées en douceur, plus des
// poses procédurales par-dessus (vol divin, vol « comète », atterrissage de super-héros, incantations).
// Il garde TOUS les pouvoirs du dieu.
import * as THREE from 'three';
import * as SkeletonUtils from 'three/addons/utils/SkeletonUtils.js';
import { G, clamp, lerp, damp } from './core.js';

export const HUMAN_HEIGHT = 1.9;
const _e = new THREE.Euler(), _q = new THREE.Quaternion(), _v = new THREE.Vector3();

export function buildHuman(asset) {
  const root = new THREE.Group();
  const m = SkeletonUtils.clone(asset.gltf.scene);
  // tenue divine : noir profond et or, sans sac
  const dye = { Green: 0x16141c, LightGreen: 0x2a2433, Brown: 0x1d1a22, Brown2: 0x3a2f1e, Grey: 0x24222a, Black: 0x0e0d12 };
  const cache = new Map();
  m.traverse((o) => {
    if (!o.isMesh) return;
    o.castShadow = true; o.frustumCulled = false;
    if (o.name === 'Backpack') o.visible = false;
    const tint = (mt) => {
      if (cache.has(mt)) return cache.get(mt);
      let c = mt;
      if (dye[mt.name] !== undefined) { c = mt.clone(); c.color = new THREE.Color(dye[mt.name]); c.roughness = 0.55; }
      if (mt.name === 'Gold') { c = mt.clone(); c.emissive = new THREE.Color(0xffb040); c.emissiveIntensity = 0.6; c.metalness = 1; c.roughness = 0.3; }
      cache.set(mt, c); return c;
    };
    o.material = Array.isArray(o.material) ? o.material.map(tint) : tint(o.material);
  });
  { const sw = m.getObjectByName('Sword'); if (sw) sw.visible = false; }
  m.scale.setScalar(HUMAN_HEIGHT / asset.height);
  root.add(m);
  const mixer = new THREE.AnimationMixer(m);
  const acts = {};
  for (const [k, c] of Object.entries(asset.clips)) acts[k] = mixer.clipAction(c);
  for (const k of ['Punch_Left', 'Punch_Right', 'Kick_Left', 'Kick_Right', 'Roll', 'Wave', 'Sword_Slash', 'HitRecieve', 'Interact']) if (acts[k]) { acts[k].setLoop(THREE.LoopOnce); acts[k].clampWhenFinished = true; }
  const bones = {};
  m.traverse((o) => { if (o.isBone) bones[o.name] = o; });
  m.updateMatrixWorld(true);
  const footOff = {};
  for (const s of ['L', 'R']) { const ll = bones['LowerLeg' + s], f = bones['Foot' + s]; if (ll && f) footOff[s] = ll.worldToLocal(f.getWorldPosition(new THREE.Vector3())); }
  // interface commune avec les autres modèles (main droite pour les pouvoirs, accessoires factices)
  const dummy = () => new THREE.Object3D();
  const B = { handR: bones.WristR || dummy(), handL: bones.WristL || dummy(), head: bones.Head || dummy(), clothF: dummy(), clothB: dummy(), haloRays: dummy(), core: dummy(), haloGlow: { material: { opacity: 1 } } };
  const H = new HumanRig(m, mixer, acts, bones, footOff);
  return { root, bones: B, human: true, rig: H, theme: { main: 0xffd27a, alt: 0xff9a40, core: 0xfff4d0 }, updateWings() {}, animate: (god, dt) => H.animate(god, dt) };
}

// actions du dieu → animations du personnage
const ACTION_CLIP = { punch: 'Punch_Right', jabR: 'Punch_Right', jabL: 'Punch_Left', hook: 'Punch_Right', uppercut: 'Kick_Right', slam: 'Sword_Slash', pose: 'Wave' };

class HumanRig {
  constructor(m, mixer, acts, bones, footOff) {
    Object.assign(this, { m, mixer, acts, bones, footOff });
    this.base = null; this.one = null; this.oneKind = null;
    this.w = { fly: 0, boost: 0, air: 0, land: 0, cast: 0, raise: 0, push: 0, charge: 0 };
    this.play('Idle');
  }

  play(name, fade = 0.25, speed = 1) {
    const a = this.acts[name] || this.acts.Idle;
    a.timeScale = speed;
    if (this.base === a) return;
    a.reset(); a.enabled = true; a.setEffectiveWeight(1); a.play();
    if (this.base) a.crossFadeFrom(this.base, fade, false);
    this.base = a;
  }
  oneShot(name, dur) {
    const a = this.acts[name]; if (!a) return;
    a.reset(); a.enabled = true; a.setEffectiveWeight(1); a.timeScale = a.getClip().duration / Math.max(0.2, dur);
    a.play(); if (this.base) a.crossFadeFrom(this.base, 0.08, false);
    this.one = a; this.oneT = dur;
  }

  rot(name, x, y, z, w) { const b = this.bones[name]; if (!b || w < 0.005) return; _e.set(x * w, y * w, z * w); _q.setFromEuler(_e); b.quaternion.multiply(_q); }

  animate(god, dt) {
    const a = god.anim, W = this.w;
    const onGround = god.state === 'ground', flying = god.state === 'fly';
    const ac = god.action;
    // ----- animation de base selon le mouvement
    if (onGround) {
      if (god.speed < 0.6) this.play(god.aura && god.aura.active ? 'Idle_Sword' : 'Idle', 0.35);
      else if (god.speed < 8) this.play('Walk', 0.3, clamp(god.speed / 3.2, 0.6, 2));
      else this.play('Run', 0.25, clamp(god.speed / 7.5, 1, 2.6));
    } else this.play('Idle_Neutral', 0.4);
    // ----- coups : vraies animations de frappe, déclenchées une fois par action
    if (ac && ac !== this.lastAction) {
      this.lastAction = ac;
      const clip = ACTION_CLIP[ac.kind];
      if (clip) this.oneShot(clip, ac.dur);
    }
    if (!ac) this.lastAction = null;
    if (this.one) { this.oneT -= dt; if (this.oneT <= 0) { this.one.fadeOut(0.15); this.base.reset().fadeIn(0.15).play(); this.one = null; } }
    this.mixer.update(dt);
    // ----- poses procédurales par-dessus l'animation
    const boost = a.boost * a.fly, hov = a.fly * (1 - a.boost);
    const kind = ac && !ACTION_CLIP[ac.kind] ? ac.kind : null;
    const env = ac ? Math.sin(Math.min(1, ac.t / ac.dur) * Math.PI) : 0;
    W.cast = damp(W.cast, kind === 'cast' || kind === 'push' ? 1 : 0, 10, dt);
    W.raise = damp(W.raise, kind === 'raise' || kind === 'summon' ? 1 : 0, 8, dt);
    W.charge = damp(W.charge, kind === 'charge' || kind === 'transform' ? 1 : 0, 6, dt);
    W.air = damp(W.air, god.state === 'air' || god.state === 'knock' ? 1 : 0, 6, dt);
    const t = G.time, sway = Math.sin(t * 1.3);
    // vol divin : jambes souples, un genou plié, pointes vers le bas ; bras légèrement écartés
    this.rot('UpperLegL', -0.08 - sway * 0.03, 0, 0, hov); this.rot('LowerLegL', 0.22, 0, 0, hov);
    this.rot('UpperLegR', -0.3 + sway * 0.04, 0, 0, hov); this.rot('LowerLegR', 0.75, 0, 0, hov);
    this.rot('UpperArmR', 0.35, 0, 0.15, hov); this.rot('UpperArmL', -0.35, 0, -0.15, hov);
    this.rot('LowerArmR', -0.3, 0, 0, hov); this.rot('LowerArmL', -0.3, 0, 0, hov);
    // vol « comète » : poing droit tendu devant, bras gauche le long du corps, jambes tendues
    this.rot('UpperArmR', 0, 0, 3.0, boost); this.rot('LowerArmR', 0.1, 0, 0, boost);
    this.rot('UpperArmL', 0.1, 0, 0.15, boost);
    this.rot('UpperLegR', 0.12, 0, 0, boost); this.rot('LowerLegR', 0.35, 0, 0, boost);
    // saut / projection : jambes repliées, bras ouverts
    this.rot('UpperLegL', -0.9, 0, 0, W.air); this.rot('LowerLegL', 1.3, 0, 0, W.air);
    this.rot('UpperLegR', -0.5, 0, 0, W.air); this.rot('LowerLegR', 1.0, 0, 0, W.air);
    this.rot('UpperArmR', 1.0, 0, 0, W.air); this.rot('UpperArmL', -1.0, 0, 0, W.air);
    // atterrissage de super-héros : genou gauche à terre, jambe droite en avant, poing au sol
    const L = a.land;
    this.rot('UpperLegL', -0.15, 0, 0, L); this.rot('LowerLegL', 1.6, 0, 0, L);
    this.rot('UpperLegR', -1.45, 0, 0, L); this.rot('LowerLegR', 1.55, 0, 0, L);
    this.rot('Torso', 0.45, 0, 0, L); this.rot('Head', -0.35, 0, 0, L);
    this.rot('UpperArmR', 0, 0, 0.9, L);
    // incantations : bras tendu vers la cible / deux bras levés / concentration (transformation)
    this.rot('UpperArmR', 0, 0, 1.5 * env, W.cast); this.rot('UpperArmL', 0, 0, -1.5 * env, W.cast);
    this.rot('UpperArmR', 0, 0, 3.0 * env, W.raise); this.rot('UpperArmL', 0, 0, -3.0 * env, W.raise); this.rot('Head', -0.4 * env, 0, 0, W.raise);
    this.rot('UpperArmR', 0.6, 0, 0.5, W.charge); this.rot('UpperArmL', -0.6, 0, -0.5, W.charge); this.rot('LowerArmR', -1.2, 0, 0, W.charge); this.rot('LowerArmL', -1.2, 0, 0, W.charge);
    this.rot('Torso', 0.25, 0, 0, W.charge * (ac && ac.t / ac.dur < 0.75 ? 1 : -1)); this.rot('Head', ac && ac.t / ac.dur > 0.75 ? -0.5 : 0.3, 0, 0, W.charge);
    // dash / plongeon : mêmes poses que la comète
    const dive = kind === 'dash' || kind === 'dive' ? 1 : 0;
    this.rot('UpperArmR', 0, 0, 3.0, dive); this.rot('UpperArmL', 0, 0, -3.0, dive * (kind === 'dive' ? 1 : 0));
    // posture abaissée à l'atterrissage
    const drop = (a.land * 0.55 + a.crouch * 0.3) * HUMAN_HEIGHT / 1.8;
    this.m.position.y = -drop;
    // les pieds (contrôleurs indépendants du squelette) suivent les tibias quand les jambes sont pliées
    const legW = Math.min(1, hov + boost + W.air + L);
    if (legW > 0.01) {
      this.m.updateMatrixWorld(true);
      for (const s of ['L', 'R']) {
        const off = this.footOff[s], ll = this.bones['LowerLeg' + s], f = this.bones['Foot' + s];
        if (!off || !ll || !f) continue;
        f.position.lerp(f.parent.worldToLocal(ll.localToWorld(_v.copy(off))), legW);
      }
    }
  }
}
