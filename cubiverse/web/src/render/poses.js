// Animations procédurales des personnages.
// Une pose = angles d'articulations + décalages du corps, calculés à partir du temps.
// Les transitions sont fondues (blend) entre la pose précédente et la nouvelle.

// Champs : by (hauteur du bassin), bl (inclinaison avant du buste), br (roulis),
// hp/hr (tête tangage/roulis), aLp/aLr/eL (épaule gauche tangage, écartement, coude) idem droite,
// lLp/kL (hanche gauche, genou) idem droite, rx (rotation corps entier autour de X : roulade / couché),
// ry (rotation supplémentaire en lacet), face: expression forcée.
const Z = { by: 0, bl: 0, br: 0, hp: 0, hr: 0, aLp: 0, aLr: 0.12, eL: 0.15, aRp: 0, aRr: 0.12, eR: 0.15, lLp: 0, kL: 0, lRp: 0, kR: 0, rx: 0, ry: 0, oy: 0 };
export const POSE_KEYS = Object.keys(Z);
const S = Math.sin, C = Math.cos, PI = Math.PI;

function base() { return { ...Z }; }

const POSES = {
  idle(t, p) { p.by = S(t * 2) * 0.004; p.hr = S(t * 0.7) * 0.12; p.hp = S(t * 0.5) * 0.05; p.aLp = S(t * 2) * 0.04; p.aRp = -S(t * 2) * 0.04; },
  walk(t, p) { const w = t * 9; p.lLp = S(w) * 0.6; p.lRp = -S(w) * 0.6; p.kL = Math.max(0, -S(w)) * 0.8; p.kR = Math.max(0, S(w)) * 0.8; p.aLp = -S(w) * 0.55; p.aRp = S(w) * 0.55; p.by = Math.abs(C(w)) * 0.018; p.bl = 0.05; },
  run(t, p) { const w = t * 15; p.lLp = S(w) * 0.95; p.lRp = -S(w) * 0.95; p.kL = Math.max(0, -S(w)) * 1.4 + 0.3; p.kR = Math.max(0, S(w)) * 1.4 + 0.3; p.aLp = -S(w) * 1.0; p.aRp = S(w) * 1.0; p.eL = p.eR = 1.3; p.by = Math.abs(C(w)) * 0.04; p.bl = 0.22; },
  runInPlace(t, p) { POSES.run(t, p); },
  jump(t, p) { const k = (t % 0.8) / 0.8; const h = Math.max(0, S(k * PI)); p.oy = h * 0.22; p.kL = p.kR = (1 - h) * 0.6; p.lLp = p.lRp = -(1 - h) * 0.3; p.aLp = p.aRp = -2.6 * h - 0.3; p.aLr = p.aRr = 0.4; p.face = 'joy'; },
  dance(t, p) { const w = t * 6; p.by = Math.abs(S(w)) * 0.03; p.br = S(w * 0.5) * 0.25; p.hr = S(w * 0.5) * 0.3; p.aLp = -1.4 + S(w) * 0.9; p.aRp = -1.4 - S(w) * 0.9; p.aLr = p.aRr = 0.6; p.eL = p.eR = 0.8; p.lLp = S(w) * 0.3; p.lRp = -S(w) * 0.3; p.kL = Math.max(0, S(w)) * 0.5; p.kR = Math.max(0, -S(w)) * 0.5; p.ry = S(w * 0.25) * 0.8; p.face = 'joy'; },
  roll(t, p) { const k = Math.min(1, t / 1.6); p.rx = k * PI * 2; p.oy = S(k * PI) * 0.12 + 0.05; p.kL = p.kR = 2.2; p.lLp = p.lRp = -1.6; p.aLp = p.aRp = -0.6; p.eL = p.eR = 2; p.face = 'surprise'; },
  fall(t, p) { const k = Math.min(1, t / 0.45); p.rx = -k * PI / 2; p.oy = -0.24 * k; p.aLp = p.aRp = -2.4 * k; p.aLr = p.aRr = 0.8; p.lLp = 0.3 * k; p.face = 'surprise'; },
  getup(t, p) { const k = Math.min(1, t / 1.0); p.rx = -(1 - k) * PI / 2; p.oy = -0.24 * (1 - k); p.kL = p.kR = S(k * PI) * 1.2; p.lLp = p.lRp = -S(k * PI) * 0.8; p.aLp = p.aRp = -0.5 * (1 - k); },
  sit(t, p) { p.by = -0.14; p.lLp = p.lRp = -1.5; p.kL = p.kR = 1.5; p.aLp = p.aRp = -0.35; p.eL = p.eR = 0.6; p.hr = S(t * 0.6) * 0.15; },
  sitFloor(t, p) { p.by = -0.25; p.lLp = p.lRp = -1.45; p.kL = p.kR = 0.15; p.aLp = p.aRp = 0.5; p.aLr = p.aRr = 0.3; p.bl = -0.15; p.hr = S(t * 0.5) * 0.2; },
  sleep(t, p) { p.rx = -PI / 2; p.oy = -0.24; p.aLp = p.aRp = 0.1; p.kL = 0.2; p.hr = 0.3; p.by = S(t * 1.4) * 0.006; p.face = 'sleep'; },
  eat(t, p) { p.aRp = -1.6 + S(t * 5) * 0.25; p.eR = 1.9; p.aLp = -0.6; p.eL = 1.2; p.hp = 0.1 + S(t * 5) * 0.05; p.face = 'joy'; },
  drink(t, p) { p.aRp = -2.0; p.eR = 2.1; p.hp = -0.35; p.aLp = -0.2; },
  laugh(t, p) { p.bl = -0.15 + S(t * 14) * 0.05; p.hp = -0.3 + S(t * 14) * 0.08; p.aLp = p.aRp = -0.4; p.aLr = p.aRr = 0.25; p.eL = p.eR = 1.6; p.by = Math.abs(S(t * 14)) * 0.01; p.face = 'laugh'; },
  cry(t, p) { p.bl = 0.25; p.hp = 0.45; p.aLp = p.aRp = -1.9; p.eL = p.eR = 2.4; p.aLr = p.aRr = -0.25; p.by = S(t * 10) * 0.006; p.face = 'sadness'; },
  sulk(t, p) { p.bl = 0.12; p.hp = 0.3; p.hr = 0.25; p.aLp = p.aRp = -0.9; p.aLr = p.aRr = -0.55; p.eL = p.eR = 2.2; p.ry = 0.5; p.face = 'anger'; },
  wave(t, p) { p.aRp = -2.6; p.aRr = 0.3 + S(t * 9) * 0.35; p.eR = 0.4; p.hr = 0.1; p.face = 'joy'; },
  coucou(t, p) { p.aRp = -2.7; p.aLp = -2.7; p.aRr = 0.4 + S(t * 10) * 0.35; p.aLr = 0.4 - S(t * 10) * 0.35; p.oy = Math.max(0, S(t * 5)) * 0.04; p.face = 'joy'; },
  fight(t, p) { const w = t * 18; p.aLp = -1.5 + S(w) * 0.8; p.aRp = -1.5 - S(w) * 0.8; p.eL = p.eR = 1.0; p.bl = 0.25; p.oy = Math.abs(S(w * 0.5)) * 0.05; p.lLp = S(w) * 0.4; p.lRp = -S(w) * 0.4; p.ry = S(w * 0.3) * 0.4; p.face = 'anger'; },
  handshake(t, p) { p.aRp = -1.0 + S(t * 12) * 0.15; p.eR = 0.3; p.face = 'joy'; },
  hug(t, p) { p.aLp = p.aRp = -1.5; p.aLr = p.aRr = -0.2; p.eL = p.eR = 1.0; p.bl = 0.15; p.br = S(t * 3) * 0.12; p.face = 'joy'; },
  throw(t, p) { const k = (t % 1.15) / 1.15; p.aRp = k < 0.5 ? 0.9 - k * 2 : -2.6 + (k - 0.5) * 3; p.eR = 0.3; p.bl = k < 0.5 ? -0.1 : 0.15; p.lLp = 0.3; p.lRp = -0.2; },
  catch(t, p) { p.aLp = p.aRp = -1.3; p.aLr = p.aRr = 0.15; p.eL = p.eR = 0.6; p.kL = p.kR = 0.35; p.lLp = p.lRp = -0.2; p.by = -0.03; },
  build(t, p) { const w = t * 8; p.bl = 0.55; p.aRp = -1.2 + S(w) * 0.7; p.eR = 0.8; p.aLp = -0.9; p.eL = 1.2; p.kL = p.kR = 0.6; p.lLp = p.lRp = -0.4; p.by = -0.06; p.hp = 0.2; },
  cook(t, p) { const w = t * 6; p.aRp = -1.1 + S(w) * 0.25; p.aRr = 0.2 + C(w) * 0.25; p.eR = 1.2; p.aLp = -0.8; p.eL = 1.3; p.hp = 0.3; p.bl = 0.1; },
  clean(t, p) { const w = t * 7; p.aRp = -1.0; p.aRr = 0.3 + S(w) * 0.6; p.eR = 0.6; p.aLp = -0.4; p.bl = 0.35; p.hp = 0.2; },
  draw(t, p) { const w = t * 5; p.aRp = -1.7 + S(w) * 0.2; p.aRr = 0.2 + C(w * 1.3) * 0.2; p.eR = 0.6; p.hr = 0.1; p.hp = -0.05; },
  read(t, p) { p.aLp = p.aRp = -1.25; p.aLr = p.aRr = -0.15; p.eL = p.eR = 1.6; p.hp = 0.35; p.hr = S(t * 0.8) * 0.1; },
  garden(t, p) { const w = t * 5; p.bl = 0.9; p.kL = p.kR = 0.8; p.lLp = p.lRp = -0.6; p.by = -0.1; p.aRp = -0.8 + S(w) * 0.4; p.aLp = -0.6; p.eR = 0.4; p.hp = 0.3; },
  fish(t, p) { p.aLp = p.aRp = -1.25; p.eL = p.eR = 0.4; p.aLr = p.aRr = -0.1; p.hp = 0.15; p.by = S(t * 1.2) * 0.004; if (S(t * 0.9) > 0.97) { p.aLp = p.aRp = -1.9; p.face = 'surprise'; } },
  lift(t, p) { const k = (S(t * 3) + 1) / 2; p.aLp = p.aRp = -3.0 * k - 0.1; p.aLr = p.aRr = 0.3; p.eL = p.eR = 1.6 * (1 - k); p.kL = p.kR = 0.4 * (1 - k); p.by = -0.04 * (1 - k); p.face = k > 0.8 ? 'anger' : null; },
  machine(t, p) { const w = t * 10; p.aLp = -1.1 + S(w) * 0.12; p.aRp = -1.1 - S(w * 1.3) * 0.12; p.eL = p.eR = 0.9; p.hp = 0.1; p.bl = 0.08; },
  cheer(t, p) { const w = t * 8; p.aLp = p.aRp = -2.8; p.aLr = p.aRr = 0.45 + S(w) * 0.2; p.oy = Math.max(0, S(w)) * 0.08; p.kL = p.kR = Math.max(0, -S(w)) * 0.4; p.face = 'joy'; },
  swim(t, p) { const w = t * 5; p.rx = -1.2; p.oy = -0.24; p.aLp = -2.6 + S(w) * 1.4; p.aRp = -2.6 - S(w) * 1.4; p.lLp = S(w * 2) * 0.4; p.lRp = -S(w * 2) * 0.4; p.hp = -0.9; p.face = 'joy'; },
  talk(t, p) { const w = t * 3; p.aRp = -0.6 + S(w) * 0.35; p.eR = 1.0 + S(w * 1.3) * 0.4; p.aLp = -0.2 + S(w * 0.7) * 0.2; p.hp = S(w * 2) * 0.08; p.hr = S(w * 0.5) * 0.15; p.face = 'talk'; },
  argue(t, p) { const w = t * 6; p.aRp = -1.9 + S(w) * 0.6; p.eR = 0.5; p.aLp = -0.3; p.aLr = -0.5; p.eL = 1.8; p.bl = 0.18 + S(w) * 0.06; p.hp = -0.1; p.face = 'anger'; },
  climb(t, p) { const w = t * 6; p.aLp = -2.8 + S(w) * 0.4; p.aRp = -2.8 - S(w) * 0.4; p.eL = p.eR = 0.5; p.lLp = -0.6 + S(w) * 0.5; p.lRp = -0.6 - S(w) * 0.5; p.kL = p.kR = 1.0; },
  piano(t, p) { POSES.sit(t, p); const w = t * 12; p.aLp = -1.3 + S(w) * 0.1; p.aRp = -1.3 + C(w * 1.2) * 0.1; p.eL = p.eR = 0.7; p.hr = S(t * 2) * 0.2; p.face = 'joy'; },
  swing(t, p) { POSES.sit(t, p); const s = S(t * 2.4); p.rx = s * 0.45; p.oy = (1 - C(s * 0.6)) * 0.3; p.aLp = p.aRp = -2.4; p.eL = p.eR = 0.3; p.kL = p.kR = 1.2 - s * 0.6; p.face = 'joy'; },
  slide(t, p) { const k = (t % 3) / 3; if (k < 0.45) { POSES.climb(t, p); p.oy = k / 0.45 * 0.5; } else { POSES.sit(t, p); p.oy = 0.5 * (1 - (k - 0.45) / 0.55); p.aLp = p.aRp = -2.6; p.face = 'joy'; } },
  observe(t, p) { p.aLp = -1.4; p.aRp = -1.5; p.eL = p.eR = 1.7; p.hp = -0.35; p.bl = 0.12; },
  seek(t, p) { p.aLp = p.aRp = -2.1; p.eL = p.eR = 2.3; p.aLr = p.aRr = -0.2; p.hp = 0.15; p.face = 'sleep'; },
  hide(t, p) { p.by = -0.2; p.kL = p.kR = 2.0; p.lLp = p.lRp = -1.6; p.bl = 0.5; p.aLp = p.aRp = -1.0; p.eL = p.eR = 2.0; p.hp = -0.2 + S(t * 2) * 0.05; p.face = 'surprise'; },
  give(t, p) { p.aLp = p.aRp = -1.3; p.eL = p.eR = 0.4; p.bl = 0.1; p.face = 'joy'; },
};
export const ANIM_NAMES = Object.keys(POSES);

const tmpA = base(), tmpB = base();
export function computePose(ch, out) {
  const a = tmpA; Object.assign(a, Z); a.face = null;
  (POSES[ch.anim] || POSES.idle)(ch.animT, a);
  if (ch.blend < 1) {
    const b = tmpB; Object.assign(b, Z); b.face = null;
    (POSES[ch.prevAnim] || POSES.idle)(ch.animT + 0.5, b);
    const k = ch.blend * ch.blend * (3 - 2 * ch.blend);
    for (const key of POSE_KEYS) out[key] = b[key] + (a[key] - b[key]) * k;
    out.face = k > 0.5 ? a.face : b.face;
  } else { for (const key of POSE_KEYS) out[key] = a[key]; out.face = a.face; }
  return out;
}
