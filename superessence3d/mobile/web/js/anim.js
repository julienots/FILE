// Animations procédurales sur le squelette des modèles générés (clips three.js, exportés en glTF).
// Les os sont orientés comme le monde au repos : X = côté, Y = haut, Z = avant.
import * as THREE from "three";

export const MOTIONS = {
  idle: "Repos", breathe: "Respiration", walk: "Marche", run: "Course", jump: "Saut", attack: "Attaque",
  fly: "Vol", wing_flap: "Battement d'ailes", tail_wave: "Queue", head_look: "Regard", tentacle_wave: "Tentacules", slither: "Reptation",
};
export const OBJECT_MOTIONS = { spin: "Rotation", bounce: "Rebond", float: "Flottement", pulse: "Pulsation", sway: "Balancement" };

const idx = (role) => { const n = parseInt(role.split("_").pop(), 10); return Number.isNaN(n) ? 1 : n; };

function channels(motion, bones, t, amp, out) {
  const w = 2 * Math.PI * t, S = Math.sin, R = Math.PI / 180;
  const rot = (b, axis, v) => { const e = (out.get(b) || out.set(b, { r: [0, 0, 0], p: [0, 0, 0], s: [0, 0, 0] }).get(b)); e.r[axis] += v; };
  const pos = (b, axis, v) => { const e = (out.get(b) || out.set(b, { r: [0, 0, 0], p: [0, 0, 0], s: [0, 0, 0] }).get(b)); e.p[axis] += v; };
  const scl = (b, axis, v) => { const e = (out.get(b) || out.set(b, { r: [0, 0, 0], p: [0, 0, 0], s: [0, 0, 0] }).get(b)); e.s[axis] += v; };
  const by = (prefix) => bones.filter((b) => (b.userData.role || "").startsWith(prefix));
  const role = (r) => bones.filter((b) => b.userData.role === r);
  const H = Math.max(0.2, bones.height || 1);
  if (motion === "walk" || motion === "run") {
    const k = motion === "walk" ? 1 : 1.7, a = 22 * R * amp * k;
    for (const b of by("leg_")) {
      const r = b.userData.role, side = r.includes("_L_") ? 1 : -1;
      const front = r.includes("_front_") || !r.includes("_back_");
      const ph = (front ? 0 : Math.PI) + (side === 1 ? 0 : Math.PI);
      if (r.endsWith("_upper")) rot(b, 0, -a * S(w + ph));
      else if (r.endsWith("_lower")) rot(b, 0, Math.abs(a) * 0.8 * Math.max(0, S(w + ph + 1.2)));
      else if (r.endsWith("_foot")) rot(b, 0, -a * 0.4 * S(w + ph - 0.8));
    }
    for (const b of by("arm_")) { const r = b.userData.role, side = r.includes("_L_") ? 1 : -1; if (r.endsWith("_upper")) rot(b, 0, a * 0.7 * S(w + (side === 1 ? 0 : Math.PI))); else if (r.endsWith("_lower")) rot(b, 0, -a * 0.4); }
    for (const b of role("root")) pos(b, 1, 0.02 * H * amp * k * Math.abs(S(2 * w)));
    for (const b of by("tail_")) rot(b, 1, 8 * R * amp * S(w + idx(b.userData.role) * 0.6));
    for (const b of role("head")) rot(b, 0, 3 * R * amp * S(2 * w));
  } else if (motion === "jump") {
    const h = t >= 0.2 && t <= 0.8 ? Math.max(0, S(Math.PI * (t - 0.2) / 0.6)) : 0;
    const c = t < 0.2 ? Math.max(0, S(Math.PI * t / 0.2)) : t > 0.8 ? Math.max(0, S(Math.PI * (t - 0.8) / 0.2)) : 0;
    for (const b of role("root")) pos(b, 1, 0.5 * H * amp * h - 0.06 * H * c);
    for (const b of by("leg_")) { if (b.userData.role.endsWith("_upper")) rot(b, 0, -(35 * c + 20 * h) * R * amp); else if (b.userData.role.endsWith("_lower")) rot(b, 0, (60 * c + 30 * h) * R * amp); }
    for (const b of by("arm_")) if (b.userData.role.endsWith("_upper")) rot(b, 0, 60 * R * h * amp);
    for (const b of by("wing_")) rot(b, 2, 35 * R * h * amp * (b.userData.role.includes("_L_") ? 1 : -1));
  } else if (motion === "attack") {
    let e = t < 0.35 ? S(Math.PI / 2 * t / 0.35) : t < 0.5 ? 1 - 2.2 * (t - 0.35) / 0.15 : -1.2 * (1 - (t - 0.5) / 0.5);
    e *= amp;
    for (const b of [...role("head"), ...role("neck")]) rot(b, 0, 25 * R * e);
    for (const b of [...role("chest"), ...role("spine")]) rot(b, 0, 8 * R * e);
    for (const b of by("arm_")) { const r = b.userData.role; if (r.endsWith("_upper")) rot(b, 0, (r.includes("_R_") ? 70 : 20) * R * e); else if (r.endsWith("_lower")) rot(b, 0, 40 * R * Math.max(0, e)); }
    for (const b of by("wing_")) rot(b, 2, 30 * R * Math.abs(e) * (b.userData.role.includes("_L_") ? 1 : -1));
    for (const b of by("tail_")) rot(b, 1, 15 * R * e * S(idx(b.userData.role)));
  } else if (motion === "wing_flap" || motion === "fly") {
    const k = motion === "fly" ? 1.2 : 1;
    for (const b of by("wing_")) { const side = b.userData.role.includes("_L_") ? 1 : -1, i = idx(b.userData.role); rot(b, 2, side * 38 * R * amp * k * S(w - 0.5 * (i - 1))); rot(b, 1, side * 6 * R * amp * S(w + Math.PI / 2)); }
    if (motion === "fly") {
      for (const b of role("root")) pos(b, 1, 0.08 * H * amp * S(w + Math.PI));
      for (const b of by("leg_")) { if (b.userData.role.endsWith("_upper")) rot(b, 0, -40 * R * amp); else if (b.userData.role.endsWith("_lower")) rot(b, 0, -50 * R * amp); }
      for (const b of by("tail_")) rot(b, 0, 5 * R * amp * S(w + idx(b.userData.role) * 0.7));
    }
  } else if (motion === "tail_wave" || motion === "slither") {
    const a = (motion === "tail_wave" ? 14 : 18) * R * amp;
    for (const b of by("tail_")) { const i = idx(b.userData.role); rot(b, 1, a * S(w - i * 0.8)); if (motion === "tail_wave") rot(b, 0, a * 0.3 * S(w * 0.5 - i * 0.5)); }
    if (motion === "slither") for (const b of role("head")) rot(b, 1, a * 0.5 * S(w + 0.8));
  } else if (motion === "tentacle_wave") {
    for (const b of by("tentacle_")) { const p = b.userData.role.split("_"); const k = +p[1] || 0, i = +p[2] || 1; rot(b, 0, 18 * R * amp * S(w - i * 0.9 + k * 1.3)); rot(b, 2, 12 * R * amp * S(w * 0.5 - i * 0.6 + k * 2.1)); }
  } else if (motion === "breathe") {
    const b0 = 0.5 + 0.5 * S(w);
    for (const b of role("chest").length ? role("chest") : role("spine").slice(0, 1)) { scl(b, 0, 0.05 * amp * b0); scl(b, 1, 0.03 * amp * b0); scl(b, 2, 0.05 * amp * b0); }
    for (const b of role("head")) rot(b, 0, -1.5 * R * amp * b0);
  } else if (motion === "idle") {
    channels("breathe", bones, t, amp * 0.8, out);
    for (const b of role("head")) rot(b, 1, 6 * R * amp * S(w));
    for (const b of by("tail_")) rot(b, 1, 6 * R * amp * S(w - idx(b.userData.role) * 0.6));
    for (const b of by("wing_")) rot(b, 2, 3 * R * amp * S(w) * (b.userData.role.includes("_L_") ? 1 : -1));
  } else if (motion === "head_look") {
    for (const b of role("head")) { rot(b, 1, 35 * R * amp * S(w)); rot(b, 0, 8 * R * amp * S(2 * w)); }
    for (const b of role("neck")) rot(b, 1, 15 * R * amp * S(w));
  }
}

