// Mode « rétro pixel » : chaque cube est un bloc de plastique brillant avec un écran
// rétroéclairé (cadre noir) sur sa face avant. Les habitants y sont dessinés en
// bonshommes pixelisés, à partir des mêmes poses que la vue 3D.
import * as THREE from 'three';
import { CUBE, HALF, LADDER } from '../sim/constants.js';
import { THEMES, OBJECT_TYPES, OBJECT_KEYS } from '../sim/themes.js';
import { computePose, POSE_KEYS } from './poses.js';
import { propGeometry, cakeGeometry } from './props.js';
import { roundedBox } from './geom.js';
import { dominantEmotion } from '../sim/social.js';

const RES = 80;                 // résolution de l'écran (pixels)
const PX = RES / CUBE;          // pixels par unité monde
const FLOOR_FRONT = RES - 5;    // ligne du sol pour z = +2 (devant)
const FLOOR_BACK = RES - 19;    // ligne du sol pour z = -2 (fond)
const CHAR_SCALE = 1.55;        // bonshommes agrandis pour la lisibilité
const SCREEN = 3.22;            // taille de l'écran (unités)
const BEZEL = 3.6;

// Couleurs de rétroéclairage (fond, encre) — une par cube selon son thème.
const BACKLIGHTS = [
  ['#c9e7c2', '#16301c'], ['#f4d79a', '#3a2406'], ['#bfe0f2', '#0f2738'], ['#dccdf3', '#251843'],
  ['#f6c7c0', '#3d1410'], ['#d8e8a6', '#26300a'], ['#c3ece6', '#0d302b'], ['#ead9c4', '#33240f'],
];
// Couleurs du plastique.
const PLASTIC = ['#ff3f8e', '#21b8e6', '#ff7a1a', '#8a2be2', '#2fcf6a', '#ffcc00', '#e8364f', '#1e6fff', '#a3176b', '#00b3a4', '#ff5ec4', '#6b4cff', '#ff9f1c', '#3a3f58', '#7bdc4c', '#00a2ff'];

function hash(s) { let h = 2166136261; for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }

// Petits glyphes 5x5 pour les bulles.
const GLYPHS = {
  heart: ['01010', '11111', '11111', '01110', '00100'], note: ['00110', '00101', '00100', '11100', '11100'],
  z: ['11111', '00010', '00100', '01000', '11111'], bang: ['00100', '00100', '00100', '00000', '00100'],
  q: ['01110', '10001', '00110', '00000', '00100'], star: ['00100', '11111', '01110', '01010', '10001'],
  angry: ['10001', '01010', '00000', '01110', '10001'], drop: ['00100', '01110', '11111', '11111', '01110'],
  smile: ['01010', '01010', '00000', '10001', '01110'], gift: ['01010', '11111', '10101', '10101', '11111'],
  dots: ['00000', '00000', '00000', '00000', '10101'],
};
const BUBBLE_GLYPH = { '❤️': 'heart', '🤗': 'heart', '😍': 'heart', '🎵': 'note', '💤': 'z', '❓': 'q', '🤔': 'q', '✨': 'star', '🏆': 'star', '🎉': 'star', '💢': 'angry', '😤': 'angry', '😢': 'drop', '😕': 'drop', '😂': 'smile', '😄': 'smile', '😆': 'smile', '😊': 'smile', '😋': 'smile', '🎁': 'gift', '🎂': 'gift', '💥': 'bang', '😱': 'bang', '😵': 'bang', '🤕': 'bang', '👀': 'bang' };

