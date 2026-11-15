// Construction du circuit et du décor (ville néon, route mouillée réfléchissante).
import * as THREE from 'three';
import { Reflector } from 'three/addons/objects/Reflector.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

export const HALF = 11.0;  // demi-largeur de la route (vibreurs inclus) : 4 voies
export const LIMIT = 9.7;   // décalage latéral maximum du centre d'une voiture

let seed = 1;
const rnd = () => { seed = (seed * 16807) % 2147483647; return (seed - 1) / 2147483646; };
const rr = (a, b) => a + (b - a) * rnd();
const pick = (arr) => arr[Math.floor(rnd() * arr.length)];
const hex = (c) => '#' + new THREE.Color(c).getHexString();

function canvasTex(w, h, draw, opts = {}) {
  const c = document.createElement('canvas'); c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.anisotropy = 4;
  Object.assign(t, opts);
  return t;
}
function box(w, h, d, x, y, z, rotY = 0) {
  const g = new THREE.BoxGeometry(w, h, d);
  if (rotY) g.rotateY(rotY);
  g.translate(x, y, z);
  return g;
}

// ---------- échantillonnage du tracé ----------
export function makeTrack(def) {
  const sc = def.echelle || 1.6; // les tracés sont dessinés petits puis agrandis
  const curve = new THREE.CatmullRomCurve3(def.pts.map(p => new THREE.Vector3(p[0] * sc, 0, p[1] * sc)), true, 'centripetal');
  const L = curve.getLength(), N = Math.round(L / 2), ds = L / N;
  const P = curve.getSpacedPoints(N);
  const px = new Float32Array(N), pz = new Float32Array(N), tx = new Float32Array(N), tz = new Float32Array(N), k = new Float32Array(N);
  for (let i = 0; i < N; i++) { px[i] = P[i].x; pz[i] = P[i].z; }
  for (let i = 0; i < N; i++) {
    const a = (i + N - 1) % N, b = (i + 1) % N;
    const dx = px[b] - px[a], dz = pz[b] - pz[a], l = Math.hypot(dx, dz);
    tx[i] = dx / l; tz[i] = dz / l;
  }
  const raw = new Float32Array(N);
  for (let i = 0; i < N; i++) {
    const a = (i + N - 1) % N, b = (i + 1) % N;
    // > 0 : virage à droite
    raw[i] = ((tx[b] - tx[a]) * (-tz[i]) + (tz[b] - tz[a]) * tx[i]) / (2 * ds);
  }
  for (let i = 0; i < N; i++) { let s = 0; for (let j = -3; j <= 3; j++) s += raw[(i + j + N) % N]; k[i] = s / 7; }
  const T = { def, N, L, ds, px, pz, tx, tz, k };
  // indice de l'échantillon le plus proche, en cherchant autour d'un indice connu
  T.nearest = (x, z, hint, span = 14) => {
    let best = hint, bd = 1e18;
    for (let j = -span; j <= span; j++) {
      const i = (hint + j + N * 4) % N, dx = x - px[i], dz = z - pz[i], d = dx * dx + dz * dz;
      if (d < bd) { bd = d; best = i; }
    }
    return best;
  };
  T.lateral = (x, z, i) => (x - px[i]) * (-tz[i]) + (z - pz[i]) * tx[i];
  T.point = (i, off, out) => { i = ((i % N) + N) % N; out.x = px[i] - tz[i] * off; out.z = pz[i] + tx[i] * off; return out; };
  T.heading = (i) => { i = ((i % N) + N) % N; return Math.atan2(-tx[i], -tz[i]); };
  return T;
}