/** Crée un clip combinant plusieurs mouvements. Renvoie null si aucun os ne correspond. */
export function makeClip(root, motions, { amplitude = 1, seconds = 1.5, name = null } = {}) {
  const bones = [];
  root.traverse((o) => { if (o.isBone) bones.push(o); });
  if (!bones.length) return null;
  const box = new THREE.Box3().setFromObject(root);
  bones.height = box.max.y - box.min.y;
  const rest = new Map(bones.map((b) => [b, { p: b.position.clone(), q: b.quaternion.clone(), s: b.scale.clone() }]));
  const N = 24;
  const tracks = new Map();
  const times = Array.from({ length: N + 1 }, (_, i) => (i / N) * seconds);
  for (let i = 0; i <= N; i++) {
    const t = (i % N) / N;
    const out = new Map();
    for (const m of motions) channels(m, bones, t, amplitude, out);
    for (const [b, ch] of out) {
      if (!tracks.has(b)) tracks.set(b, { q: [], p: [], s: [] });
      const tr = tracks.get(b), r = rest.get(b);
      const q = r.q.clone().multiply(new THREE.Quaternion().setFromEuler(new THREE.Euler(ch.r[0], ch.r[1], ch.r[2])));
      tr.q.push(q.x, q.y, q.z, q.w);
      tr.p.push(r.p.x + ch.p[0], r.p.y + ch.p[1], r.p.z + ch.p[2]);
      tr.s.push(r.s.x * (1 + ch.s[0]), r.s.y * (1 + ch.s[1]), r.s.z * (1 + ch.s[2]));
    }
  }
  if (!tracks.size) return null;
  const kt = [];
  for (const [b, tr] of tracks) {
    kt.push(new THREE.QuaternionKeyframeTrack(`${b.name}.quaternion`, times, tr.q));
    if (tr.p.some((v, i) => Math.abs(v - [rest.get(b).p.x, rest.get(b).p.y, rest.get(b).p.z][i % 3]) > 1e-6)) kt.push(new THREE.VectorKeyframeTrack(`${b.name}.position`, times, tr.p));
    if (tr.s.some((v) => Math.abs(v - 1) > 1e-6)) kt.push(new THREE.VectorKeyframeTrack(`${b.name}.scale`, times, tr.s));
  }
  return new THREE.AnimationClip(name || motions.map((m) => MOTIONS[m] || m).join(" + "), seconds, kt);
}