// Rendu des silhouettes d'objets en sprites 2 tons, à partir des modèles 3D.
function bakeSprites(renderer) {
  const sprites = {};
  const scene = new THREE.Scene();
  const mat = new THREE.MeshBasicMaterial({ vertexColors: true });
  const mesh = new THREE.Mesh(undefined, mat); scene.add(mesh);
  const cam = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.01, 20);
  const prevTarget = renderer.getRenderTarget();
  const prevClear = renderer.getClearColor(new THREE.Color()); const prevAlpha = renderer.getClearAlpha();
  renderer.setClearColor(0x000000, 0);
  const bake = (key, geo, side) => {
    geo.computeBoundingBox(); const bb = geo.boundingBox;
    const w = side ? bb.max.z - bb.min.z : bb.max.x - bb.min.x, h = bb.max.y - bb.min.y;
    const pw = Math.max(2, Math.round(w * PX)), ph = Math.max(2, Math.round(h * PX));
    mesh.geometry = geo;
    const cx = side ? (bb.max.z + bb.min.z) / 2 : (bb.max.x + bb.min.x) / 2;
    cam.left = -w / 2; cam.right = w / 2; cam.top = h; cam.bottom = 0; cam.updateProjectionMatrix();
    if (side) { cam.position.set(5, 0, cx); cam.lookAt(0, 0, cx); cam.position.y = bb.min.y; cam.lookAt(0, bb.min.y, cx); }
    else { cam.position.set(cx, bb.min.y, 5); cam.lookAt(cx, bb.min.y, 0); }
    const rt = new THREE.WebGLRenderTarget(pw, ph);
    renderer.setRenderTarget(rt); renderer.clear(); renderer.render(scene, cam);
    const buf = new Uint8Array(pw * ph * 4); renderer.readRenderTargetPixels(rt, 0, 0, pw, ph, buf);
    rt.dispose();
    const px = new Uint8Array(pw * ph);
    for (let y = 0; y < ph; y++) for (let x = 0; x < pw; x++) {
      const i = ((ph - 1 - y) * pw + x) * 4;
      if (buf[i + 3] < 100) continue;
      const lum = (buf[i] * 0.3 + buf[i + 1] * 0.55 + buf[i + 2] * 0.15) / 255;
      px[y * pw + x] = lum < 0.32 ? 2 : 3;
    }
    // contour foncé pour bien lire la forme
    const out = px.map((v) => (v === 3 ? 0 : v));
    for (let y = 0; y < ph; y++) for (let x = 0; x < pw; x++) {
      if (!px[y * pw + x]) continue;
      if (x === 0 || y === 0 || x === pw - 1 || y === ph - 1 || !px[y * pw + x - 1] || !px[y * pw + x + 1] || !px[(y - 1) * pw + x] || !px[(y + 1) * pw + x]) out[y * pw + x] = 1;
    }
    sprites[key] = { w: pw, h: ph, px: out, ox: Math.round(-cx * PX + pw / 2) };
  };
  for (const k of OBJECT_KEYS) { const g = propGeometry(k); bake(k, g, false); bake(k + ':side', g, true); }
  bake('cake', cakeGeometry(), false);
  renderer.setRenderTarget(prevTarget); renderer.setClearColor(prevClear, prevAlpha);
  return sprites;
}

class Screen {
  constructor(group) {
    this.canvas = document.createElement('canvas'); this.canvas.width = this.canvas.height = RES;
    this.ctx = this.canvas.getContext('2d');
    this.img = this.ctx.createImageData(RES, RES);
    this.ink = new Uint8Array(RES * RES);   // 0 vide, 1 encre, 2 demi-teinte
    this.tex = new THREE.CanvasTexture(this.canvas);
    this.tex.magFilter = THREE.NearestFilter; this.tex.minFilter = THREE.LinearFilter; this.tex.colorSpace = THREE.SRGBColorSpace;
    this.mesh = new THREE.Mesh(new THREE.PlaneGeometry(SCREEN, SCREEN), new THREE.MeshBasicMaterial({ map: this.tex, toneMapped: false }));
    this.mesh.renderOrder = 1;
    group.add(this.mesh);
    this.cubeId = null;
  }
}

