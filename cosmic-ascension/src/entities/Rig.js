import * as THREE from 'three';
// Pilotage des modèles à squelette (clips Tripo): locomotion calée sur la vitesse réelle, pose d'arrêt, coups sur le haut du corps.
// Les clips Tripo contiennent un déplacement de la racine (le personnage avance puis « saute » en arrière à chaque boucle):
// `prepareClips` le retire (tendance linéaire de la piste de position) et mémorise la foulée par cycle.
const UPPER = /Waist|Spine|Neck|Head|Facial|Eye|Clavicle|Upperarm|Forearm|Elbow|Hand|Pinky|Ring|Mid|Index|Thumb/i;
export function prepareClips(clips) {
  for (const c of clips) { if (c.userData?.prepared) continue; c.userData = { prepared: true, stride: 0 };
    for (const t of c.tracks) { if (!t.name.endsWith('.position')) continue; const v = t.values, n = t.times.length, T = t.times[n - 1] - t.times[0] || 1;
      const d = [v[(n - 1) * 3] - v[0], v[(n - 1) * 3 + 1] - v[1], v[(n - 1) * 3 + 2] - v[2]], len = Math.hypot(...d); if (len < .05) continue;
      for (let i = 0; i < n; i++) { const k = (t.times[i] - t.times[0]) / T; v[i * 3] -= d[0] * k; v[i * 3 + 1] -= d[1] * k; v[i * 3 + 2] -= d[2] * k; }
      if (len > c.userData.stride) { c.userData.stride = len; c.userData.rootBone = t.name.split('.')[0]; } } }
  return clips;
}
const qa = new THREE.Quaternion(), qb = new THREE.Quaternion();
// Pose d'arrêt: instant de la marche où les deux cuisses sont les plus proches de leur orientation moyenne (jambes qui se croisent).
function standTime(clip) {
  const th = clip.tracks.filter(t => /Thigh\.quaternion$/i.test(t.name) && !/Twist/i.test(t.name)); if (!th.length) return 0;
  const means = th.map(t => { const m = new THREE.Quaternion(0, 0, 0, 0), ref = new THREE.Quaternion().fromArray(t.values, 0); for (let i = 0; i < t.times.length; i++) { qa.fromArray(t.values, i * 4); if (qa.dot(ref) < 0) qa.set(-qa.x, -qa.y, -qa.z, -qa.w); m.x += qa.x; m.y += qa.y; m.z += qa.z; m.w += qa.w; } return m.normalize(); });
  let best = 0, bd = 1e9; const n = th[0].times.length;
  for (let i = 0; i < n; i++) { let d = 0; th.forEach((t, k) => { if (i < t.times.length) d += means[k].angleTo(qb.fromArray(t.values, i * 4)); }); if (d < bd) { bd = d; best = th[0].times[i]; } }
  return best;
}
function staticClip(clip, time, name) {
  const tracks = clip.tracks.map(t => { const size = t.getValueSize(), interp = t.createInterpolant(), v = Array.from(interp.evaluate(time)).slice(0, size); return new t.constructor(t.name, [0, 1], [...v, ...v]); });
  return new THREE.AnimationClip(name, 1, tracks);
}
const upperOnly = (clip) => new THREE.AnimationClip(clip.name + '_upper', clip.duration, clip.tracks.filter(t => UPPER.test(t.name.split('.')[0])));

export class Rig {
  // model: Group renvoyé par Assets.get (userData.mixer, userData.animations). unit: mètres par unité d'os (échelle monde de la racine, hors échelle du héros).
  constructor(model) {
    const clips = prepareClips(model.userData.animations), mixer = this.mixer = model.userData.mixer, find = (...n) => clips.find(c => n.some(x => c.name.toLowerCase().includes(x)));
    const walk = find('walk'), run = find('run') ?? walk, box = find('box_01') ?? find('box'), kick = find('kick_02') ?? find('kick');
    model.updateMatrixWorld(true); let unit = 1; { const b = model.getObjectByName(walk?.userData.rootBone ?? run?.userData.rootBone ?? ''); if (b?.parent) unit = b.parent.getWorldScale(new THREE.Vector3()).y; }
    const A = (c, w = 0) => { if (!c) return null; const a = mixer.clipAction(c); a.play(); a.setEffectiveWeight(w); return a; };
    this.idle = A(walk && staticClip(walk, standTime(walk), 'idle'), 1); this.walk = A(walk); this.run = run !== walk ? A(run) : null;
    this.punch = A(box && upperOnly(box)); this.kick = A(kick);
    this.walkV = walk ? walk.userData.stride * unit / walk.duration : 1.4; this.runV = run ? run.userData.stride * unit / run.duration : 4;   // vitesse naturelle de chaque clip (m/s à l'échelle 1)
    this.w = { idle: 1, walk: 0, run: 0, punch: 0, kick: 0 }; this.running = false;
  }
  // v: vitesse au sol en m/s ramenée à l'échelle 1 du héros; air: en l'air; attacking: tir en cours
  update(dt, v, air, attacking) {
    const k = Math.min(1, dt * 7), moving = v > .25, w = this.w;
    if (this.run) this.running = moving && (this.running ? v > this.walkV * 1.5 : v > this.walkV * 2.1);      // hystérésis: pas de bascule marche/course à chaque image
    const tgt = { idle: moving ? 0 : 1, walk: moving && !this.running ? 1 : 0, run: moving && this.running ? 1 : 0, punch: attacking && (moving || !this.kick || this.combo % 3 !== 2) ? 1 : 0, kick: attacking && !moving && this.kick && this.combo % 3 === 2 ? 1 : 0 };
    if (attacking && !this.wasAtk) { this.combo = (this.combo ?? -1) + 1; if (this.punch) this.punch.time = 0; if (this.kick) this.kick.time = 0; } this.wasAtk = attacking;
    for (const n in w) w[n] += (tgt[n] - w[n]) * k;
    const full = 1 - w.kick;     // le coup de pied prend tout le corps; le coup de poing ne remplace que le haut du corps (poids fort)
    this.idle?.setEffectiveWeight(w.idle * full); this.walk?.setEffectiveWeight(w.walk * full); this.run?.setEffectiveWeight(w.run * full); this.punch?.setEffectiveWeight(w.punch * 8); this.kick?.setEffectiveWeight(w.kick * 8);
    // cadence = vitesse réelle / vitesse naturelle du clip (pieds qui ne patinent pas), bornée pour rester lisible
    if (this.walk) this.walk.timeScale = THREE.MathUtils.clamp(v / this.walkV, .6, 1.7) * (air ? .35 : 1);
    if (this.run) this.run.timeScale = THREE.MathUtils.clamp(v / this.runV, .7, 1.5) * (air ? .35 : 1);
    if (this.punch) this.punch.timeScale = 1.5; if (this.kick) this.kick.timeScale = 1.4;
    this.mixer.update(dt);
  }
}