/** Animation d'un objet entier (sans squelette). */
export function makeObjectClip(obj, motion, { amplitude = 1, seconds = 2 } = {}) {
  const N = 32, times = [], vals = [];
  const p0 = obj.position.clone(), q0 = obj.quaternion.clone(), s0 = obj.scale.clone();
  const track = { spin: "quaternion", bounce: "position", float: "position", pulse: "scale", sway: "quaternion" }[motion];
  for (let i = 0; i <= N; i++) {
    const t = i / N, w = 2 * Math.PI * t;
    times.push(t * seconds);
    if (motion === "spin") { const q = q0.clone().multiply(new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 1, 0), w * amplitude)); vals.push(q.x, q.y, q.z, q.w); }
    else if (motion === "sway") { const q = q0.clone().multiply(new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 0, 1), Math.sin(w) * 0.12 * amplitude)); vals.push(q.x, q.y, q.z, q.w); }
    else if (motion === "bounce") vals.push(p0.x, p0.y + Math.abs(Math.sin(w)) * 0.5 * amplitude, p0.z);
    else if (motion === "float") vals.push(p0.x, p0.y + Math.sin(w) * 0.15 * amplitude, p0.z);
    else { const k = 1 + 0.08 * amplitude * Math.sin(w); vals.push(s0.x * k, s0.y * k, s0.z * k); }
  }
  const T = track === "quaternion" ? THREE.QuaternionKeyframeTrack : THREE.VectorKeyframeTrack;
  obj.name ||= "Objet";
  return new THREE.AnimationClip(OBJECT_MOTIONS[motion], seconds, [new T(`${obj.name}.${track}`, times, vals)]);
}