export class PixelRenderer {
  constructor(scene, glRenderer, gradientMap) {
    this.scene = scene; this.gl = glRenderer;
    this.group = new THREE.Group(); scene.add(this.group);
    this.sprites = bakeSprites(glRenderer);
    const shellMat = new THREE.MeshPhysicalMaterial({ color: 0xffffff, roughness: 0.28, metalness: 0, clearcoat: 1, clearcoatRoughness: 0.15 });
    this.shells = this.inst(roundedBox(CUBE - 0.05, CUBE - 0.05, CUBE - 0.05, 0.32), shellMat, true);
    const bezelGeo = roundedBox(BEZEL, BEZEL, 0.12, 0.05);
    this.bezels = this.inst(bezelGeo, new THREE.MeshStandardMaterial({ color: 0x101114, roughness: 0.45, metalness: 0.1 }), false);
    this.blank = this.inst(new THREE.PlaneGeometry(SCREEN, SCREEN), new THREE.MeshBasicMaterial({ color: 0xffffff, toneMapped: false }), false);
    // reflet de vitre au-dessus de chaque écran
    const g = document.createElement('canvas'); g.width = g.height = 128; const c = g.getContext('2d');
    const grd = c.createLinearGradient(0, 0, 128, 128); grd.addColorStop(0, 'rgba(255,255,255,0.32)'); grd.addColorStop(0.35, 'rgba(255,255,255,0.06)'); grd.addColorStop(0.5, 'rgba(255,255,255,0)'); grd.addColorStop(1, 'rgba(255,255,255,0.04)');
    c.fillStyle = grd; c.fillRect(0, 0, 128, 128);
    const glassTex = new THREE.CanvasTexture(g);
    this.glass = this.inst(new THREE.PlaneGeometry(SCREEN, SCREEN), new THREE.MeshBasicMaterial({ map: glassTex, transparent: true, depthWrite: false }), false);
    this.glass.renderOrder = 2;
    // pastilles de connexion sur les côtés
    this.pads = this.inst(roundedBox(0.9, 0.9, 0.04, 0.02), new THREE.MeshStandardMaterial({ color: 0xd9dde5, roughness: 0.3, metalness: 0.6 }), false);
    this.screens = [];
    this.maxScreens = 48;
    this.key = '';
    this.drawTimer = 0;
    this.visible = [];
    this.m = new THREE.Matrix4(); this.q = new THREE.Quaternion(); this.v = new THREE.Vector3(); this.s = new THREE.Vector3(1, 1, 1); this.col = new THREE.Color();
    this.pose = {}; for (const k of POSE_KEYS) this.pose[k] = 0;
    this.J = {}; for (const n of ['root', 'pel', 'tor', 'hd', 'a', 't']) this.J[n] = new THREE.Matrix4();
    this.pv = new THREE.Vector3();
    this.cubeList = [];
  }