// bande le long du tracé : fn(j, s) -> [ax,ay,az, bx,by,bz, ua,va, ub,vb]
function strip(T, fn, step = 1) {
  const pos = [], uv = [], idx = [];
  let n = 0;
  for (let i = 0; i <= T.N; i += step) {
    const ii = Math.min(i, T.N);
    const v = fn(ii % T.N, ii * T.ds);
    pos.push(v[0], v[1], v[2], v[3], v[4], v[5]); uv.push(v[6], v[7], v[8], v[9]);
    if (n > 0) { const q = (n - 1) * 2; idx.push(q, q + 1, q + 2, q + 1, q + 3, q + 2); }
    n++;
    if (ii === T.N) break;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  g.setIndex(idx); g.computeVertexNormals();
  return g;
}
const wall = (T, off, y0, y1, uScale, v0 = 0, v1 = 1) => strip(T, (j, s) => {
  const x = T.px[j] - T.tz[j] * off, z = T.pz[j] + T.tx[j] * off;
  return [x, y0, z, x, y1, z, s / uScale, v0, s / uScale, v1];
});
const flat = (T, o0, o1, y, uScale) => strip(T, (j, s) => [
  T.px[j] - T.tz[j] * o0, y, T.pz[j] + T.tx[j] * o0, T.px[j] - T.tz[j] * o1, y, T.pz[j] + T.tx[j] * o1, s / uScale, 0, s / uScale, 1]);

const ROAD_VERT = `uniform mat4 textureMatrix; varying vec4 vUv; varying vec3 vW; varying vec2 vT;
  void main(){ vUv = textureMatrix * vec4(position,1.); vec4 w = modelMatrix*vec4(position,1.); vW = w.xyz; vT = uv; gl_Position = projectionMatrix*viewMatrix*w; }`;
const ROAD_FRAG = `uniform sampler2D tDiffuse; uniform vec3 fogColor; uniform float fogDensity; uniform float half_; uniform vec3 tint; uniform vec3 curbCol;
  varying vec4 vUv; varying vec3 vW; varying vec2 vT;
  float hash(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
  float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
  float fbm(vec2 p){ float a=.5, s=0.; for(int i=0;i<3;i++){ s+=a*noise(p); p*=2.03; a*=.5; } return s; }
  void main(){
    vec3 toCam = cameraPosition - vW; float dist = length(toCam); vec3 V = toCam/dist;
    vec2 p = vW.xz; float ax = abs(vT.x); float s = vT.y;
    float pud = smoothstep(0.42, 0.62, fbm(p*0.16 + 7.3));
    float grain = noise(p*38.0);
    float micro = fbm(p*vec2(5.0,2.2));
    float lane = max(1. - smoothstep(0.055, 0.08, abs(ax - 4.8)), 1. - smoothstep(0.055, 0.08, ax)) * step(0.45, fract(s/9.0));
    float edge = 1. - smoothstep(0.08, 0.11, abs(ax - (half_-0.95)));
    float curb = step(half_-0.6, ax) * step(ax, half_);
    float cs = step(0.5, fract(s/3.2));
    float startBand = step(abs(s - 3.0), 0.9) * step(ax, half_-1.0);
    float chk = mod(floor(vT.x/0.6) + floor(s/0.6), 2.0);
    float paint = max(lane, edge) * (0.75 + 0.25*grain);
    vec3 albedo = mix(vec3(0.030,0.032,0.042), vec3(0.014,0.015,0.02), micro) + 0.012*grain;
    albedo = mix(albedo, vec3(0.42,0.42,0.46), paint*0.9);
    albedo = mix(albedo, mix(vec3(0.02), vec3(0.5), chk), startBand);
    albedo = mix(albedo, mix(curbCol, vec3(0.45), cs), curb);
    float off = step(half_, ax);
    albedo = mix(albedo, vec3(0.02,0.02,0.03), off);
    float rough = mix(0.55, 0.06, pud); rough = mix(rough, 0.5, max(max(paint, startBand)*0.6, curb*0.7)); rough = mix(rough, 0.8, off);
    vec2 uv = vUv.xy / vUv.w;
    vec2 nrm = vec2(noise(p*9.0+1.7)-.5, noise(p*9.0+8.1)-.5) + 0.6*vec2(noise(p*31.)-.5, noise(p*31.+4.)-.5);
    float att = 1.0/(1.0+dist*0.035);
    uv += nrm * (0.012 + 0.05*rough) * att;
    float spread = (0.010 + 0.085*rough) * (0.35 + 0.65*att);
    vec3 refl = vec3(0.); float wsum = 0.;
    for(int i=0;i<TAPS;i++){ float t = (float(i)-float(TAPS/2))/float(TAPS/2); float w = 1.-0.75*abs(t);
      refl += texture2D(tDiffuse, uv + vec2(0.35*nrm.x*t*spread, t*spread)).rgb * w; wsum += w; }
    refl /= wsum;
    float fres = 0.05 + 0.95*pow(1.0 - clamp(V.y,0.,1.), 4.0);
    float strength = mix(0.55, 1.0, pud) * mix(1.0, 0.45, rough);
    vec3 col = albedo * tint * 1.6 + refl * fres * strength;
    float f = 1.0 - exp(-fogDensity*fogDensity*dist*dist);
    col = mix(col, fogColor, f);
    gl_FragColor = vec4(col, 1.);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }`;

function windowTex(palette, lit) {
  // 16 colonnes x 32 étages : 1,5 m x 3,5 m par cellule
  return canvasTex(512, 1024, (g, w, h) => {
    g.fillStyle = '#000'; g.fillRect(0, 0, w, h);
    const cw = w / 16, ch = h / 32;
    for (let y = 0; y < 32; y++) {
      const floorLit = rnd() < 0.18 ? 0.95 : (rnd() < 0.15 ? 0.08 : lit);
      const floorCol = pick(palette);
      for (let x = 0; x < 16; x++) {
        const X = x * cw + 3, Y = y * ch + 5, WW = cw - 6, HH = ch - 11;
        if (rnd() > floorLit) { g.globalAlpha = 0.10; g.fillStyle = '#5a6aa0'; g.fillRect(X, Y, WW, HH); continue; }
        g.globalAlpha = rr(0.35, 1);
        g.fillStyle = rnd() < 0.7 ? floorCol : pick(palette);
        g.fillRect(X, Y, WW, HH);
        if (rnd() < 0.3) { g.globalAlpha = 0.55; g.fillStyle = '#000'; g.fillRect(X, Y, WW, HH * rr(0.25, 0.6)); } // store
        if (rnd() < 0.25) { g.globalAlpha = 0.35; g.fillStyle = '#000'; g.fillRect(X + WW * rr(0.2, 0.6), Y + HH * 0.45, WW * 0.3, HH * 0.55); } // silhouette / meuble
      }
    }
  });
}

export function buildWorld(def, renderer, q) {
  seed = 1337 + def.id.length * 7919 + def.pts.length * 104729;
  const th = def.theme;
  const T = makeTrack(def);
  const group = new THREE.Group();
  const disposables = [];
  const add = (geo, mat, layer1) => { const m = new THREE.Mesh(geo, mat); if (layer1) m.layers.set(1); group.add(m); disposables.push(geo, mat); return m; };
  const FOG = new THREE.Color(th.fog);
  const fog = new THREE.FogExp2(FOG, th.dens);

  // --- ciel ---
  const v3 = (a) => `vec3(${a.map(x => x.toFixed(3)).join(',')})`;
  const sky = add(new THREE.SphereGeometry(1800, 24, 12), new THREE.ShaderMaterial({
    side: THREE.BackSide, fog: false, depthWrite: false,
    vertexShader: 'varying vec3 vP; void main(){ vP = position; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.); }',
    fragmentShader: `varying vec3 vP;
      const vec3 HOR = ${v3(th.sky[2])}; const vec3 MID = ${v3(th.sky[1])};
      float h21(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
      float vn(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(h21(i),h21(i+vec2(1,0)),f.x), mix(h21(i+vec2(0,1)),h21(i+vec2(1,1)),f.x), f.y); }
      float fb(vec2 p){ float a=.5, s=0.; for(int i=0;i<4;i++){ s+=a*vn(p); p*=2.1; a*=.5; } return s; }
      void main(){ vec3 d = normalize(vP); float h = d.y;
        vec3 c = mix(${v3(th.sky[2])}, ${v3(th.sky[1])}, smoothstep(0.0,0.16,h)); c = mix(c, ${v3(th.sky[0])}, smoothstep(0.1,0.6,h));
        vec2 sp = floor(d.xz/max(0.2,d.y+0.3)*90.); float st = step(0.9965, h21(sp)) * smoothstep(0.25,0.6,h);
        float cl = smoothstep(0.42, 0.78, fb(d.xz/(max(d.y,0.0)+0.28)*1.5)) * smoothstep(0.03, 0.3, h);
        c += st*vec3(0.9,0.95,1.0)*0.8*(1.-cl);
        c = mix(c, HOR*0.5 + MID*0.35, cl*0.6);
        gl_FragColor = vec4(c,1.);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }`,
  }));
  sky.frustumCulled = false;

  const NEON = { a: new THREE.Color(th.a).multiplyScalar(3.2), b: new THREE.Color(th.b).multiplyScalar(3.0), c: new THREE.Color(th.c).multiplyScalar(3.4), w: new THREE.Color(th.lamp).multiplyScalar(3.0) };
  const neonGeos = { a: [], b: [], c: [], w: [] };

  // --- immeubles ---
  const winTex = th.win.map((p, i) => windowTex(p, [0.6, 0.55, 0.5, 0.7][i]));
  const bMats = winTex.map(t => new THREE.MeshStandardMaterial({ color: 0x0d0e1c, roughness: 0.22, metalness: 0.85, emissive: 0xffffff, emissiveMap: t, emissiveIntensity: 1.25, envMapIntensity: 0.9 }));
  disposables.push(...winTex);
  const bGeos = [[], [], [], []];
  const placed = [];
  const adQuads = [], shopGeos = [];
  function tower(x, z, w, d, h, ang, mi) {
    const g = new THREE.BoxGeometry(w, h, d);
    const uv = g.attributes.uv;
    const ou = Math.floor(rr(0, 16)) / 16, ov = Math.floor(rr(0, 32)) / 32;
    for (let f = 0; f < 6; f++) for (let i = 0; i < 4; i++) {
      const kk = f * 4 + i;
      if (f === 2 || f === 3) { uv.setXY(kk, 0.001, 0.001); continue; }
      const span = (f < 2 ? d : w);
      uv.setXY(kk, Math.round(uv.getX(kk) * span / 1.5) / 16 + ou, Math.round(uv.getY(kk) * h / 3.5) / 32 + ov);
    }
    g.rotateY(ang); g.translate(x, h / 2, z);
    bGeos[mi].push(g);
  }
  function building(x, z, w, d, h, ang, faceSide) {
    const mi = Math.floor(rnd() * 4);
    tower(x, z, w, d, h, ang, mi);
    let top = h;
    if (rnd() < 0.55) { const s = rr(0.55, 0.8), h2 = rr(8, 30); const c = Math.cos(ang), sn = Math.sin(ang); void c; void sn; tower(x, z, w * s, d * s, h + h2, ang, mi); top = h + h2; }
    const ca = Math.cos(ang), sa = Math.sin(ang);
    const corner = (lx, lz) => [x + lx * ca + lz * sa, z - lx * sa + lz * ca];
    if (rnd() < 0.8) {
      const kk = pick(['a', 'b', 'c', 'a', 'b']);
      for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) if (rnd() < 0.6) { const [cx, cz] = corner(sx * w / 2, sz * d / 2); neonGeos[kk].push(box(0.7, h, 0.7, cx, h / 2, cz, ang)); }
      if (rnd() < 0.6) neonGeos[pick(['a', 'b', 'c'])].push(box(w + 0.6, 0.8, d + 0.6, x, h - rr(0.5, 5), z, ang));
      if (rnd() < 0.4) neonGeos[pick(['a', 'b'])].push(box(w + 0.6, 0.6, d + 0.6, x, h * rr(0.3, 0.7), z, ang));
    }
    if (rnd() < 0.35) neonGeos.w.push(box(0.5, rr(8, 22), 0.5, x, top + 6, z));
    if (faceSide) {
      const g = new THREE.BoxGeometry(w + 0.5, 5.2, d + 0.5); const suv = g.attributes.uv; const o = Math.floor(rr(0, 4)) / 4;
      for (let f = 0; f < 6; f++) for (let i = 0; i < 4; i++) { const kk = f * 4 + i; if (f === 2 || f === 3) { suv.setXY(kk, 0.002, 0.002); continue; } suv.setX(kk, Math.round(suv.getX(kk) * (f < 2 ? d : w) / 6) / 4 + o); }
      g.rotateY(ang); g.translate(x, 2.6, z); shopGeos.push(g);
    }
    // panneau publicitaire sur la façade tournée vers la piste
    if (faceSide && rnd() < 0.5 && w > 16) {
      const pw = Math.min(w - 3, rr(11, 18)), ph = pw * 0.5, y = rr(9, Math.max(10, Math.min(24, h - ph)));
      const [cx, cz] = corner(-faceSide * (w / 2 + 0.25), 0);
      adQuads.push({ cx, cz, y, pw, ph, ang: ang + (faceSide > 0 ? -Math.PI / 2 : Math.PI / 2), cell: Math.floor(rnd() * 4) });
    }
  }
  const farFromTrack = (x, z, r) => { for (let i = 0; i < T.N; i += 3) { const dx = x - T.px[i], dz = z - T.pz[i]; if (dx * dx + dz * dz < r * r) return false; } return true; };
  const freeSpot = (x, z, r) => { for (const p of placed) { const dx = x - p[0], dz = z - p[1], rr2 = (r + p[2]) * 0.92; if (dx * dx + dz * dz < rr2 * rr2) return false; } return true; };
  for (let row = 0; row < 2; row++) for (let i = 0; i < T.N; i += 9) for (const side of [-1, 1]) {
    const w = rr(18, 30), d = rr(16, 30), r = Math.hypot(w, d) / 2;
    const off = side * (HALF + 13 + w / 2 + row * 40 + rr(0, 8));
    const j = (i + Math.floor(rr(0, 6))) % T.N;
    const x = T.px[j] - T.tz[j] * off, z = T.pz[j] + T.tx[j] * off;
    if (!farFromTrack(x, z, HALF + 9 + r) || !freeSpot(x, z, r)) continue;
    const h = rr(24, 60) + row * rr(20, 50) + (rnd() < 0.2 ? rr(40, 90) : 0);
    building(x, z, w, d, h, Math.atan2(-T.tx[j], -T.tz[j]) + Math.PI / 2, row === 0 ? side : 0);
    placed.push([x, z, r]);
  }
  // ligne d'horizon lointaine
  let cx = 0, cz = 0, R = 0;
  for (let i = 0; i < T.N; i++) { cx += T.px[i]; cz += T.pz[i]; } cx /= T.N; cz /= T.N;
  for (let i = 0; i < T.N; i++) R = Math.max(R, Math.hypot(T.px[i] - cx, T.pz[i] - cz));
  for (let i = 0; i < 130; i++) {
    const a = rr(0, Math.PI * 2), rad = R * rr(0.0, 1) + rr(110, 430) * (rnd() < 0.5 ? 1 : 0.6);
    const x = cx + Math.cos(a) * rad, z = cz + Math.sin(a) * rad, w = rr(20, 36), d = rr(20, 36), r = Math.hypot(w, d) / 2;
    if (!farFromTrack(x, z, HALF + 60 + r) || !freeSpot(x, z, r)) continue;
    building(x, z, w, d, rr(70, 160) + (rnd() < 0.3 ? rr(60, 130) : 0), rr(0, 3.14), 0);
    placed.push([x, z, r]);
  }
  bGeos.forEach((gs, i) => { if (gs.length) add(mergeGeometries(gs), bMats[i]); });
  if (shopGeos.length) {
    // vitrines : 4 boutiques de 6 m par texture
    const shopTex = canvasTex(1024, 128, (g) => {
      g.fillStyle = '#07070c'; g.fillRect(0, 0, 1024, 128);
      const signs = [hex(th.a), hex(th.b), hex(th.c), '#ffffff'];
      for (let i = 0; i < 4; i++) {
        const X = i * 256, warm = rnd() < 0.5;
        const gr = g.createLinearGradient(0, 40, 0, 128); gr.addColorStop(0, warm ? '#ffd9a0' : '#bfe9ff'); gr.addColorStop(1, warm ? '#7a4a20' : '#20506a');
        g.fillStyle = gr; g.fillRect(X + 14, 42, 228, 80);
        g.fillStyle = 'rgba(0,0,0,0.55)'; for (let k = 0; k < 6; k++) g.fillRect(X + 24 + k * 37 + rr(-6, 6), 70 + rr(0, 20), rr(10, 22), 60); // silhouettes, rayons
        g.fillStyle = '#07070c'; g.fillRect(X + 86, 42, 5, 80); g.fillRect(X + 166, 42, 5, 80);
        g.fillStyle = signs[i]; g.fillRect(X + 22, 10, 212, 22);
        g.fillStyle = '#07070c'; for (let k = 0; k < 7; k++) g.fillRect(X + 34 + k * 27, 15, rr(10, 20), 12);
      }
    });
    add(mergeGeometries(shopGeos), new THREE.MeshBasicMaterial({ map: shopTex, color: new THREE.Color(1.15, 1.15, 1.15) }));
    disposables.push(shopTex);
  }

  // --- panneaux publicitaires (atlas 2x2) ---
  {
    const A = hex(th.a), B = hex(th.b), C = hex(th.c);
    const atlas = canvasTex(1024, 512, (g) => {
      const cell = (i, fn) => { g.save(); g.translate((i % 2) * 512, Math.floor(i / 2) * 256); g.beginPath(); g.rect(0, 0, 512, 256); g.clip(); fn(); g.restore(); };
      cell(0, () => { const cols = [A, B, C, '#101840', '#ffffff']; for (let y = 0; y < 4; y++) for (let x = 0; x < 8; x++) for (let k = 0; k < 2; k++) {
        g.fillStyle = pick(cols); g.globalAlpha = rr(0.45, 1); g.beginPath(); const s = 64, X = x * s, Y = y * s;
        if (k) { g.moveTo(X, Y); g.lineTo(X + s, Y); g.lineTo(X, Y + s); } else { g.moveTo(X + s, Y + s); g.lineTo(X + s, Y); g.lineTo(X, Y + s); } g.fill(); } g.globalAlpha = 1; });
      const word = (txt, bg, fg, sub) => { g.fillStyle = bg; g.fillRect(0, 0, 512, 256); g.fillStyle = fg; g.font = 'italic 900 150px Arial, sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText(txt, 256, 112);
        g.fillStyle = sub; g.fillRect(40, 205, 432, 14); };
      cell(1, () => word('VOLT', '#05060f', A, B));
      cell(2, () => { const gr = g.createLinearGradient(0, 0, 512, 256); gr.addColorStop(0, B); gr.addColorStop(1, C); g.fillStyle = gr; g.fillRect(0, 0, 512, 256);
        g.fillStyle = '#05060f'; g.font = 'italic 900 150px Arial, sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText('NOVA', 256, 128); });
      cell(3, () => { word('HYPR', '#05060f', '#ffffff', A); g.fillStyle = C; for (let i = 0; i < 9; i++) g.fillRect(20 + i * 54, 20, 30, 12); });
    }, { wrapS: THREE.ClampToEdgeWrapping, wrapT: THREE.ClampToEdgeWrapping });
    const geos = adQuads.map(a => {
      const g = new THREE.PlaneGeometry(a.pw, a.ph); const uv = g.attributes.uv;
      const u0 = (a.cell % 2) * 0.5, v0 = a.cell < 2 ? 0.5 : 0;
      for (let i = 0; i < 4; i++) uv.setXY(i, u0 + uv.getX(i) * 0.5, v0 + uv.getY(i) * 0.5);
      g.rotateY(a.ang); g.translate(a.cx, a.y + a.ph / 2, a.cz); return g;
    });
    if (geos.length) add(mergeGeometries(geos), new THREE.MeshBasicMaterial({ map: atlas, color: new THREE.Color(1.25, 1.25, 1.25), side: THREE.DoubleSide }));
    disposables.push(atlas);
  }

  // --- bordures : glissières rayées, rail néon, grillage ---
  const curbHex = new THREE.Color(th.b).lerp(new THREE.Color(0xd8121e), 0.75);
  const stripeTex = canvasTex(64, 8, (g) => { g.fillStyle = hex(curbHex); g.fillRect(0, 0, 32, 8); g.fillStyle = '#f2f2f2'; g.fillRect(32, 0, 32, 8); });
  const barrierMat = new THREE.MeshStandardMaterial({ map: stripeTex, emissive: 0xffffff, emissiveMap: stripeTex, emissiveIntensity: 0.5, roughness: 0.35, metalness: 0.1, side: THREE.DoubleSide });
  const fenceTex = canvasTex(64, 64, (g) => {
    g.clearRect(0, 0, 64, 64); g.strokeStyle = '#fff'; g.lineWidth = 3.5;
    g.beginPath(); for (let i = -64; i < 128; i += 16) { g.moveTo(i, 0); g.lineTo(i + 64, 64); g.moveTo(i + 64, 0); g.lineTo(i, 64); } g.stroke();
  });
  const fenceMat = new THREE.MeshBasicMaterial({ map: fenceTex, alphaTest: 0.4, color: new THREE.Color(th.c).lerp(new THREE.Color(0x8890c0), 0.7).multiplyScalar(0.55), side: THREE.DoubleSide });
  disposables.push(stripeTex, fenceTex);
  const posts = [];
  for (const s of [-1, 1]) {
    add(wall(T, s * HALF, 0, 1.0, 7), barrierMat);
    add(flat(T, s * HALF, s * (HALF + 0.6), 1.0, 7), barrierMat);
    add(wall(T, s * (HALF + 0.6), 1.0, 4.3, 1.2, 0, 2.75), fenceMat, true);
    const nk = s < 0 ? 'a' : 'b';
    add(wall(T, s * (HALF - 0.02), 1.0, 1.16, 1), new THREE.MeshBasicMaterial({ color: NEON[nk], side: THREE.DoubleSide }));
    add(flat(T, s * (HALF - 0.02), s * (HALF + 0.14), 1.16, 1), new THREE.MeshBasicMaterial({ color: NEON[nk], side: THREE.DoubleSide }));
    add(wall(T, s * (HALF + 0.6), 4.3, 4.42, 1), new THREE.MeshBasicMaterial({ color: NEON[nk].clone().multiplyScalar(0.5), side: THREE.DoubleSide }));
    for (let i = 0; i < T.N; i += 4) { const off = s * (HALF + 0.62); posts.push(box(0.12, 4.4, 0.12, T.px[i] - T.tz[i] * off, 2.2, T.pz[i] + T.tx[i] * off)); }
  }

  // --- lampadaires + flaques de lumière ---
  const pools = [], poles = [], cones = [];
  const palmSpots = [];
  for (let i = 0, n = 0; i < T.N; i += 17, n++) {
    const s = n % 2 ? 1 : -1, ang = Math.atan2(-T.tx[i], -T.tz[i]);
    const o = s * (HALF + 2.4);
    const x = T.px[i] - T.tz[i] * o, z = T.pz[i] + T.tx[i] * o;
    poles.push(box(0.22, 10, 0.22, x, 5, z, ang));
    const o2 = s * (HALF + 0.8), xm = T.px[i] - T.tz[i] * o2, zm = T.pz[i] + T.tx[i] * o2;
    poles.push(box(3.4, 0.18, 0.18, xm, 10, zm, ang));
    const o3 = s * (HALF - 0.6), xl = T.px[i] - T.tz[i] * o3, zl = T.pz[i] + T.tx[i] * o3;
    neonGeos.w.push(box(1.1, 0.14, 0.5, xl, 9.88, zl, ang));
    const o4 = s * (HALF - 3.2);
    pools.push([T.px[i] - T.tz[i] * o4, T.pz[i] + T.tx[i] * o4]);
    const cone = new THREE.ConeGeometry(5.2, 9.8, 14, 1, true); cone.translate(xl, 4.9, zl); cones.push(cone);
  }
  add(mergeGeometries(poles.concat(posts)), new THREE.MeshStandardMaterial({ color: 0x1a1b28, metalness: 0.85, roughness: 0.4 }));
  {
    const pt = canvasTex(64, 64, (g) => { const gr = g.createRadialGradient(32, 32, 0, 32, 32, 32); gr.addColorStop(0, 'rgba(255,255,255,0.55)'); gr.addColorStop(1, 'rgba(255,255,255,0)'); g.fillStyle = gr; g.fillRect(0, 0, 64, 64); });
    const geos = pools.map(([x, z]) => { const g = new THREE.PlaneGeometry(15, 15); g.rotateX(-Math.PI / 2); g.translate(x, 0.02, z); return g; });
    add(mergeGeometries(geos), new THREE.MeshBasicMaterial({ map: pt, color: new THREE.Color(th.lamp).multiplyScalar(0.16), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }), true);
    disposables.push(pt);
    add(mergeGeometries(cones), new THREE.MeshBasicMaterial({ color: new THREE.Color(th.lamp).multiplyScalar(0.014), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, fog: false }), true);
  }

  // --- palmiers ---
  if (th.palms) {
    for (let i = 0; i < T.N; i += 8) for (const s of [-1, 1]) if (rnd() < 0.7) {
      const o = s * (HALF + rr(4.2, 7)), x = T.px[i] - T.tz[i] * o, z = T.pz[i] + T.tx[i] * o;
      if (farFromTrack(x, z, HALF + 3.5)) palmSpots.push([x, z]);
    }
    const parts = [];
    const trunk = new THREE.CylinderGeometry(0.16, 0.3, 9, 7, 5);
    const p = trunk.attributes.position;
    for (let i = 0; i < p.count; i++) { const t = (p.getY(i) + 4.5) / 9; p.setX(i, p.getX(i) + t * t * 0.9); }
    trunk.translate(0, 4.5, 0); parts.push(trunk.toNonIndexed());
    for (let i = 0; i < 13; i++) {
      // palme : bande courbée qui retombe
      const f = new THREE.PlaneGeometry(0.9, 4.6, 2, 6); const qp = f.attributes.position;
      for (let j = 0; j < qp.count; j++) {
        const t = (qp.getY(j) + 2.3) / 4.6, x = qp.getX(j);
        qp.setXYZ(j, x * (1 - t * 0.85), t * 4.6, -t * t * 2.4 - Math.abs(x) * 0.5);
      }
      f.rotateX(Math.PI / 2 - rr(0.5, 1.0)); f.rotateY(i / 13 * Math.PI * 2 + rr(-0.2, 0.2)); f.translate(0.9, 9, 0);
      parts.push(f.toNonIndexed());
    }
    const geo = mergeGeometries(parts); geo.computeVertexNormals();
    const mat = new THREE.MeshStandardMaterial({ color: 0x0c1a12, roughness: 0.7, side: THREE.DoubleSide, emissive: 0x040a07 });
    const im = new THREE.InstancedMesh(geo, mat, palmSpots.length);
    const d = new THREE.Object3D();
    palmSpots.forEach(([x, z], i) => { d.position.set(x, 0, z); d.rotation.y = rr(0, 6.28); const s = rr(0.85, 1.35); d.scale.set(s, s, s); d.updateMatrix(); im.setMatrixAt(i, d.matrix); });
    group.add(im); disposables.push(geo, mat);
  }

  // --- chevrons lumineux à l'extérieur des virages ---
  {
    const chev = canvasTex(256, 64, (g, w, h) => {
      g.fillStyle = '#02040c'; g.fillRect(0, 0, w, h); g.strokeStyle = '#fff'; g.lineWidth = 13;
      for (let i = 0; i < 4; i++) { const x = 30 + i * 62; g.beginPath(); g.moveTo(x - 6, 8); g.lineTo(x + 22, 32); g.lineTo(x - 6, 56); g.stroke(); }
    });
    const geos = [];
    for (let i = 0; i < T.N; i += 4) {
      // prévient un peu avant le virage
      const kk = T.k[(i + 6) % T.N];
      if (Math.abs(kk) < 1 / 120) continue;
      const right = kk > 0, s = right ? -1 : 1; // panneau à l'extérieur
      const o = s * (HALF + 0.5), x = T.px[i] - T.tz[i] * o, z = T.pz[i] + T.tx[i] * o;
      const g = new THREE.PlaneGeometry(7.4, 1.8);
      const uv = g.attributes.uv;
      // la face visible depuis la piste : flèches pointant dans le sens du virage
      if (right) for (let j = 0; j < 4; j++) uv.setX(j, 1 - uv.getX(j));
      g.rotateY(Math.atan2(-T.tx[i], -T.tz[i]) + (s > 0 ? -Math.PI / 2 : Math.PI / 2));
      g.translate(x, 2.2, z); geos.push(g);
    }
    if (geos.length) add(mergeGeometries(geos), new THREE.MeshBasicMaterial({ map: chev, color: new THREE.Color(th.c).lerp(new THREE.Color(0x2fa0ff), 0.4).multiplyScalar(3.0), side: THREE.DoubleSide }));
    disposables.push(chev);
  }

  // --- portiques ---
  {
    const frames = [];
    const checker = canvasTex(256, 32, (g) => { for (let y = 0; y < 2; y++) for (let x = 0; x < 16; x++) { g.fillStyle = (x + y) % 2 ? '#fff' : '#06060a'; g.fillRect(x * 16, y * 16, 16, 16); } });
    const gantry = (i, start) => {
      const ang = Math.atan2(-T.tx[i], -T.tz[i]);
      for (const s of [-1, 1]) { const o = s * (HALF + 1.4); frames.push(box(0.7, 8.4, 0.7, T.px[i] - T.tz[i] * o, 4.2, T.pz[i] + T.tx[i] * o, ang)); }
      frames.push(box(HALF * 2 + 3.5, 0.5, 0.9, T.px[i], 8.4, T.pz[i], ang));
      frames.push(box(HALF * 2 + 3.5, 0.3, 0.6, T.px[i], 6.6, T.pz[i], ang));
      neonGeos[start ? 'w' : 'c'].push(box(HALF * 2 + 3, 0.16, 0.16, T.px[i] + T.tx[i] * 0.5, 6.4, T.pz[i] + T.tz[i] * 0.5, ang));
      neonGeos[start ? 'a' : 'b'].push(box(HALF * 2 + 3, 0.16, 0.16, T.px[i] + T.tx[i] * 0.5, 8.75, T.pz[i] + T.tz[i] * 0.5, ang));
      if (start) {
        const g = new THREE.PlaneGeometry(HALF * 2 + 2, 1.5); g.rotateY(ang); g.translate(T.px[i] - T.tx[i] * 0.5, 7.5, T.pz[i] - T.tz[i] * 0.5);
        add(g, new THREE.MeshBasicMaterial({ map: checker, color: new THREE.Color(1.3, 1.3, 1.3), side: THREE.DoubleSide }));
      }
    };
    gantry(2, true);
    for (const f of [0.27, 0.52, 0.77]) gantry(Math.floor(T.N * f), false);
    add(mergeGeometries(frames), new THREE.MeshStandardMaterial({ color: 0x15161f, metalness: 0.9, roughness: 0.35 }));
    disposables.push(checker);
  }

  for (const kk of Object.keys(neonGeos)) if (neonGeos[kk].length) add(mergeGeometries(neonGeos[kk]), new THREE.MeshBasicMaterial({ color: NEON[kk] }));

  // --- sol hors piste : dalles mouillées (reflet par carte d'environnement) ---
  const ground = add(new THREE.PlaneGeometry(4000, 4000), new THREE.MeshStandardMaterial({ color: 0x08080d, roughness: 0.5, metalness: 0.0, envMapIntensity: 0.3 }));
  ground.rotation.x = -Math.PI / 2; ground.position.set(cx, -0.04, cz);
  ground.renderOrder = -1; // toujours dessiné avant la route, sinon il la recouvre selon l'ordre de tri

  // --- route réfléchissante ---
  const roadGeo = strip(T, (j, s) => {
    const o = HALF + 0.7;
    return [T.px[j] + T.tz[j] * o, -(T.pz[j] - T.tx[j] * o), 0, T.px[j] - T.tz[j] * o, -(T.pz[j] + T.tx[j] * o), 0, -o, s, o, s];
  });
  const road = new Reflector(roadGeo, {
    textureWidth: 512, textureHeight: 256, clipBias: 0.003, multisample: 0,
    shader: {
      uniforms: { color: { value: null }, tDiffuse: { value: null }, textureMatrix: { value: null }, fogColor: { value: FOG }, fogDensity: { value: th.dens }, half_: { value: HALF },
        tint: { value: new THREE.Color(th.c).lerp(new THREE.Color(0.55, 0.5, 0.9), 0.6) }, curbCol: { value: curbHex.clone().multiplyScalar(0.45) } },
      vertexShader: ROAD_VERT, fragmentShader: `#define TAPS ${q.taps}\n` + ROAD_FRAG,
    },
  });
  road.rotation.x = -Math.PI / 2; road.frustumCulled = false; road.renderOrder = 5;
  group.add(road); disposables.push(roadGeo, road.material);

  // --- pluie ---
  let rain = null;
  if (th.rain) {
    const n = 420, pos = new Float32Array(n * 6), base = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { base[i * 3] = rr(-22, 22); base[i * 3 + 1] = rr(0, 18); base[i * 3 + 2] = rr(-22, 22); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    rain = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ color: 0xaac4ff, transparent: true, opacity: 0.22, depthWrite: false }));
    rain.frustumCulled = false; rain.layers.set(1); rain.userData = { base, n };
    group.add(rain); disposables.push(g, rain.material);
  }
  const updateRain = (cam, dt, vx, vz) => {
    if (!rain || !rain.visible) return;
    const { base, n } = rain.userData, p = rain.geometry.attributes.position.array;
    for (let i = 0; i < n; i++) {
      let y = base[i * 3 + 1] - 26 * dt; if (y < 0) y += 18; base[i * 3 + 1] = y;
      // on garde les gouttes dans une boîte centrée sur la caméra
      let x = base[i * 3] - vx * dt, z = base[i * 3 + 2] - vz * dt;
      if (x < -22) x += 44; else if (x > 22) x -= 44; if (z < -22) z += 44; else if (z > 22) z -= 44;
      base[i * 3] = x; base[i * 3 + 2] = z;
      const X = cam.position.x + x, Z = cam.position.z + z, k6 = i * 6;
      p[k6] = X; p[k6 + 1] = y; p[k6 + 2] = Z;
      p[k6 + 3] = X - vx * 0.03; p[k6 + 4] = y + 0.9; p[k6 + 5] = Z - vz * 0.03;
    }
    rain.geometry.attributes.position.needsUpdate = true;
  };

  // --- lumières ---
  group.add(new THREE.HemisphereLight(new THREE.Color(th.c).lerp(new THREE.Color(0x8080ff), 0.5), 0x140820, 1.1));
  const key = new THREE.DirectionalLight(0xbfd0ff, 1.2); key.position.set(-6, 14, 8); group.add(key);
  const rim = new THREE.DirectionalLight(th.b, 1.4); rim.position.set(10, 6, -12); group.add(rim);

  // --- carte d'environnement capturée dans la ville elle-même ---
  const captureEnv = (scene) => {
    road.visible = false;
    const fake = new THREE.Mesh(new THREE.PlaneGeometry(600, 600), new THREE.MeshBasicMaterial({ color: new THREE.Color(th.fog).multiplyScalar(0.5) }));
    fake.rotation.x = -Math.PI / 2; fake.position.set(T.px[0], 0, T.pz[0]); scene.add(fake);
    const crt = new THREE.WebGLCubeRenderTarget(q.env, { type: THREE.HalfFloatType });
    const cc = new THREE.CubeCamera(0.5, 2000, crt); cc.position.set(T.px[4], 1.6, T.pz[4]); scene.add(cc);
    const tm = renderer.toneMapping; renderer.toneMapping = THREE.NoToneMapping;
    cc.update(renderer, scene);
    renderer.toneMapping = tm;
    const pm = new THREE.PMREMGenerator(renderer);
    const env = pm.fromCubemap(crt.texture).texture;
    scene.remove(fake); scene.remove(cc); road.visible = true; crt.dispose(); pm.dispose(); fake.geometry.dispose(); fake.material.dispose();
    disposables.push(env);
    return env;
  };

  const dispose = () => { for (const d of disposables) d.dispose && d.dispose(); road.dispose && road.dispose(); };
  return { T, group, fog, road, rain, updateRain, captureEnv, dispose, theme: th };
}