  inst(geo, mat, shadow) {
    const o = { geo, mat, mesh: null, cap: 0, shadow };
    return o;
  }
  begin(o, n, color = true) {
    if (!o.mesh || n > o.cap) {
      if (o.mesh) { this.group.remove(o.mesh); o.mesh.dispose(); }
      o.cap = Math.max(16, Math.ceil(n * 1.5));
      o.mesh = new THREE.InstancedMesh(o.geo, o.mat, o.cap);
      o.mesh.castShadow = o.shadow; o.mesh.receiveShadow = o.shadow;
      if (color) o.mesh.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(o.cap * 3).fill(1), 3);
      if (o.renderOrder) o.mesh.renderOrder = o.renderOrder;
      this.group.add(o.mesh);
    }
    o.n = 0;
  }
  add(o, m, c) { o.mesh.setMatrixAt(o.n, m); if (c && o.mesh.instanceColor) o.mesh.setColorAt(o.n, c); o.n++; }
  end(o) { o.mesh.count = o.n; o.mesh.instanceMatrix.needsUpdate = true; if (o.mesh.instanceColor) o.mesh.instanceColor.needsUpdate = true; o.mesh.computeBoundingSphere(); }

  setVisible(v) { this.group.visible = v; }

  static colors(cube) {
    const h = hash(cube.id + cube.theme);
    const plastic = cube.tint || PLASTIC[h % PLASTIC.length];
    const bl = BACKLIGHTS[(h >>> 5) % BACKLIGHTS.length];
    return { plastic, bg: bl[0], ink: bl[1] };
  }

  // Construit coques, cadres et répartit les écrans dynamiques sur les cubes proches.
  build(world, focus, opts) {
    const cubes = [...world.cubes.values()].filter((c) => c.y <= opts.cutY);
    const withD = cubes.map((c) => [((c.x * CUBE - focus.x) ** 2 + (c.y * CUBE - focus.y) ** 2 + (c.z * CUBE - focus.z) ** 2), c]).sort((a, b) => a[0] - b[0]);
    this.cubeList = withD.map((x) => x[1]);
    const live = this.cubeList.slice(0, this.maxScreens);
    const liveSet = new Set(live.map((c) => c.id));
    const N = cubes.length;
    const { m, q, v, s, col } = this;
    this.glass.renderOrder = 2;
    this.begin(this.shells, N); this.begin(this.bezels, N, false); this.begin(this.blank, N); this.begin(this.glass, N, false); this.begin(this.pads, N * 6, false);
    q.identity();
    for (const c of cubes) {
      const k = PixelRenderer.colors(c);
      const ox = c.x * CUBE, oy = c.y * CUBE + HALF, oz = c.z * CUBE;
      m.compose(v.set(ox, oy, oz), q, s); this.add(this.shells, m, col.set(k.plastic));
      m.compose(v.set(ox, oy + 0.05, oz + HALF - 0.04), q, s); this.add(this.bezels, m);
      m.compose(v.set(ox, oy + 0.05, oz + HALF + 0.035), q, s); this.add(this.glass, m);
      if (!liveSet.has(c.id)) { m.compose(v.set(ox, oy + 0.05, oz + HALF + 0.03), q, s); this.add(this.blank, m, col.set(k.bg).multiplyScalar(0.92)); }
      for (const side of ['px', 'nx', 'py', 'ny']) {
        if (!world.neighbor(c, side)) continue;
        const e = new THREE.Euler(side[1] === 'y' ? Math.PI / 2 : 0, side[1] === 'x' ? Math.PI / 2 : 0, 0);
        const sg = side[0] === 'p' ? 1 : -1;
        const pv = side[1] === 'x' ? [ox + sg * (HALF - 0.015), oy, oz] : [ox, oy + sg * (HALF - 0.015), oz];
        m.compose(v.set(...pv), new THREE.Quaternion().setFromEuler(e), s); this.add(this.pads, m);
      }
    }
    for (const o of [this.shells, this.bezels, this.blank, this.glass, this.pads]) this.end(o);
    // écrans dynamiques
    while (this.screens.length < live.length) this.screens.push(new Screen(this.group));
    this.screens.forEach((sc, i) => {
      const c = live[i];
      sc.mesh.visible = !!c; sc.cubeId = c ? c.id : null;
      if (c) sc.mesh.position.set(c.x * CUBE, c.y * CUBE + HALF + 0.05, c.z * CUBE + HALF + 0.032);
    });
    this.drawTimer = 1;
  }

  update(world, focus, dt, state, opts) {
    const key = `${world.version}|${Math.round(focus.x / CUBE)},${Math.round(focus.y / CUBE)},${Math.round(focus.z / CUBE)}|${opts.cutY}`;
    if (key !== this.key || state.forceRebuild) { this.key = key; this.build(world, focus, opts); }
    // positions écran des habitants (pour la sélection tactile)
    this.visible.length = 0;
    for (const sc of this.screens) {
      if (!sc.cubeId) continue; const c = world.cubes.get(sc.cubeId); if (!c) continue;
      for (const id of c.chars) {
        const ch = world.chars.get(id); if (!ch) continue;
        if (!c.awake) { ch.animT += dt; }
        const [sx, sy] = this.toScreen(ch.x, ch.z, ch.y);
        this.visible.push({ ch, wx: c.x * CUBE + (sx / RES - 0.5) * SCREEN, wy: c.y * CUBE + HALF + 0.05 + (0.5 - (sy - 8) / RES) * SCREEN + 0.28, wz: c.z * CUBE + HALF + 0.04 });
      }
    }
    this.drawTimer += dt;
    if (this.drawTimer < 1 / 12) return;
    this.drawTimer = 0;
    for (const sc of this.screens) if (sc.cubeId) this.drawScreen(world, sc, state, opts);
  }

  toScreen(x, z, y = 0) {
    const floor = FLOOR_BACK + ((z + HALF) / CUBE) * (FLOOR_FRONT - FLOOR_BACK);
    return [(x + HALF) * PX, floor - y * ((FLOOR_BACK - 2) / CUBE)];
  }

  drawScreen(world, sc, state, opts) {
    const c = world.cubes.get(sc.cubeId); if (!c) return;
    const ink = sc.ink; ink.fill(0);
    const set = (x, y, v = 1) => { x |= 0; y |= 0; if (x >= 0 && y >= 0 && x < RES && y < RES && (v !== 2 || !ink[y * RES + x])) ink[y * RES + x] = v; };
    const line = (x0, y0, x1, y1, v = 1) => {
      x0 = Math.round(x0); y0 = Math.round(y0); x1 = Math.round(x1); y1 = Math.round(y1);
      const dx = Math.abs(x1 - x0), dy = -Math.abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1; let err = dx + dy;
      for (let i = 0; i < 200; i++) { set(x0, y0, v); if (x0 === x1 && y0 === y1) break; const e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
    };
    const blit = (sp, x, y, flip) => {
      const x0 = Math.round(x - sp.w / 2), y0 = Math.round(y - sp.h);
      for (let j = 0; j < sp.h; j++) for (let i = 0; i < sp.w; i++) { const p = sp.px[j * sp.w + (flip ? sp.w - 1 - i : i)]; if (p) set(x0 + i, y0 + j, p); }
    };
    // décor : sol, murs fermés, échelle, trappe
    for (let x = 0; x < RES; x += 2) set(x, FLOOR_BACK - 1, 2);
    for (let x = 0; x < RES; x++) set(x, FLOOR_FRONT + 2, 1);
    if (!world.neighbor(c, 'nx')) for (let y = 2; y < FLOOR_FRONT + 2; y++) set(1, y, 1);
    if (!world.neighbor(c, 'px')) for (let y = 2; y < FLOOR_FRONT + 2; y++) set(RES - 2, y, 1);
    if (!world.neighbor(c, 'py')) for (let x = 1; x < RES - 1; x++) set(x, 1, 1);
    const [lx] = this.toScreen(LADDER.x, LADDER.z);
    if (world.neighbor(c, 'py')) { const [, ly] = this.toScreen(LADDER.x, LADDER.z); for (let y = 0; y <= ly; y++) { set(lx - 3, y, 1); set(lx + 3, y, 1); if (y % 4 === 0) line(lx - 3, y, lx + 3, y, 1); } }
    if (world.neighbor(c, 'ny')) { const [, ly] = this.toScreen(LADDER.x, LADDER.z); line(lx - 4, ly + 1, lx + 4, ly + 1, 2); }
    // objets et habitants triés par profondeur
    const items = [];
    for (const o of c.objects) items.push([o.z, 0, o]);
    for (const id of c.chars) { const ch = world.chars.get(id); if (ch) items.push([ch.z, 1, ch]); }
    items.sort((a, b) => a[0] - b[0]);
    for (const [, kind, it] of items) {
      if (kind === 0) {
        const side = Math.abs(Math.sin(it.r)) > 0.7;
        const sp = this.sprites[side ? it.type + ':side' : it.type] || this.sprites[it.type];
        const [x, y] = this.toScreen(it.x, it.z);
        blit(sp, x, y + 1, side ? Math.sin(it.r) < 0 : Math.cos(it.r) < 0 && false);
        if (it.state.dirty) { set(x - 2, y - 8, 1); set(x + 1, y - 9, 1); set(x + 3, y - 8, 1); }
      } else this.drawChar(world, it, set, line, state);
    }
    // effets
    for (const f of world.fx) {
      if (f.cube !== c.id) continue; const k = f.t / f.dur;
      if (f.type === 'ball') { const x = f.from[0] + (f.to[0] - f.from[0]) * k, z = f.from[1] + (f.to[1] - f.from[1]) * k; const [sx, sy] = this.toScreen(x, z, 0.5 + Math.sin(k * Math.PI) * 0.5); set(sx, sy, 1); set(sx + 1, sy, 1); set(sx, sy + 1, 1); set(sx + 1, sy + 1, 1); }
      if (f.type === 'confetti') for (let i = 0; i < 24; i++) { const x = (Math.sin(i * 12.9 + f.x) * 0.5 + 0.5) * RES; const y = ((i * 7.3 + k * 70) % 60) + 3; set(x, y, i % 3 ? 2 : 1); }
      if (f.type === 'dust' || f.type === 'smoke') { const [sx, sy] = this.toScreen(f.x, f.z, 0.3); for (let i = 0; i < 26; i++) { const a = i * 2.4 + performance.now() / 90; const r = 4 + (i % 4) + (f.type === 'smoke' ? k * 6 : 0); set(sx + Math.cos(a) * r, sy - 4 + Math.sin(a) * r * 0.7 - (f.type === 'smoke' ? k * 10 : 0), i % 2 ? 1 : 2); } }
      if (f.type === 'cake') { const [sx, sy] = this.toScreen(f.x, f.z); blit(this.sprites.cake, sx, sy); }
    }
    // composition : fond rétroéclairé, ombre portée de l'encre (effet LCD), encre
    const k = PixelRenderer.colors(c);
    const bg = hexRgb(k.bg), fg = hexRgb(k.ink);
    const dim = world.isNight ? 0.82 : 1;
    const d = sc.img.data;
    for (let y = 0; y < RES; y++) for (let x = 0; x < RES; x++) {
      const i = y * RES + x; const o = i * 4;
      let r = bg[0] * dim, g = bg[1] * dim, b = bg[2] * dim;
      // trame des pixels et léger vignettage
      if ((x & 1) && (y & 1)) { r *= 0.985; g *= 0.985; b *= 0.985; }
      const vg = 1 - (((x - RES / 2) ** 2 + (y - RES / 2) ** 2) / (RES * RES)) * 0.25; r *= vg; g *= vg; b *= vg;
      const sh = x > 0 && y > 0 ? ink[(y - 1) * RES + x - 1] : 0;
      if (sh) { r = r * 0.86; g = g * 0.86; b = b * 0.86; }
      const v = ink[i];
      if (v === 1) { r = fg[0]; g = fg[1]; b = fg[2]; } else if (v === 2) { r = (r + fg[0]) / 2; g = (g + fg[1]) / 2; b = (b + fg[2]) / 2; }
      d[o] = r; d[o + 1] = g; d[o + 2] = b; d[o + 3] = 255;
    }
    sc.ctx.putImageData(sc.img, 0, 0);
    sc.tex.needsUpdate = true;
  }

  // Bonhomme pixelisé : projection de face du squelette 3D (mêmes 45 animations).
  drawChar(world, ch, set, line, state) {
    const P = computePose(ch, this.pose); const J = this.J; const T = J.t; const v = this.pv;
    const [fx, fy] = this.toScreen(ch.x, ch.z, ch.y);
    const S = PX * CHAR_SCALE;
    const pt = (mat, x = 0, y = 0, z = 0) => { v.set(x, y, z).applyMatrix4(mat); return [fx + v.x * S, fy - v.y * S]; };
    J.root.makeRotationY(ch.ry + P.ry);
    if (P.oy || P.rx) { J.root.multiply(T.makeTranslation(0, 0.3 + P.oy, 0)); J.root.multiply(T.makeRotationX(P.rx)); J.root.multiply(T.makeTranslation(0, -0.3, 0)); }
    const pel = J.pel.copy(J.root).multiply(T.makeTranslation(0, 0.26 + P.by, 0));
    const tor = J.tor.copy(pel).multiply(T.makeRotationX(P.bl)); if (P.br) tor.multiply(T.makeRotationZ(P.br));
    const neck = pt(tor, 0, 0.2, 0); const hip = pt(pel);
    const hd = J.hd.copy(tor).multiply(T.makeTranslation(0, 0.2, 0)).multiply(T.makeRotationX(P.hp)).multiply(T.makeTranslation(0, 0.085, 0));
    const head = pt(hd);
    line(hip[0], hip[1], neck[0], neck[1]);
    for (const side of [1, -1]) {
      const ap = side > 0 ? P.aLp : P.aRp, ar = side > 0 ? P.aLr : P.aRr, el = side > 0 ? P.eL : P.eR;
      const a = J.a.copy(tor).multiply(T.makeTranslation(0.05 * side, 0.17, 0)).multiply(T.makeRotationX(ap)).multiply(T.makeRotationZ(ar * side));
      const sh = pt(a); const elb = pt(a, 0, -0.11, 0); a.multiply(T.makeTranslation(0, -0.11, 0)).multiply(T.makeRotationX(-el)); const hand = pt(a, 0, -0.11, 0);
      line(sh[0], sh[1], elb[0], elb[1]); line(elb[0], elb[1], hand[0], hand[1]);
      const lp = side > 0 ? P.lLp : P.lRp, kn = side > 0 ? P.kL : P.kR;
      const l = J.a.copy(pel).multiply(T.makeTranslation(0.035 * side, 0, 0)).multiply(T.makeRotationX(lp));
      const h0 = pt(l); const knee = pt(l, 0, -0.13, 0); l.multiply(T.makeTranslation(0, -0.13, 0)).multiply(T.makeRotationX(kn)); const foot = pt(l, 0, -0.13, 0); const toe = pt(l, 0, -0.13, 0.05);
      line(h0[0], h0[1], knee[0], knee[1]); line(knee[0], knee[1], foot[0], foot[1]); line(foot[0], foot[1], toe[0], toe[1]);
    }
    // tête ronde évidée (signature du style) + yeux selon l'orientation
    const hx = Math.round(head[0]), hy = Math.round(head[1]);
    for (const [dx, dy] of CIRCLE) set(hx + dx, hy + dy, 1);
    for (const [dx, dy] of DISC) set(hx + dx, hy + dy, 0);
    const facing = Math.cos(ch.ry); const side = Math.sin(ch.ry);
    const sleep = ch.anim === 'sleep' || P.face === 'sleep';
    if (P.rx === 0 || Math.abs(P.rx) < 0.5) {
      if (sleep) { set(hx - 1, hy, 1); set(hx + 1, hy, 1); }
      else if (facing > 0.3) { set(hx - 1, hy - 1, 1); set(hx + 1, hy - 1, 1); const e = dominantEmotion(ch); if (e === 'joy') { set(hx - 1, hy + 1, 1); set(hx, hy + 2, 1); set(hx + 1, hy + 1, 1); } else if (e === 'sadness' || e === 'anger') { set(hx - 1, hy + 2, 1); set(hx, hy + 1, 1); set(hx + 1, hy + 2, 1); } }
      else if (facing > -0.3) { set(hx + (side > 0 ? 1 : -1), hy - 1, 1); }
    }
    // bulle
    if (ch.bubble && state.bubbles) {
      const g = GLYPHS[BUBBLE_GLYPH[ch.bubble.icon] || 'dots'];
      const bx = hx - 3, by = hy - 13;
      for (let y = -1; y <= 5; y++) for (let x = -1; x <= 5; x++) { const edge = y === -1 || y === 5 || x === -1 || x === 5; set(bx + x, by + y, edge ? 1 : 0); }
      set(bx + 2, by + 6, 1);
      for (let y = 0; y < 5; y++) for (let x = 0; x < 5; x++) if (g[y][x] === '1') set(bx + x, by + y, 1);
    }
    if (state.selectedChar === ch.id && Math.floor(performance.now() / 300) % 2) { line(hx - 1, hy - 9, hx + 1, hy - 9); set(hx, hy - 8, 1); }
  }
}

const CIRCLE = [], DISC = [];
for (let y = -3; y <= 3; y++) for (let x = -3; x <= 3; x++) { const d = Math.sqrt(x * x + y * y); if (d <= 3.4 && d > 2.3) CIRCLE.push([x, y]); }
for (let y = -2; y <= 2; y++) for (let x = -2; x <= 2; x++) if (x * x + y * y <= 4.5) DISC.push([x, y]);
function hexRgb(h) { const n = parseInt(h.slice(1), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255]; }
