// Construction des circuits et des décors : ville néon de nuit, ou nature de jour (prairie, canyon, neige).
import * as THREE from 'three';
import { Reflector } from 'three/addons/objects/Reflector.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

let seed = 1;
const rnd = () => { seed = (seed * 16807) % 2147483647; return (seed - 1) / 2147483646; };
const rr = (a, b) => a + (b - a) * rnd();
const pick = (arr) => arr[Math.floor(rnd() * arr.length)];
const hex = (c) => '#' + new THREE.Color(c).getHexString();
const v3 = (a) => `vec3(${a.map(x => x.toFixed(3)).join(',')})`;

export function canvasTex(w, h, draw, opts = {}) {
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
function colored(g, col) {
  const n = g.index ? g.toNonIndexed() : g; n.deleteAttribute('uv');
  const c = new THREE.Color(col), cnt = n.attributes.position.count, a = new Float32Array(cnt * 3);
  for (let i = 0; i < cnt; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; }
  n.setAttribute('color', new THREE.BufferAttribute(a, 3));
  return n;
}

// bruit et couleur du sol à grande échelle, partagés entre le terrain et les brins d'herbe pour qu'ils se fondent
const GROUND_GLSL = `uniform vec3 gc0; uniform vec3 gc1; uniform vec3 gc2; uniform vec3 gc3; uniform vec3 sunDir; uniform vec3 sunCol; uniform vec3 amb; uniform vec3 fogColor; uniform float fogDensity; uniform float time;
  float gh(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
  float gn(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(gh(i),gh(i+vec2(1,0)),f.x), mix(gh(i+vec2(0,1)),gh(i+vec2(1,1)),f.x), f.y); }
  float gf(vec2 p){ float a=.5, s=0.; for(int i=0;i<3;i++){ s+=a*gn(p); p=p*2.03+17.; a*=.5; } return s; }
  vec3 groundBase(vec2 p, out float field){
    float m = gf(p*0.021+7.0); field = smoothstep(0.50, 0.56, gf(p*0.0032+3.0));
    vec3 c = mix(gc0, gc1, smoothstep(0.25, 0.75, m));
    c = mix(c, gc2, field*0.85);
    return mix(c, gc3, smoothstep(0.66, 0.80, gf(p*0.035+19.0))*0.7); }
`;
// le même bruit côté JS : dit si un point du sol est dans un champ de blé (0..1), pour la paille soulevée par les roues
const fr = (v) => v - Math.floor(v);
function jgh(x, y) { x = fr(x * 123.34); y = fr(y * 456.21); const d = x * (x + 45.32) + y * (y + 45.32); return fr((x + d) * (y + d)); }
function jgn(x, y) { const ix = Math.floor(x), iy = Math.floor(y); let fx = x - ix, fy = y - iy; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  const a = jgh(ix, iy), b = jgh(ix + 1, iy), c = jgh(ix, iy + 1), d = jgh(ix + 1, iy + 1); return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy; }
function fieldAt(x, z) { let px = x * 0.0032 + 3, pz = z * 0.0032 + 3, a = 0.5, s = 0; for (let i = 0; i < 3; i++) { s += a * jgn(px, pz); px = px * 2.03 + 17; pz = pz * 2.03 + 17; a *= 0.5; } return Math.min(1, Math.max(0, (s - 0.5) / 0.06)); }
// ajoute à un matériau standard une variation de teinte en 3D (roche, bois) sans texture
function noisy(mat, scale, amt) {
  mat.onBeforeCompile = (sh) => {
    sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nvarying vec3 vOP;').replace('#include <begin_vertex>', '#include <begin_vertex>\nvOP = position;');
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', `#include <common>
      varying vec3 vOP;
      float nzh(vec3 p){ return fract(sin(dot(p, vec3(12.9898,78.233,37.719)))*43758.5453); }
      float nz(vec3 p){ vec3 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
        return mix(mix(mix(nzh(i),nzh(i+vec3(1,0,0)),f.x), mix(nzh(i+vec3(0,1,0)),nzh(i+vec3(1,1,0)),f.x), f.y), mix(mix(nzh(i+vec3(0,0,1)),nzh(i+vec3(1,0,1)),f.x), mix(nzh(i+vec3(0,1,1)),nzh(i+vec3(1,1,1)),f.x), f.y), f.z); }`)
      .replace('#include <color_fragment>', `#include <color_fragment>\ndiffuseColor.rgb *= ${(1 - amt).toFixed(2)} + ${(amt * 2).toFixed(2)} * (0.6*nz(vOP*${scale.toFixed(1)}) + 0.4*nz(vOP*${(scale * 4.3).toFixed(1)}));`);
  };
  return mat;
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
    raw[i] = ((tx[b] - tx[a]) * (-tz[i]) + (tz[b] - tz[a]) * tx[i]) / (2 * ds); // > 0 : virage à droite
  }
  for (let i = 0; i < N; i++) { let s = 0; for (let j = -3; j <= 3; j++) s += raw[(i + j + N) % N]; k[i] = s / 7; }
  const half = def.half || 21;           // demi-largeur de la route (42 m : pistes larges, on peut doubler partout)
  const T = { def, N, L, ds, px, pz, tx, tz, k, half, limit: def.limit || half - 1.3, libre: !!def.libre };
  T.nearest = (x, z, hint, span = 14) => {
    let bestI = hint, bd = 1e18;
    for (let j = -span; j <= span; j++) {
      const i = (hint + j + N * 4) % N, dx = x - px[i], dz = z - pz[i], d = dx * dx + dz * dz;
      if (d < bd) { bd = d; bestI = i; }
    }
    return bestI;
  };
  T.lateral = (x, z, i) => (x - px[i]) * (-tz[i]) + (z - pz[i]) * tx[i];
  T.point = (i, off, out) => { i = ((i % N) + N) % N; out.x = px[i] - tz[i] * off; out.z = pz[i] + tx[i] * off; return out; };
  T.heading = (i) => { i = ((i % N) + N) % N; return Math.atan2(-tx[i], -tz[i]); };
  return T;
}

// bande le long du tracé : fn(j, s) -> [ax,ay,az, bx,by,bz, ua,va, ub,vb]
function strip(T, fn) {
  const pos = [], uv = [], idx = [];
  for (let i = 0; i <= T.N; i++) {
    const v = fn(i % T.N, i * T.ds);
    pos.push(v[0], v[1], v[2], v[3], v[4], v[5]); uv.push(v[6], v[7], v[8], v[9]);
    if (i > 0) { const q = (i - 1) * 2; idx.push(q, q + 1, q + 2, q + 1, q + 3, q + 2); }
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
const ROAD_FRAG = `uniform sampler2D tDiffuse; uniform vec3 fogColor; uniform float fogDensity; uniform float half_; uniform float laneW; uniform float curbOn;
  uniform vec3 amb; uniform vec3 curbCol; uniform vec3 offCol; uniform vec3 sunDir; uniform vec3 sunCol; uniform float wet; uniform float day; uniform float time; uniform float rainK; uniform float dry;
  varying vec4 vUv; varying vec3 vW; varying vec2 vT;
  float hash(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
  float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
  float fbm(vec2 p){ float a=.5, s=0.; for(int i=0;i<3;i++){ s+=a*noise(p); p*=2.03; a*=.5; } return s; }
  // ronds de pluie : chaque case reçoit une goutte à un instant tiré au hasard, l'onde s'élargit puis s'éteint
  vec2 ripple(vec2 p, float sc, float t){ vec2 q = p*sc; vec2 id = floor(q); vec2 f = fract(q) - 0.5; float h = hash(id);
    vec2 o = (vec2(hash(id + 2.3), hash(id + 5.1)) - 0.5) * 0.5; float ph = fract(t*(0.8 + h*0.7) + h*7.0); vec2 d = f - o; float r = length(d) + 1e-4;
    float R = ph*0.48; float w = sin(clamp((r - R)*52.0, -3.1416, 3.1416)) * smoothstep(0.09, 0.0, abs(r - R)) * (1.0 - ph) * (1.0 - ph);
    return d / r * w; }
  void main(){
    vec3 toCam = cameraPosition - vW; float dist = length(toCam); vec3 V = toCam/dist;
    vec2 p = vW.xz; float ax = abs(vT.x); float s = vT.y;
    float pud = smoothstep(0.42, 0.62, fbm(p*0.16 + 7.3) + (wet - 1.0)*0.35) * (1.0 - 0.9*dry);
    float grain = noise(p*38.0);
    float micro = fbm(p*vec2(5.0,2.2));
    float lx = abs(mod(ax + laneW*0.5, laneW) - laneW*0.5);
    float lane = (1. - smoothstep(0.055, 0.08, lx)) * step(ax, half_ - 2.0) * step(0.45, fract(s/9.0));
    float edge = 1. - smoothstep(0.08, 0.11, abs(ax - (half_-0.95)));
    float curb = step(half_-0.6, ax) * step(ax, half_) * curbOn;
    float cs = step(0.5, fract(s/3.2));
    float startBand = step(abs(s - 3.0), 0.9) * step(ax, half_-1.0) * curbOn;
    float chk = mod(floor(vT.x/0.6) + floor(s/0.6), 2.0);
    float paint = max(lane, edge) * (0.75 + 0.25*grain);
    vec3 albedo = mix(vec3(0.030,0.032,0.042), vec3(0.014,0.015,0.02), micro) + 0.012*grain;
    albedo *= mix(1.0, 2.6, day);
    // bitume sec de jour : gris clair légèrement chaud, granulats qui scintillent, zones polies par les pneus plus sombres
    float agg = noise(p*70.0), agg2 = noise(p*160.0 + 3.0);
    albedo = mix(albedo, albedo*vec3(1.35,1.3,1.22)*(0.82 + 0.36*agg) + vec3(0.02)*step(0.82, agg2), dry);
    albedo = mix(albedo, vec3(0.42,0.42,0.46), paint*0.9);
    albedo = mix(albedo, mix(vec3(0.02), vec3(0.5), chk), startBand);
    albedo = mix(albedo, mix(curbCol, vec3(0.45), cs), curb);
    float off = step(half_, ax);
    // usure : fissures, rustines de bitume, traces de gomme dans chaque voie
    float crack = smoothstep(0.03, 0.0, abs(noise(p*0.55+3.0)-0.5)) * smoothstep(0.5, 0.7, fbm(p*0.06));
    float patchK = step(0.88, hash(floor(p/vec2(3.4,7.5)) + 4.0));
    float lw2 = abs(mod(ax, laneW) - laneW*0.5);
    float rubber = exp(-pow((lw2 - 0.85)/0.4, 2.0)) * step(ax, half_ - 2.0) * (0.6 + 0.4*noise(vec2(vT.x*3.0, s*0.15)));
    float bare = (1.0 - paint) * (1.0 - curb) * (1.0 - startBand);
    albedo *= 1.0 - bare * (0.55*crack + 0.30*patchK + 0.32*rubber);
    albedo = mix(albedo, offCol*(0.8+0.4*grain), off);
    float rough = mix(0.55, 0.06, pud); rough = mix(rough, rough*0.55, patchK*bare); rough = mix(rough, 0.7, crack*bare*0.6); rough = mix(rough, 0.5, max(max(paint, startBand)*0.6, curb*0.7)); rough = mix(rough, 0.9, off); rough = mix(rough, max(rough, 0.78 - 0.25*rubber), dry*(1.0 - paint));
    vec2 uv = vUv.xy / vUv.w;
    vec2 nrm = vec2(noise(p*9.0+1.7)-.5, noise(p*9.0+8.1)-.5) + 0.6*vec2(noise(p*31.)-.5, noise(p*31.+4.)-.5);
    float att = 1.0/(1.0+dist*0.035);
    if (rainK > 0.0) {
      // sous la pluie : bitume détrempé (plus sombre, plus lisse), film d'eau qui ruisselle, ronds de pluie qui font trembler les reflets
      float near = smoothstep(70.0, 8.0, dist) * (1.0 - off);
      vec2 rip = ripple(p, 1.7, time) + 0.7*ripple(p + 13.7, 2.9, time*1.13);
      nrm += rip * rainK * near * (0.9 + 1.6*pud);
      nrm += vec2(noise(p*vec2(2.0,0.6) + vec2(0.0, time*1.8)) - 0.5) * 0.25 * rainK;   // film d'eau qui coule
      albedo *= mix(1.0, 0.55, rainK*(1.0 - off));
      rough *= mix(1.0, 0.5, rainK);
    }
    uv += nrm * (0.012 + 0.05*rough) * att;
    float spread = (0.010 + 0.085*rough) * (0.35 + 0.65*att);
    vec3 refl = vec3(0.); float wsum = 0.;
    for(int i=0;i<TAPS;i++){ float t = (float(i)-float(TAPS/2))/float(TAPS/2); float w = 1.-0.75*abs(t);
      refl += texture2D(tDiffuse, uv + vec2(0.35*nrm.x*t*spread, t*spread)).rgb * w; wsum += w; }
    refl /= wsum;
    float fres = 0.05 + 0.95*pow(1.0 - clamp(V.y,0.,1.), 4.0);
    float strength = mix(0.55, 1.0, pud) * mix(1.0, 0.45, rough) * mix(0.25, 1.0, wet) * (1.0 - off*0.85) * (1.0 + 0.35*rainK) * mix(1.0, 0.4, dry);
    vec3 N = normalize(vec3(nrm.x*0.16, 1.0, nrm.y*0.16));
    vec3 col = albedo * (amb + sunCol * max(dot(N, sunDir), 0.0) * 0.85) + refl * fres * strength;
    // reflet du soleil sur la chaussée : traînée brillante vers le soleil, plus nette dans les flaques
    float sp = pow(max(dot(N, normalize(V + sunDir)), 0.0), mix(45.0, 900.0, 1.0 - rough));
    col += sunCol * sp * (0.25 + 4.0*(1.0 - rough)) * (1.0 - off);
    float f = 1.0 - exp(-fogDensity*fogDensity*dist*dist);
    col = mix(col, fogColor, f);
    gl_FragColor = vec4(col, 1.);
    #include <tonemapping_fragment>
    #include <colorspace_fragment>
  }`;

// façade d'immeuble : 16 colonnes x 32 étages (1,5 m x 3,5 m par cellule).
// Trois textures dessinées ensemble : couleur (béton, vitres sombres), lumière des fenêtres, et brillance (vitres miroir, murs mats).
function facadeTex(palette, lit, wallCol) {
  const W = 512, H = 1024, cw = W / 16, ch = H / 32;
  const mk = () => { const c = document.createElement('canvas'); c.width = W; c.height = H; return c; };
  const cE = mk(), cA = mk(), cR = mk(), e = cE.getContext('2d'), a = cA.getContext('2d'), r = cR.getContext('2d');
  e.fillStyle = '#000'; e.fillRect(0, 0, W, H);
  a.fillStyle = wallCol; a.fillRect(0, 0, W, H);
  r.fillStyle = 'rgb(0,215,0)'; r.fillRect(0, 0, W, H);                       // vert = rugosité, bleu = métal
  for (let i = 0; i < 260; i++) { a.globalAlpha = rr(0.04, 0.16); a.fillStyle = rnd() < 0.5 ? '#000' : '#8a8a90'; const x = rr(0, W); a.fillRect(x, rr(0, H), rr(1, 4), rr(20, 220)); } // coulures
  a.globalAlpha = 0.35; a.fillStyle = '#000'; for (let y = 0; y < 32; y++) a.fillRect(0, y * ch, W, 2); for (let x = 0; x < 16; x += 4) a.fillRect(x * cw, 0, 2, H); a.globalAlpha = 1;
  const style = Math.floor(rnd() * 3);
  for (let y = 0; y < 32; y++) {
    const floorLit = rnd() < 0.18 ? 0.95 : (rnd() < 0.15 ? 0.08 : lit), floorCol = pick(palette);
    for (let x = 0; x < 16; x++) {
      const X = x * cw + (style === 1 ? 2 : 4), Y = y * ch + 5, WW = cw - (style === 1 ? 4 : 8), HH = ch - (style === 2 ? 15 : 11);
      a.fillStyle = '#070b12'; a.fillRect(X, Y, WW, HH);
      r.fillStyle = 'rgb(0,18,255)'; r.fillRect(X, Y, WW, HH);
      a.fillStyle = 'rgba(255,255,255,0.10)'; a.fillRect(X, Y + HH, WW, 2);                                   // appui de fenêtre
      a.fillStyle = '#1a1c22'; a.fillRect(X + WW / 2 - 1, Y, 2, HH);                                          // meneau
      if (rnd() > floorLit) { e.globalAlpha = 0.08; e.fillStyle = '#5a6aa0'; e.fillRect(X, Y, WW, HH); continue; }
      e.globalAlpha = rr(0.4, 1);
      const col = rnd() < 0.7 ? floorCol : pick(palette);
      const gr = e.createLinearGradient(0, Y, 0, Y + HH); gr.addColorStop(0, '#ffffff'); gr.addColorStop(0.25, col); gr.addColorStop(1, col);   // plafonnier en haut de la pièce
      e.fillStyle = gr; e.fillRect(X, Y, WW, HH);
      e.globalAlpha = 0.6; e.fillStyle = '#000';
      if (rnd() < 0.3) e.fillRect(X, Y, WW, HH * rr(0.25, 0.6));                                              // store
      if (rnd() < 0.3) e.fillRect(X + WW * rr(0.1, 0.6), Y + HH * 0.5, WW * 0.28, HH * 0.5);                  // meuble, silhouette
      e.fillRect(X + WW / 2 - 1, Y, 2, HH);
    }
  }
  e.globalAlpha = 1;
  const tex = (c, cs) => { const t = new THREE.CanvasTexture(c); t.colorSpace = cs; t.wrapS = t.wrapT = THREE.RepeatWrapping; t.anisotropy = 4; return t; };
  return { emis: tex(cE, THREE.SRGBColorSpace), alb: tex(cA, THREE.SRGBColorSpace), rm: tex(cR, THREE.NoColorSpace) };
}

// carte d'environnement capturée dans la scène elle-même (reflets sur les carrosseries)
export function captureEnv(renderer, scene, pos, size, hide, floorColor) {
  if (hide) hide.visible = false;
  const fake = new THREE.Mesh(new THREE.PlaneGeometry(600, 600), new THREE.MeshBasicMaterial({ color: floorColor }));
  fake.rotation.x = -Math.PI / 2; fake.position.set(pos.x, 0, pos.z); scene.add(fake);
  const crt = new THREE.WebGLCubeRenderTarget(size, { type: THREE.HalfFloatType });
  const cc = new THREE.CubeCamera(0.5, 2000, crt); cc.position.copy(pos); scene.add(cc);
  const tm = renderer.toneMapping; renderer.toneMapping = THREE.NoToneMapping;
  cc.update(renderer, scene);
  renderer.toneMapping = tm;
  const pm = new THREE.PMREMGenerator(renderer);
  const env = pm.fromCubemap(crt.texture).texture;
  scene.remove(fake); scene.remove(cc); if (hide) hide.visible = true;
  crt.dispose(); pm.dispose(); fake.geometry.dispose(); fake.material.dispose();
  return env;
}

export function skyDome(th) {
  const sun = th.sun ? `float sd = max(dot(d, ${v3(th.sun.dir)}), 0.0); c += ${v3(th.sun.col)} * (pow(sd, 1400.0)*9.0 + pow(sd, 60.0)*0.35 + pow(sd, 7.0)*0.14);` : '';
  const m = new THREE.Mesh(new THREE.SphereGeometry(1800, 24, 12), new THREE.ShaderMaterial({
    side: THREE.BackSide, fog: false, depthWrite: false,
    vertexShader: 'varying vec3 vP; void main(){ vP = position; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.); }',
    fragmentShader: `varying vec3 vP;
      const vec3 HOR = ${v3(th.sky[2])}; const vec3 MID = ${v3(th.sky[1])};
      float h21(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
      float vn(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(h21(i),h21(i+vec2(1,0)),f.x), mix(h21(i+vec2(0,1)),h21(i+vec2(1,1)),f.x), f.y); }
      float fb(vec2 p){ float a=.5, s=0.; for(int i=0;i<4;i++){ s+=a*vn(p); p*=2.1; a*=.5; } return s; }
      void main(){ vec3 d = normalize(vP); float h = d.y;
        vec3 c = mix(HOR, MID, smoothstep(0.0,0.16,h)); c = mix(c, ${v3(th.sky[0])}, smoothstep(0.1,0.6,h));
        float cl = smoothstep(0.42, 0.78, fb(d.xz/(max(d.y,0.0)+0.28)*1.5)) * smoothstep(0.03, 0.3, h);
        ${th.sun ? `float sdc = max(dot(d, ${v3(th.sun.dir)}), 0.0);
        c += ${v3(th.sun.col)} * pow(sdc, 3.0) * 0.10;                                           // halo diffus autour du soleil
        float thick = fb((d.xz + ${v3(th.sun.dir)}.xz*0.10)/(max(d.y,0.0)+0.28)*1.5);            // épaisseur du nuage vers le soleil
        vec3 cc = mix(vec3(1.0)*${(th.cloud ?? 0.9).toFixed(2)} + HOR*0.2, HOR*0.45 + MID*0.25, smoothstep(0.35, 0.85, thick)*0.75);
        cc += ${v3(th.sun.col)} * pow(sdc, 6.0) * 0.55 * (1.0 - smoothstep(0.5, 0.9, cl));         // liseré lumineux côté soleil
        float cl2 = smoothstep(0.50, 0.85, fb(d.xz/(max(d.y,0.0)+0.4)*3.7 + 9.0)) * smoothstep(0.05, 0.4, h) * 0.5;
        c = mix(c, cc, clamp(cl*0.85 + cl2, 0.0, 0.92));` : `vec2 sp = floor(d.xz/max(0.2,d.y+0.3)*90.); c += step(0.9965, h21(sp)) * smoothstep(0.25,0.6,h) * vec3(0.9,0.95,1.0)*0.8*(1.-cl); c = mix(c, HOR*0.5 + MID*0.35, cl*0.6);`}
        ${sun}
        gl_FragColor = vec4(c,1.);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }`,
  }));
  m.frustumCulled = false;
  return m;
}

export function buildWorld(def, renderer, q) {
  seed = 1337 + def.id.length * 7919 + def.pts.length * 104729;
  const th = def.theme, city = th.env !== 'nature';
  const T = makeTrack(def), HALF = T.half;
  const group = new THREE.Group();
  const disposables = [];
  const add = (geo, mat, layer1) => { const m = new THREE.Mesh(geo, mat); if (layer1) m.layers.set(1); group.add(m); disposables.push(geo, mat); return m; };
  const FOG = new THREE.Color(th.fog);
  const fog = new THREE.FogExp2(FOG, th.dens);
  const sky = skyDome(th); group.add(sky); disposables.push(sky.geometry, sky.material);
  const farFromTrack = (x, z, r) => { for (let i = 0; i < T.N; i += 3) { const dx = x - T.px[i], dz = z - T.pz[i]; if (dx * dx + dz * dz < r * r) return false; } return true; };
  let cx = 0, cz = 0, R = 0;
  for (let i = 0; i < T.N; i++) { cx += T.px[i]; cz += T.pz[i]; } cx /= T.N; cz /= T.N;
  for (let i = 0; i < T.N; i++) R = Math.max(R, Math.hypot(T.px[i] - cx, T.pz[i] - cz));
  const updaters = [];
  let bend = null;

  const stripeCol = city ? new THREE.Color(th.b).lerp(new THREE.Color(0xd8121e), 0.75) : new THREE.Color(0xd8121e);
  const stripeTex = canvasTex(64, 8, (g) => { g.fillStyle = hex(stripeCol); g.fillRect(0, 0, 32, 8); g.fillStyle = '#f2f2f2'; g.fillRect(32, 0, 32, 8); });
  disposables.push(stripeTex);

  // panneaux publicitaires (atlas 2x2), utilisés en ville et le long des circuits de campagne
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
  disposables.push(atlas);
  const adQuads = [];
  const flushAds = (bright) => {
    const geos = adQuads.map(a => {
      const g = new THREE.PlaneGeometry(a.pw, a.ph); const uv = g.attributes.uv;
      const u0 = (a.cell % 2) * 0.5, v0 = a.cell < 2 ? 0.5 : 0;
      for (let i = 0; i < 4; i++) uv.setXY(i, u0 + uv.getX(i) * 0.5, v0 + uv.getY(i) * 0.5);
      g.rotateY(a.ang); g.translate(a.cx, a.y + a.ph / 2, a.cz); return g;
    });
    if (geos.length) add(mergeGeometries(geos), new THREE.MeshBasicMaterial({ map: atlas, color: new THREE.Color(bright, bright, bright), side: THREE.DoubleSide }));
  };

  // portiques (départ + repères)
  const NEON = { a: new THREE.Color(th.a).multiplyScalar(3.2), b: new THREE.Color(th.b).multiplyScalar(3.0), c: new THREE.Color(th.c).multiplyScalar(3.4), w: new THREE.Color(th.lamp || 0xffffff).multiplyScalar(3.0) };
  const neonGeos = { a: [], b: [], c: [], w: [] };
  if (!T.libre) {
    const frames = [];
    const checker = canvasTex(256, 32, (g) => { for (let y = 0; y < 2; y++) for (let x = 0; x < 16; x++) { g.fillStyle = (x + y) % 2 ? '#fff' : '#06060a'; g.fillRect(x * 16, y * 16, 16, 16); } });
    const gantry = (i, start) => {
      const ang = Math.atan2(-T.tx[i], -T.tz[i]);
      for (const s of [-1, 1]) { const o = s * (HALF + 1.4); frames.push(box(0.7, 8.4, 0.7, T.px[i] - T.tz[i] * o, 4.2, T.pz[i] + T.tx[i] * o, ang)); }
      frames.push(box(HALF * 2 + 3.5, 0.5, 0.9, T.px[i], 8.4, T.pz[i], ang));
      frames.push(box(HALF * 2 + 3.5, 0.3, 0.6, T.px[i], 6.6, T.pz[i], ang));
      if (city) {
        neonGeos[start ? 'w' : 'c'].push(box(HALF * 2 + 3, 0.16, 0.16, T.px[i] + T.tx[i] * 0.5, 6.4, T.pz[i] + T.tz[i] * 0.5, ang));
        neonGeos[start ? 'a' : 'b'].push(box(HALF * 2 + 3, 0.16, 0.16, T.px[i] + T.tx[i] * 0.5, 8.75, T.pz[i] + T.tz[i] * 0.5, ang));
      } else if (!start) adQuads.push({ cx: T.px[i], cz: T.pz[i], y: 6.75, pw: HALF * 2 + 2, ph: 1.5, ang, cell: 1 + Math.floor(rnd() * 3) });
      if (start) {
        const g = new THREE.PlaneGeometry(HALF * 2 + 2, 1.5); g.rotateY(ang); g.translate(T.px[i] - T.tx[i] * 0.5, 7.5, T.pz[i] - T.tz[i] * 0.5);
        add(g, new THREE.MeshBasicMaterial({ map: checker, color: new THREE.Color(1.3, 1.3, 1.3), side: THREE.DoubleSide }));
      }
    };
    gantry(2, true);
    for (const f of [0.27, 0.52, 0.77]) gantry(Math.floor(T.N * f), false);
    add(mergeGeometries(frames), new THREE.MeshStandardMaterial({ color: city ? 0x15161f : 0x3a3d48, metalness: 0.9, roughness: 0.35 }));
    disposables.push(checker);
  }

  // chevrons à l'extérieur des virages
  if (!T.libre) {
    const chev = canvasTex(256, 64, (g, w, h) => {
      g.fillStyle = city ? '#02040c' : '#f4f4f4'; g.fillRect(0, 0, w, h); g.strokeStyle = city ? '#fff' : '#d8121e'; g.lineWidth = 13;
      for (let i = 0; i < 4; i++) { const x = 30 + i * 62; g.beginPath(); g.moveTo(x - 6, 8); g.lineTo(x + 22, 32); g.lineTo(x - 6, 56); g.stroke(); }
    });
    const geos = [];
    for (let i = 0; i < T.N; i += 4) {
      const kk = T.k[(i + 6) % T.N];
      if (Math.abs(kk) < 1 / 120) continue;
      const right = kk > 0, s = right ? -1 : 1;
      const o = s * (HALF + 0.5), x = T.px[i] - T.tz[i] * o, z = T.pz[i] + T.tx[i] * o;
      const g = new THREE.PlaneGeometry(7.4, 1.8); const uv = g.attributes.uv;
      if (right) for (let j = 0; j < 4; j++) uv.setX(j, 1 - uv.getX(j));
      g.rotateY(Math.atan2(-T.tx[i], -T.tz[i]) + (s > 0 ? -Math.PI / 2 : Math.PI / 2));
      g.translate(x, city ? 2.2 : 1.9, z); geos.push(g);
    }
    if (geos.length) add(mergeGeometries(geos), new THREE.MeshBasicMaterial({ map: chev, color: city ? new THREE.Color(th.c).lerp(new THREE.Color(0x2fa0ff), 0.4).multiplyScalar(3.0) : new THREE.Color(0.85, 0.85, 0.85), side: THREE.DoubleSide }));
    disposables.push(chev);
  }

  if (city) {
    // ================= VILLE NÉON =================
    const facades = th.win.map((p, i) => facadeTex(p, [0.6, 0.55, 0.5, 0.7][i], ['#2a2c38', '#34302f', '#1f2531', '#2c2a35'][i]));
    const bMats = facades.map(f => new THREE.MeshStandardMaterial({ map: f.alb, roughness: 1, metalness: 1, roughnessMap: f.rm, metalnessMap: f.rm, emissive: 0xffffff, emissiveMap: f.emis, emissiveIntensity: 1.3, envMapIntensity: 1.15 }));
    for (const f of facades) disposables.push(f.emis, f.alb, f.rm);
    const bGeos = [[], [], [], []], placed = [], shopGeos = [];
    const tower = (x, z, w, d, h, ang, mi) => {
      const g = new THREE.BoxGeometry(w, h, d); const uv = g.attributes.uv;
      const ou = Math.floor(rr(0, 16)) / 16, ov = Math.floor(rr(0, 32)) / 32;
      for (let f = 0; f < 6; f++) for (let i = 0; i < 4; i++) {
        const kk = f * 4 + i;
        if (f === 2 || f === 3) { uv.setXY(kk, 0.001, 0.001); continue; }
        uv.setXY(kk, Math.round(uv.getX(kk) * (f < 2 ? d : w) / 1.5) / 16 + ou, Math.round(uv.getY(kk) * h / 3.5) / 32 + ov);
      }
      g.rotateY(ang); g.translate(x, h / 2, z); bGeos[mi].push(g);
    };
    const building = (x, z, w, d, h, ang, faceSide) => {
      const mi = Math.floor(rnd() * 4);
      tower(x, z, w, d, h, ang, mi);
      let top = h;
      if (rnd() < 0.55) { const s = rr(0.55, 0.8), h2 = rr(8, 30); tower(x, z, w * s, d * s, h + h2, ang, mi); top = h + h2; }
      const ca = Math.cos(ang), sa = Math.sin(ang);
      const corner = (lx, lz) => [x + lx * ca + lz * sa, z - lx * sa + lz * ca];
      if (rnd() < 0.8) {
        const kk = pick(['a', 'b', 'c', 'a', 'b']);
        for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) if (rnd() < 0.6) { const [qx, qz] = corner(sx * w / 2, sz * d / 2); neonGeos[kk].push(box(0.7, h, 0.7, qx, h / 2, qz, ang)); }
        if (rnd() < 0.6) neonGeos[pick(['a', 'b', 'c'])].push(box(w + 0.6, 0.8, d + 0.6, x, h - rr(0.5, 5), z, ang));
        if (rnd() < 0.4) neonGeos[pick(['a', 'b'])].push(box(w + 0.6, 0.6, d + 0.6, x, h * rr(0.3, 0.7), z, ang));
      }
      if (rnd() < 0.35) neonGeos.w.push(box(0.5, rr(8, 22), 0.5, x, top + 6, z));
      if (faceSide) {
        const g = new THREE.BoxGeometry(w + 0.5, 5.2, d + 0.5); const suv = g.attributes.uv; const o = Math.floor(rr(0, 4)) / 4;
        for (let f = 0; f < 6; f++) for (let i = 0; i < 4; i++) { const kk = f * 4 + i; if (f === 2 || f === 3) { suv.setXY(kk, 0.002, 0.002); continue; } suv.setX(kk, Math.round(suv.getX(kk) * (f < 2 ? d : w) / 6) / 4 + o); }
        g.rotateY(ang); g.translate(x, 2.6, z); shopGeos.push(g);
      }
      if (faceSide && rnd() < 0.5 && w > 16) {
        const pw = Math.min(w - 3, rr(11, 18)), ph = pw * 0.5, y = rr(9, Math.max(10, Math.min(24, h - ph)));
        const [qx, qz] = corner(-faceSide * (w / 2 + 0.25), 0);
        adQuads.push({ cx: qx, cz: qz, y, pw, ph, ang: ang + (faceSide > 0 ? -Math.PI / 2 : Math.PI / 2), cell: Math.floor(rnd() * 4) });
      }
    };
    const freeSpot = (x, z, r) => { for (const p of placed) { const dx = x - p[0], dz = z - p[1], r2 = (r + p[2]) * 0.92; if (dx * dx + dz * dz < r2 * r2) return false; } return true; };
    for (let row = 0; row < 2; row++) for (let i = 0; i < T.N; i += 9) for (const side of [-1, 1]) {
      const w = rr(18, 30), d = rr(16, 30), r = Math.hypot(w, d) / 2;
      const off = side * (HALF + 13 + w / 2 + row * 40 + rr(0, 8));
      const j = (i + Math.floor(rr(0, 6))) % T.N;
      const x = T.px[j] - T.tz[j] * off, z = T.pz[j] + T.tx[j] * off;
      if (!farFromTrack(x, z, HALF + 9 + r) || !freeSpot(x, z, r)) continue;
      building(x, z, w, d, rr(24, 60) + row * rr(20, 50) + (rnd() < 0.2 ? rr(40, 90) : 0), Math.atan2(-T.tx[j], -T.tz[j]) + Math.PI / 2, row === 0 ? side : 0);
      placed.push([x, z, r]);
    }
    for (let i = 0; i < 130; i++) {
      const a = rr(0, Math.PI * 2), rad = R * rr(0.0, 1) + rr(110, 430) * (rnd() < 0.5 ? 1 : 0.6);
      const x = cx + Math.cos(a) * rad, z = cz + Math.sin(a) * rad, w = rr(20, 36), d = rr(20, 36), r = Math.hypot(w, d) / 2;
      if (!farFromTrack(x, z, HALF + 60 + r) || !freeSpot(x, z, r)) continue;
      building(x, z, w, d, rr(70, 160) + (rnd() < 0.3 ? rr(60, 130) : 0), rr(0, 3.14), 0);
      placed.push([x, z, r]);
    }
    bGeos.forEach((gs, i) => { if (gs.length) add(mergeGeometries(gs), bMats[i]); });
    if (shopGeos.length) {
      const shopTex = canvasTex(1024, 128, (g) => {
        g.fillStyle = '#07070c'; g.fillRect(0, 0, 1024, 128);
        const signs = [A, B, C, '#ffffff'];
        for (let i = 0; i < 4; i++) {
          const X = i * 256, warm = rnd() < 0.5;
          const gr = g.createLinearGradient(0, 40, 0, 128); gr.addColorStop(0, warm ? '#ffd9a0' : '#bfe9ff'); gr.addColorStop(1, warm ? '#7a4a20' : '#20506a');
          g.fillStyle = gr; g.fillRect(X + 14, 42, 228, 80);
          g.fillStyle = 'rgba(0,0,0,0.55)'; for (let k = 0; k < 6; k++) g.fillRect(X + 24 + k * 37 + rr(-6, 6), 70 + rr(0, 20), rr(10, 22), 60);
          g.fillStyle = '#07070c'; g.fillRect(X + 86, 42, 5, 80); g.fillRect(X + 166, 42, 5, 80);
          g.fillStyle = signs[i]; g.fillRect(X + 22, 10, 212, 22);
          g.fillStyle = '#07070c'; for (let k = 0; k < 7; k++) g.fillRect(X + 34 + k * 27, 15, rr(10, 20), 12);
        }
      });
      add(mergeGeometries(shopGeos), new THREE.MeshBasicMaterial({ map: shopTex, color: new THREE.Color(1.15, 1.15, 1.15) }));
      disposables.push(shopTex);
    }
    flushAds(1.25);

    // glissières rayées, rail néon, grillage
    const barrierMat = new THREE.MeshStandardMaterial({ map: stripeTex, emissive: 0xffffff, emissiveMap: stripeTex, emissiveIntensity: 0.5, roughness: 0.35, metalness: 0.1, side: THREE.DoubleSide });
    const fenceTex = canvasTex(64, 64, (g) => {
      g.clearRect(0, 0, 64, 64); g.strokeStyle = '#fff'; g.lineWidth = 3.5;
      g.beginPath(); for (let i = -64; i < 128; i += 16) { g.moveTo(i, 0); g.lineTo(i + 64, 64); g.moveTo(i + 64, 0); g.lineTo(i, 64); } g.stroke();
    });
    const fenceMat = new THREE.MeshBasicMaterial({ map: fenceTex, alphaTest: 0.4, color: new THREE.Color(th.c).lerp(new THREE.Color(0x8890c0), 0.7).multiplyScalar(0.55), side: THREE.DoubleSide });
    disposables.push(fenceTex);
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
    // lampadaires, flaques et cônes de lumière
    const pools = [], poles = [], cones = [];
    for (let i = 0, n = 0; i < T.N; i += 17, n++) {
      const s = n % 2 ? 1 : -1, ang = Math.atan2(-T.tx[i], -T.tz[i]);
      const o = s * (HALF + 2.4);
      poles.push(box(0.22, 10, 0.22, T.px[i] - T.tz[i] * o, 5, T.pz[i] + T.tx[i] * o, ang));
      const o2 = s * (HALF + 0.8); poles.push(box(3.4, 0.18, 0.18, T.px[i] - T.tz[i] * o2, 10, T.pz[i] + T.tx[i] * o2, ang));
      const o3 = s * (HALF - 0.6), xl = T.px[i] - T.tz[i] * o3, zl = T.pz[i] + T.tx[i] * o3;
      neonGeos.w.push(box(1.1, 0.14, 0.5, xl, 9.88, zl, ang));
      const o4 = s * (HALF - 3.2); pools.push([T.px[i] - T.tz[i] * o4, T.pz[i] + T.tx[i] * o4]);
      const cone = new THREE.ConeGeometry(5.2, 9.8, 14, 1, true); cone.translate(xl, 4.9, zl); cones.push(cone);
    }
    add(mergeGeometries(poles.concat(posts)), new THREE.MeshStandardMaterial({ color: 0x1a1b28, metalness: 0.85, roughness: 0.4 }));
    const pt = canvasTex(64, 64, (g) => { const gr = g.createRadialGradient(32, 32, 0, 32, 32, 32); gr.addColorStop(0, 'rgba(255,255,255,0.55)'); gr.addColorStop(1, 'rgba(255,255,255,0)'); g.fillStyle = gr; g.fillRect(0, 0, 64, 64); });
    add(mergeGeometries(pools.map(([x, z]) => { const g = new THREE.PlaneGeometry(15, 15); g.rotateX(-Math.PI / 2); g.translate(x, 0.02, z); return g; })),
      new THREE.MeshBasicMaterial({ map: pt, color: new THREE.Color(th.lamp).multiplyScalar(0.16), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }), true);
    add(mergeGeometries(cones), new THREE.MeshBasicMaterial({ color: new THREE.Color(th.lamp).multiplyScalar(0.014), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, fog: false }), true);
    disposables.push(pt);
    // palmiers
    if (th.palms) {
      const spots = [];
      for (let i = 0; i < T.N; i += 8) for (const s of [-1, 1]) if (rnd() < 0.7) {
        const o = s * (HALF + rr(4.2, 7)), x = T.px[i] - T.tz[i] * o, z = T.pz[i] + T.tx[i] * o;
        if (farFromTrack(x, z, HALF + 3.5)) spots.push([x, z]);
      }
      const parts = [];
      const trunk = new THREE.CylinderGeometry(0.16, 0.3, 9, 7, 5); const p = trunk.attributes.position;
      for (let i = 0; i < p.count; i++) { const t = (p.getY(i) + 4.5) / 9; p.setX(i, p.getX(i) + t * t * 0.9); }
      trunk.translate(0, 4.5, 0); parts.push(trunk.toNonIndexed());
      for (let i = 0; i < 13; i++) {
        const f = new THREE.PlaneGeometry(0.9, 4.6, 2, 6); const qp = f.attributes.position;
        for (let j = 0; j < qp.count; j++) { const t = (qp.getY(j) + 2.3) / 4.6, x = qp.getX(j); qp.setXYZ(j, x * (1 - t * 0.85), t * 4.6, -t * t * 2.4 - Math.abs(x) * 0.5); }
        f.rotateX(Math.PI / 2 - rr(0.5, 1.0)); f.rotateY(i / 13 * Math.PI * 2 + rr(-0.2, 0.2)); f.translate(0.9, 9, 0);
        parts.push(f.toNonIndexed());
      }
      const geo = mergeGeometries(parts); geo.computeVertexNormals();
      const mat = new THREE.MeshStandardMaterial({ color: 0x0c1a12, roughness: 0.7, side: THREE.DoubleSide, emissive: 0x040a07 });
      const im = new THREE.InstancedMesh(geo, mat, spots.length); const d = new THREE.Object3D();
      spots.forEach(([x, z], i) => { d.position.set(x, 0, z); d.rotation.y = rr(0, 6.28); const s = rr(0.85, 1.35); d.scale.set(s, s, s); d.updateMatrix(); im.setMatrixAt(i, d.matrix); });
      group.add(im); disposables.push(geo, mat);
    }
    for (const kk of Object.keys(neonGeos)) if (neonGeos[kk].length) add(mergeGeometries(neonGeos[kk]), new THREE.MeshBasicMaterial({ color: NEON[kk] }));
    // trottoirs : dalles de pierre mouillées, avec des flaques qui reflètent la ville
    const slabA = canvasTex(256, 256, (g) => { g.fillStyle = '#16171d'; g.fillRect(0, 0, 256, 256);
      for (let y = 0; y < 4; y++) for (let x = 0; x < 4; x++) { const v = 18 + rnd() * 16 | 0; g.fillStyle = `rgb(${v},${v},${v + 5})`; g.fillRect(x * 64 + 2, y * 64 + 2, 60, 60); }
      for (let i = 0; i < 700; i++) { g.fillStyle = rnd() < 0.5 ? 'rgba(0,0,0,0.25)' : 'rgba(120,120,140,0.10)'; g.fillRect(rr(0, 256), rr(0, 256), rr(1, 3), rr(1, 3)); } });
    const slabR = canvasTex(256, 256, (g) => { g.fillStyle = 'rgb(0,150,0)'; g.fillRect(0, 0, 256, 256);
      for (let i = 0; i < 9; i++) { const x = rr(0, 256), y = rr(0, 256), rad = rr(22, 60); const gr = g.createRadialGradient(x, y, rad * 0.3, x, y, rad); gr.addColorStop(0, 'rgba(0,14,0,1)'); gr.addColorStop(1, 'rgba(0,14,0,0)'); g.fillStyle = gr; g.fillRect(x - rad, y - rad, rad * 2, rad * 2); }
      g.fillStyle = 'rgb(0,200,0)'; for (let k = 0; k <= 4; k++) { g.fillRect(k * 64 - 1, 0, 3, 256); g.fillRect(0, k * 64 - 1, 256, 3); } }, { colorSpace: THREE.NoColorSpace });
    slabA.repeat.set(750, 750); slabR.repeat.set(750, 750); disposables.push(slabA, slabR);
    const ground = add(new THREE.PlaneGeometry(6000, 6000, 72, 72), new THREE.MeshStandardMaterial({ map: slabA, roughness: 1, roughnessMap: slabR, metalness: 0.0, envMapIntensity: 0.75, polygonOffset: true, polygonOffsetFactor: 2, polygonOffsetUnits: 8 }));
    ground.rotation.x = -Math.PI / 2; ground.position.set(cx, -0.1, cz); ground.renderOrder = -1; // découpé en dalles : un rectangle géant fausse la profondeur et passe devant la route
    // ville de jour : la lumière vient du ciel et du soleil au lieu des néons
    if (th.jourVille) { group.add(new THREE.HemisphereLight(new THREE.Color().fromArray(th.sky[1]), 0x3a3d46, 2.3)); const sl = new THREE.DirectionalLight(new THREE.Color().fromArray(th.sun.col), 3.0); sl.position.fromArray(th.sun.dir).multiplyScalar(100); group.add(sl); }
    else group.add(new THREE.HemisphereLight(new THREE.Color(th.c).lerp(new THREE.Color(0x8080ff), 0.5), 0x140820, 1.1));
    const key = new THREE.DirectionalLight(0xbfd0ff, th.jourVille ? 0.3 : 1.2); key.position.set(-6, 14, 8); group.add(key);
    const rim = new THREE.DirectionalLight(th.b, 1.4); rim.position.set(10, 6, -12); group.add(rim);
  } else {
    // ================= NATURE DE JOUR =================
    const gcol = new THREE.Color(th.ground);
    const sombre = th.nuit || th.rain ? 1 : 0, sunV = new THREE.Vector3().fromArray(th.sun.dir).normalize(), sunC = new THREE.Color().fromArray(th.sun.col).multiplyScalar((th.sunI ?? 2.6) * 0.5 * (sombre ? 1 : 1.18));
    const ambC = new THREE.Color().fromArray(th.sky[1]).lerp(new THREE.Color(0.75, 0.78, 0.8), sombre ? 0 : 0.35).multiplyScalar((th.ambI ?? 1.5) * (sombre ? 1 : 0.78));
    const sol = th.sol;
    const gUni = () => ({ gc0: { value: new THREE.Color(sol.c[0]) }, gc1: { value: new THREE.Color(sol.c[1]) }, gc2: { value: new THREE.Color(sol.c[2]) }, gc3: { value: new THREE.Color(sol.c[3]) },
      sunDir: { value: sunV }, sunCol: { value: sunC }, amb: { value: ambC }, fogColor: { value: FOG }, fogDensity: { value: th.dens }, time: { value: 0 } });
    const timed = [];
    updaters.push((cam, dt, time) => { for (const m of timed) m.uniforms.time.value = time; });

    // --- relief : plat près de la piste, vallonné au-delà ---
    const distTrack = (x, z) => { let b = 1e18; for (let i = 0; i < T.N; i += 4) { const dx = x - T.px[i], dz = z - T.pz[i], d = dx * dx + dz * dz; if (d < b) b = d; } return Math.sqrt(b); };
    const amp = th.relief ?? 34;
    const hills = (x, z) => Math.sin(x * 0.0061 + 1.3) * Math.cos(z * 0.0053 + 0.4) * 0.5 + Math.sin(x * 0.013 + z * 0.011) * 0.28 + Math.sin(x * 0.031 - z * 0.027 + 2.0) * 0.1 + 0.55;
    const mer = th.mer;
    const heightAt = mer
      // côte : plage en pente douce jusqu'à la mer, quelques îles et caps au loin
      ? (x, z) => { const d = distTrack(x, z), t = Math.min(1, Math.max(0, (d - 60) / 300)), s = Math.min(1, Math.max(0, (d - 24) / 56));
          return (hills(x, z) - 0.72) * amp * t * t * (3 - 2 * t) - 8 * s * s * (3 - 2 * s); }
      : (x, z) => { const t = Math.min(1, Math.max(0, (distTrack(x, z) - 34) / 240)); return hills(x, z) * amp * t * t * (3 - 2 * t); };
    {
      const size = 2 * (R + 1050), seg = q.terrain;
      const g = new THREE.PlaneGeometry(size, size, seg, seg); const p = g.attributes.position;
      for (let i = 0; i < p.count; i++) p.setZ(i, heightAt(cx + p.getX(i), cz - p.getY(i)));
      g.computeVertexNormals();
      const mat = new THREE.ShaderMaterial({
        uniforms: Object.assign(gUni(), { kind: { value: sol.kind }, eau: { value: th.mer ? th.mer.niveau + 0.12 : -1e4 } }), polygonOffset: true, polygonOffsetFactor: 2, polygonOffsetUnits: 8,
        vertexShader: `varying vec3 vW; varying vec3 vN; void main(){ vec4 w = modelMatrix*vec4(position,1.); vW = w.xyz; vN = normalize(mat3(modelMatrix)*normal); gl_Position = projectionMatrix*viewMatrix*w; }`,
        fragmentShader: GROUND_GLSL + `uniform float kind; uniform float eau; varying vec3 vW; varying vec3 vN;
          void main(){
            if (vW.y < eau) discard;                       // sous la mer : seule l'eau est dessinée (sinon les deux se disputent l'écran au loin)
            vec2 p = vW.xz; vec3 toCam = cameraPosition - vW; float dist = length(toCam); vec3 V = toCam/dist;
            float field; vec3 alb = groundBase(p, field);
            float near = exp(-dist/55.0), mid = exp(-dist/260.0);
            vec3 N = normalize(vN);
            if (kind < 0.5) {
              // herbe : brins serrés de près, touffes à mi-distance, sillons dans les champs, fleurs
              float streak = gn(p*vec2(31.0,29.0)) * gn(p.yx*vec2(47.0,53.0)+5.0);
              alb *= mix(1.0, 0.5 + 1.5*streak, near) * mix(1.0, 0.78 + 0.44*gn(p*3.1), mid);
              alb *= 1.0 + field * 0.07 * sin(dot(p, vec2(0.6,0.8))*1.6);
              float fl = step(0.992, gh(floor(p*2.6))) * near * (1.0 - field);
              alb = mix(alb, mix(vec3(0.95,0.85,0.25), vec3(0.95), step(0.5, gh(floor(p*2.6)+3.0))), fl);
              N = normalize(N + vec3(gn(p*1.7)-0.5, 0.0, gn(p*1.7+9.0)-0.5)*0.4);
            } else if (kind < 1.5) {
              // sable : rides du vent, terre craquelée, cailloux
              float ph = (p.x*0.9 + p.y*0.35)*2.2 + gf(p*0.12)*9.0;
              float crack = smoothstep(0.035, 0.0, abs(gn(p*0.9)-0.5)) * smoothstep(0.45, 0.6, gf(p*0.05+4.0));
              float peb = step(0.975, gh(floor(p*5.0))) * near;
              alb *= 0.92 + 0.10*sin(ph)*mid + 0.28*(gn(p*24.0)-0.5)*near; alb *= 1.0 - 0.6*crack*mid; alb *= 1.0 - 0.45*peb;
              N = normalize(N + vec3(cos(ph), 0.0, 0.0)*0.16*mid);
            } else {
              // neige : congères bleutées, grain fin
              N = normalize(N + vec3(gn(p*0.6)-0.5, 0.0, gn(p*0.6+5.0)-0.5)*0.55);
              alb = mix(alb, alb*vec3(0.78,0.86,1.0), smoothstep(0.4, 0.7, gf(p*0.09)));
              alb *= 0.95 + 0.10*gn(p*14.0)*near;
            }
            alb *= mix(0.5, 1.0, smoothstep(-0.6, 0.25, vW.y));                                // sable mouillé au bord de l'eau
            float cs = smoothstep(0.48, 0.72, gf(p*0.0045 + vec2(time*0.012, time*0.007)));   // ombres des nuages
            float diff = max(dot(N, sunDir), 0.0);
            vec3 col = alb * (amb*0.55 + sunCol * diff * 1.3 * (1.0 - 0.45*cs));
            if (kind > 1.5) { col += sunCol * pow(gh(floor(p*38.0) + floor(V.xz*9.0)), 40.0) * 2.5 * near; col += sunCol * pow(max(dot(reflect(-sunDir, N), V), 0.0), 24.0) * 0.22; }
            col = mix(col, fogColor, 1.0 - exp(-fogDensity*fogDensity*dist*dist));
            gl_FragColor = vec4(col, 1.0);
            #include <tonemapping_fragment>
            #include <colorspace_fragment>
          }`,
      });
      const ground = add(g, mat); ground.rotation.x = -Math.PI / 2; ground.position.set(cx, -0.1, cz); ground.renderOrder = -1; ground.frustumCulled = false;
      timed.push(mat);
    }
    if (mer) {
      // eau : houle en plusieurs trains de vagues, reflet du ciel selon l'angle (Fresnel), traînée du soleil et éclats
      const wm = new THREE.ShaderMaterial({
        uniforms: Object.assign(gUni(), { hor: { value: new THREE.Color().fromArray(th.sky[2]) }, mid: { value: new THREE.Color().fromArray(th.sky[1]) }, top: { value: new THREE.Color().fromArray(th.sky[0]) },
          deep: { value: new THREE.Color(mer.profond) }, sunRaw: { value: new THREE.Color().fromArray(th.sun.col) } }),
        vertexShader: `varying vec3 vW; void main(){ vec4 w = modelMatrix*vec4(position,1.); vW = w.xyz; gl_Position = projectionMatrix*viewMatrix*w; }`,
        fragmentShader: GROUND_GLSL + `uniform vec3 hor; uniform vec3 mid; uniform vec3 top; uniform vec3 deep; uniform vec3 sunRaw; varying vec3 vW;
          vec2 wave(vec2 p, vec2 d, float k, float a, float sp){ float ph = dot(d, p)*k + time*sp; return d * k * a * cos(ph); }
          void main(){
            vec2 p = vW.xz; vec3 toCam = cameraPosition - vW; float dist = length(toCam); vec3 V = toCam/dist;
            vec2 g = wave(p, vec2(0.80, 0.60), 0.090, 0.55, 1.10) + wave(p, vec2(-0.45, 0.89), 0.140, 0.32, 1.45)
                   + wave(p, vec2(0.97, -0.24), 0.310, 0.12, 2.10) + wave(p, vec2(-0.70, -0.71), 0.520, 0.06, 2.90);
            float e = 0.35; vec2 q = p*0.9 + vec2(time*0.6, time*0.35);                       // clapot fin
            g += vec2(gn(q + vec2(e,0.)) - gn(q - vec2(e,0.)), gn(q + vec2(0.,e)) - gn(q - vec2(0.,e))) * 0.55;
            vec2 q2 = p*3.1 - vec2(time*0.9, time*0.4);
            g += vec2(gn(q2 + vec2(e,0.)) - gn(q2 - vec2(e,0.)), gn(q2 + vec2(0.,e)) - gn(q2 - vec2(0.,e))) * 0.25 * exp(-dist/70.0);
            g *= (0.12 + 3.0*exp(-dist/140.0)) * mix(0.18, 1.0, smoothstep(0.03, 0.3, V.y));   // eau vue en rasant : vagues adoucies (sinon le reflet clignote)                                                      // pentes réalistes de près, eau plus lisse au loin (évite le scintillement)
            vec3 N = normalize(vec3(-g.x, 1.0, -g.y));
            vec3 R = reflect(-V, N); R.y = abs(R.y);
            vec3 sky = mix(hor, mid, smoothstep(0.0, 0.10, R.y)); sky = mix(sky, top, smoothstep(0.06, 0.4, R.y)) * 0.85;
            float sd = max(dot(R, sunDir), 0.0);
            sky += sunRaw * (pow(sd, 7.0)*0.35 + pow(sd, 60.0)*0.8);
            vec3 Nf = normalize(mix(vec3(0.0,1.0,0.0), N, 0.4));                                 // normale lissée pour le Fresnel
            float fres = min(0.02 + 0.98*pow(1.0 - max(dot(Nf, V), 0.0), 5.0), 0.62);
            vec3 body = deep * (0.45 + 0.55*max(dot(N, sunDir), 0.0)) + deep * amb * 0.25;
            body += vec3(0.05, 0.22, 0.20) * sunRaw * pow(max(dot(-V, sunDir), 0.0), 4.0) * max(g.x + g.y, 0.0) * 0.6;  // lumière à travers la crête des vagues
            vec3 col = mix(body, sky, fres);
            float nearW = exp(-dist/220.0);
            col += sunRaw * (pow(sd, 1100.0) * 40.0 * nearW + pow(sd, 240.0) * 2.5 * (0.3 + 0.7*nearW));   // éclats du soleil, adoucis au loin
            col += sunRaw * pow(max(dot(reflect(-V, vec3(0.0,1.0,0.0)), sunDir), 0.0), 60.0) * 0.9 * (1.0 - nearW);  // au loin : traînée continue vers le soleil
            float foam = smoothstep(0.62, 0.9, gn(p*0.6 + time*0.15) * gn(p*1.7 - time*0.2) * 2.2) * exp(-dist/120.0) * 0.25;
            col = mix(col, vec3(0.9)*(amb*0.5 + sunCol*0.6), foam);
            col = mix(col, fogColor, 1.0 - exp(-fogDensity*fogDensity*dist*dist));
            gl_FragColor = vec4(col, 1.0);
            #include <tonemapping_fragment>
            #include <colorspace_fragment>
          }`,
      });
      const wsz = 2 * (R + 3000);
      const water = add(new THREE.PlaneGeometry(wsz, wsz, 40, 40), wm); water.rotation.x = -Math.PI / 2; water.position.set(cx, mer.niveau, cz); water.frustumCulled = false;
      timed.push(wm);
    }
    // montagnes à l'horizon
    {
      const pos = [], idx = [], col = [], seg = 120;
      const far = new THREE.Color(th.hill).lerp(FOG, 0.45), base = new THREE.Color(th.ground).lerp(FOG, 0.55);
      for (let ring = 0; ring < 2; ring++) {
        const rad = R + 900 + ring * 500, o = pos.length / 3;
        for (let i = 0; i <= seg; i++) {
          const a = i / seg * Math.PI * 2, x = cx + Math.cos(a) * rad, z = cz + Math.sin(a) * rad;
          const h = (th.hillH || 120) * (0.35 + 0.65 * Math.abs(Math.sin(a * 3.1 + ring * 2) * Math.sin(a * 7.3 + 1.3 + ring) + 0.35 * Math.sin(a * 17.0))) * (1 + ring * 0.9);
          pos.push(x, -2, z, x, h + amp, z);
          const c1 = ring ? far.clone().lerp(FOG, 0.35) : far; col.push(base.r, base.g, base.b, c1.r, c1.g, c1.b);
          if (i < seg) { const k2 = o + i * 2; idx.push(k2, k2 + 1, k2 + 2, k2 + 1, k2 + 3, k2 + 2); }
        }
      }
      const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3)); g.setIndex(idx);
      const m = add(g, new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.DoubleSide, fog: false })); m.frustumCulled = false;
    }

    // --- végétation ---
    // feuillage : des cartes couvertes de feuilles, éclairées comme une boule (normales sphériques), qui bougent au vent
    const leafTex = (pine, snow) => canvasTex(256, 256, (g) => {
      g.clearRect(0, 0, 256, 256);
      if (pine) {
        for (let b = 0; b < 9; b++) { const x0 = rr(30, 226), y0 = rr(10, 60), len = rr(120, 190), ang = rr(-0.5, 0.5);
          for (let i = 0; i < 46; i++) { const t = i / 46, x = x0 + Math.sin(ang) * len * t, y = y0 + Math.cos(ang) * len * t, w = (1 - t * 0.6) * 26;
            g.strokeStyle = snow && rnd() < 0.3 ? '#eef4fa' : `rgb(${20 + rnd() * 25 | 0},${60 + rnd() * 50 | 0},${35 + rnd() * 25 | 0})`; g.lineWidth = 2.2;
            g.beginPath(); g.moveTo(x, y); g.lineTo(x - w, y + w * 0.7); g.moveTo(x, y); g.lineTo(x + w, y + w * 0.7); g.stroke(); } }
      } else {
        for (let i = 0; i < 150; i++) { const x = 128 + (rnd() - 0.5) * 215 * Math.sqrt(rnd()) * 1.1, y = 128 + (rnd() - 0.5) * 215 * Math.sqrt(rnd()) * 1.1, s = rr(9, 19);
          g.save(); g.translate(x, y); g.rotate(rr(0, 6.28));
          g.fillStyle = `rgb(${40 + rnd() * 55 | 0},${95 + rnd() * 80 | 0},${25 + rnd() * 35 | 0})`; g.beginPath(); g.ellipse(0, 0, s, s * 0.5, 0, 0, 7); g.fill();
          g.strokeStyle = 'rgba(0,0,0,0.25)'; g.lineWidth = 1; g.beginPath(); g.moveTo(-s, 0); g.lineTo(s, 0); g.stroke(); g.restore(); }
      }
    }, { wrapS: THREE.ClampToEdgeWrapping, wrapT: THREE.ClampToEdgeWrapping });
    const foliageMat = (tex, tint) => {
      const m = new THREE.ShaderMaterial({
        side: THREE.DoubleSide, uniforms: Object.assign(gUni(), { map: { value: tex }, tint: { value: new THREE.Color(tint) } }),
        vertexShader: `attribute float aAO; uniform float time; varying vec2 vU; varying vec3 vN; varying vec3 vWp; varying float vAO; varying float vK;
          void main(){ vU = uv; vAO = aAO; vec4 w = modelMatrix * instanceMatrix * vec4(position, 1.0);
            vK = fract(sin(dot(instanceMatrix[3].xz, vec2(12.9898, 78.233))) * 43758.5453);
            float sway = sin(time*1.3 + w.x*0.31 + w.y*0.7 + vK*6.0) + 0.5*sin(time*2.9 + w.z*0.5);
            w.xz += sway * 0.035 * position.y * (0.5 + aAO);
            vN = normalize(mat3(modelMatrix) * mat3(instanceMatrix) * normal); vWp = w.xyz; gl_Position = projectionMatrix * viewMatrix * w; }`,
        fragmentShader: `uniform sampler2D map; uniform vec3 tint; uniform vec3 sunDir; uniform vec3 sunCol; uniform vec3 amb; uniform vec3 fogColor; uniform float fogDensity;
          varying vec2 vU; varying vec3 vN; varying vec3 vWp; varying float vAO; varying float vK;
          void main(){ vec4 t = texture2D(map, vU); if (t.a < 0.5) discard;
            vec3 toCam = cameraPosition - vWp; float dist = length(toCam); vec3 V = toCam/dist;
            vec3 alb = t.rgb * tint * (0.8 + 0.45*vK);
            float l = dot(normalize(vN), sunDir)*0.5 + 0.5; l *= l;
            float back = pow(max(dot(-V, sunDir), 0.0), 3.0) * (1.0 - l);          // lumière qui traverse les feuilles à contre-jour
            vec3 col = alb * (amb*0.5 + sunCol*l*1.5) * mix(0.35, 1.0, vAO) + alb * vec3(1.3,1.5,0.6) * sunCol * back * 0.5;
            col = mix(col, fogColor, 1.0 - exp(-fogDensity*fogDensity*dist*dist));
            gl_FragColor = vec4(col, 1.0);
            #include <tonemapping_fragment>
            #include <colorspace_fragment>
          }`,
      });
      timed.push(m); disposables.push(m); return m;
    };
    const card = (w, h, cxl, cyl, czl, rx, ry, rz, center, radius) => {
      const g = new THREE.PlaneGeometry(w, h); g.rotateZ(rz); g.rotateX(rx); g.rotateY(ry); g.translate(cxl, cyl, czl);
      const p = g.attributes.position, n = g.attributes.normal, ao = new Float32Array(p.count);
      for (let i = 0; i < p.count; i++) { const dx = p.getX(i) - center[0], dy = p.getY(i) - center[1], dz = p.getZ(i) - center[2], l = Math.hypot(dx, dy, dz) || 1; n.setXYZ(i, dx / l, dy / l, dz / l); ao[i] = Math.min(1, l / radius); }
      g.setAttribute('aAO', new THREE.BufferAttribute(ao, 1)); return g;
    };
    const canopy = {
      feuillu: () => { const gs = [];
        for (const [bx, by, bz, br] of [[0, 5.8, 0, 2.7], [1.5, 4.7, 0.7, 2.0], [-1.3, 5.0, -0.7, 1.9], [0.2, 4.4, -1.4, 1.7]])
          for (let i = 0; i < 8; i++) { const a = rr(0, 6.28), e = rr(-0.5, 1.2), rad = br * rr(0.35, 0.8);
            gs.push(card(br * 1.5, br * 1.5, bx + Math.cos(a) * Math.cos(e) * rad, by + Math.sin(e) * rad, bz + Math.sin(a) * Math.cos(e) * rad, rr(-0.9, 0.9), rr(0, 6.28), rr(0, 6.28), [bx * 0.4, 5.2, bz * 0.4], 3.6)); }
        return mergeGeometries(gs); },
      palmier: () => { const gs = [];
        for (let i = 0; i < 13; i++) { const a = i / 13 * 6.283 + rr(-0.15, 0.15);
          gs.push(card(1.25, 4.8, 1.3 + Math.sin(a) * 1.9, 9.6 + rr(-0.3, 0.3), Math.cos(a) * 1.9, -1.1 - rr(0, 0.45), a, 0, [1.3, 10.3, 0], 3.2)); }
        return mergeGeometries(gs); },
      sapin: () => { const gs = [];
        for (let t = 0; t < 6; t++) { const y = 2.0 + t * 1.3, rad = 2.7 * (1 - t / 6.6);
          for (let k = 0; k < 6; k++) { const a = k / 6 * 6.283 + t * 0.5; gs.push(card(rad * 1.5, 2.3, Math.sin(a) * rad * 0.45, y, Math.cos(a) * rad * 0.45, -0.95, a, 0, [0, y + 0.8, 0], rad * 1.1)); } }
        return mergeGeometries(gs); },
    };
    const barkTex = canvasTex(64, 128, (g) => { g.fillStyle = '#4a3828'; g.fillRect(0, 0, 64, 128); for (let i = 0; i < 90; i++) { g.strokeStyle = rnd() < 0.5 ? '#2c2018' : '#6a5440'; g.lineWidth = rr(1, 3); const x = rr(0, 64); g.beginPath(); g.moveTo(x, rr(0, 128)); g.lineTo(x + rr(-3, 3), rr(0, 128)); g.stroke(); } });
    const trunkMat = new THREE.MeshStandardMaterial({ map: barkTex, roughness: 1, metalness: 0, envMapIntensity: 0.2 });
    const rockMat = noisy(new THREE.MeshStandardMaterial({ vertexColors: true, flatShading: true, roughness: 0.95, metalness: 0, envMapIntensity: 0.25 }), 1.7, 0.45);
    disposables.push(barkTex, trunkMat, rockMat);
    const solid = {
      feuillu: () => mergeGeometries([new THREE.CylinderGeometry(0.22, 0.42, 4.6, 7).translate(0, 2.3, 0), new THREE.CylinderGeometry(0.1, 0.18, 2.2, 5).rotateZ(-0.7).translate(0.8, 4.0, 0.2), new THREE.CylinderGeometry(0.1, 0.17, 2.0, 5).rotateZ(0.75).translate(-0.75, 4.1, -0.2)]),
      sapin: () => new THREE.CylinderGeometry(0.12, 0.34, 9.4, 6).translate(0, 4.7, 0),
      palmier: () => { const g = new THREE.CylinderGeometry(0.17, 0.3, 10, 8, 10); const p = g.attributes.position;
        for (let i = 0; i < p.count; i++) { const t = (p.getY(i) + 5) / 10; p.setX(i, p.getX(i) + t * t * 1.3); const r2 = 1 + 0.12 * Math.abs(Math.sin(t * 60)); p.setX(i, p.getX(i) * 1); p.setZ(i, p.getZ(i) * r2); }
        return g.translate(0, 5, 0); },
      cactus: () => mergeGeometries([colored(new THREE.CylinderGeometry(0.42, 0.48, 5, 10).translate(0, 2.5, 0), 0x3d7a3a), colored(new THREE.SphereGeometry(0.42, 10, 6).translate(0, 5, 0), 0x468a40),
        colored(new THREE.CylinderGeometry(0.28, 0.28, 1.6, 8).rotateZ(Math.PI / 2).translate(0.9, 2.6, 0), 0x3d7a3a), colored(new THREE.CylinderGeometry(0.28, 0.28, 1.8, 8).translate(1.6, 3.4, 0), 0x468a40),
        colored(new THREE.CylinderGeometry(0.26, 0.26, 1.3, 8).rotateZ(Math.PI / 2).translate(-0.8, 1.8, 0), 0x3d7a3a), colored(new THREE.CylinderGeometry(0.26, 0.26, 1.4, 8).translate(-1.4, 2.4, 0), 0x468a40)]),
      rocher: () => { const g = new THREE.IcosahedronGeometry(2, 1); const p = g.attributes.position; for (let i = 0; i < p.count; i++) { const k = 0.75 + 0.5 * Math.abs(Math.sin(p.getX(i) * 3.1 + p.getY(i) * 2.3 + p.getZ(i) * 4.7)); p.setXYZ(i, p.getX(i) * k * 1.4, p.getY(i) * k * 0.9, p.getZ(i) * k * 1.1); } return colored(g.translate(0, 0.8, 0), th.rock || 0x777a80); },
    };
    const shTex = canvasTex(64, 64, (g) => { const gr = g.createRadialGradient(32, 32, 2, 32, 32, 32); gr.addColorStop(0, 'rgba(0,0,0,0.75)'); gr.addColorStop(0.6, 'rgba(0,0,0,0.45)'); gr.addColorStop(1, 'rgba(0,0,0,0)'); g.fillStyle = gr; g.fillRect(0, 0, 64, 64); });
    const shGeo = new THREE.PlaneGeometry(1, 1).rotateX(-Math.PI / 2), shMat = new THREE.MeshBasicMaterial({ map: shTex, transparent: true, depthWrite: false, opacity: th.ombre ?? 0.6 });
    disposables.push(shTex, shGeo, shMat);
    const d = new THREE.Object3D(), sunAng = Math.atan2(sunV.x, sunV.z), sunLen = Math.min(3.2, 1 / Math.max(0.2, sunV.y / Math.hypot(sunV.x, sunV.z)));
    for (const [kind, count, near, farD, s0, s1] of th.flore) {
      const spots = [];
      for (let t = 0; t < count * 3 && spots.length < count; t++) {
        const i = Math.floor(rnd() * T.N), side = rnd() < 0.5 ? -1 : 1, off = side * (HALF + near + Math.pow(rnd(), 1.6) * farD);
        const x = T.px[i] - T.tz[i] * off, z = T.pz[i] + T.tx[i] * off;
        if (!farFromTrack(x, z, HALF + near - 1)) continue;
        const hy = heightAt(x, z); if (mer && hy < (kind === 'rocher' ? -3 : 0.15)) continue;   // pas d'arbre dans l'eau
        spots.push([x, z, hy, rr(s0, s1), rr(0, 6.28)]);
      }
      const place = (geo, mat) => { geo.computeVertexNormals && !geo.attributes.aAO && geo.computeVertexNormals();
        const im = new THREE.InstancedMesh(geo, mat, spots.length);
        spots.forEach(([x, z, y, s, a], i) => { d.position.set(x, y - 0.15, z); d.rotation.set(0, a, 0); d.scale.set(s, s, s); d.updateMatrix(); im.setMatrixAt(i, d.matrix); });
        group.add(im); disposables.push(geo); return im; };
      place(solid[kind](), kind === 'feuillu' || kind === 'sapin' || kind === 'palmier' ? trunkMat : rockMat);
      if (canopy[kind]) { const tex = leafTex(kind !== 'feuillu', kind === 'sapin' && th.snowTop); disposables.push(tex); place(canopy[kind](), foliageMat(tex, kind === 'sapin' ? (th.snowTop ? 0xdfe6e8 : 0xd0e0c8) : kind === 'palmier' ? 0xc8e8a0 : th.feuille ? new THREE.Color().fromArray(th.feuille) : 0xffffff)); }
      // ombres portées au sol, allongées à l'opposé du soleil
      const sh = new THREE.InstancedMesh(shGeo, shMat, spots.length); sh.layers.set(1); sh.renderOrder = 1;
      const w0 = kind === 'cactus' ? 2.2 : kind === 'rocher' ? 5.5 : kind === 'palmier' ? 6 : 7.5;
      spots.forEach(([x, z, y, s], i) => { const L = w0 * s * (1 + sunLen * 0.45), ox = -Math.sin(sunAng) * L * 0.3, oz = -Math.cos(sunAng) * L * 0.3;
        d.position.set(x + ox, heightAt(x + ox, z + oz) + 0.0, z + oz); d.rotation.set(0, sunAng, 0); d.scale.set(w0 * s, 1, L); d.updateMatrix(); sh.setMatrixAt(i, d.matrix); });
      group.add(sh);
    }

    // --- bords de route ---
    if (T.libre) {
      const wood = [];
      for (let i = 0; i < T.N; i += 3) for (const s of [-1, 1]) { const o = s * (HALF + 9); wood.push(box(0.16, 1.2, 0.16, T.px[i] - T.tz[i] * o, 0.6, T.pz[i] + T.tx[i] * o)); }
      for (let i = 0; i < T.N; i += 24) { const o = -(HALF + 6); wood.push(box(0.3, 9, 0.3, T.px[i] - T.tz[i] * o, 4.5, T.pz[i] + T.tx[i] * o), box(2.2, 0.16, 0.16, T.px[i] - T.tz[i] * o, 8.4, T.pz[i] + T.tx[i] * o, Math.atan2(-T.tx[i], -T.tz[i]))); }
      add(mergeGeometries(wood), noisy(new THREE.MeshStandardMaterial({ color: 0x6a5238, roughness: 1 }), 9, 0.4));
      for (const s of [-1, 1]) for (const y of [0.55, 0.95]) add(wall(T, s * (HALF + 9), y, y + 0.1, 1), new THREE.MeshStandardMaterial({ color: 0x6a5540, roughness: 1, side: THREE.DoubleSide }));
    } else {
      const rail = new THREE.MeshStandardMaterial({ color: 0xb8bcc4, metalness: 0.85, roughness: 0.35, side: THREE.DoubleSide, envMapIntensity: 0.8 });
      const posts = [];
      for (const s of [-1, 1]) {
        add(wall(T, s * (HALF + 0.05), 0.38, 0.86, 1), rail);
        add(flat(T, s * (HALF + 0.05), s * (HALF + 0.2), 0.86, 1), rail);
        for (let i = 0; i < T.N; i += 2) { const o = s * (HALF + 0.22); posts.push(box(0.14, 0.86, 0.14, T.px[i] - T.tz[i] * o, 0.43, T.pz[i] + T.tx[i] * o)); }
      }
      add(mergeGeometries(posts), new THREE.MeshStandardMaterial({ color: 0x70747c, metalness: 0.8, roughness: 0.5 }));
      for (let i = 30; i < T.N; i += 46) { const s = rnd() < 0.5 ? -1 : 1, o = s * (HALF + 2.2); adQuads.push({ cx: T.px[i] - T.tz[i] * o, cz: T.pz[i] + T.tx[i] * o, y: 0.5, pw: 9, ph: 2.2, ang: Math.atan2(-T.tx[i], -T.tz[i]) + (s > 0 ? -Math.PI / 2 : Math.PI / 2), cell: 1 + Math.floor(rnd() * 3) }); }
    }
    flushAds(0.9);

    // --- herbe : brins individuels qui suivent la caméra, en deux couches (tapis ras + herbes hautes) ---
    if (th.grass && q.grass > 0) {
      // véhicules qui couchent l'herbe : push = (x, z, rayon, force) autour de chaque véhicule ; trail = (x, z, direction × force) le long du sillage
      bend = { push: Array.from({ length: 6 }, () => new THREE.Vector4(0, 0, 1, 0)), trail: Array.from({ length: 20 }, () => new THREE.Vector4(0, 0, 0, 0)), focus: new THREE.Vector3(0, 0, 0) };
      const layer = (n, S, hgt, wid, tall) => {
        const g = new THREE.InstancedBufferGeometry();
        const base = mergeGeometries([new THREE.PlaneGeometry(wid, 1, 1, 3).translate(0, 0.5, 0), new THREE.PlaneGeometry(wid, 1, 1, 3).translate(0, 0.5, 0).rotateY(Math.PI / 2 * 0.9)]);
        g.index = base.index; g.setAttribute('position', base.attributes.position); g.setAttribute('uv', base.attributes.uv);
        const off = new Float32Array(n * 4);
        for (let i = 0; i < n; i++) { off[i * 4] = rr(-S / 2, S / 2); off[i * 4 + 1] = rr(-S / 2, S / 2); off[i * 4 + 2] = 0; off[i * 4 + 3] = rr(0, 6.28); }
        const attr = new THREE.InstancedBufferAttribute(off, 4); attr.setUsage(THREE.DynamicDrawUsage); g.setAttribute('aOff', attr);
        g.instanceCount = n;
        const mat = new THREE.ShaderMaterial({
          side: THREE.DoubleSide, uniforms: Object.assign(gUni(), { hgt: { value: hgt }, tall: { value: tall }, half_: { value: S / 2 }, push: { value: bend.push }, trail: { value: bend.trail }, focus: { value: bend.focus } }),
          vertexShader: GROUND_GLSL + `attribute vec4 aOff; uniform float hgt; uniform float tall; uniform float half_; uniform vec4 push[6]; uniform vec4 trail[20]; uniform vec3 focus;
            varying vec2 vU; varying float vD; varying float vK; varying float vField; varying vec3 vBase; varying vec3 vWp; varying float vCs;
            void main(){ vU = uv; float c = cos(aOff.w), sn = sin(aOff.w);
              vK = gh(aOff.xy*0.37 + 1.3);
              vBase = groundBase(aOff.xy, vField);
              vCs = smoothstep(0.48, 0.72, gf(aOff.xy*0.0045 + vec2(time*0.012, time*0.007)));
              float edge = smoothstep(half_, half_ - 7.0, distance(aOff.xy, cameraPosition.xz));      // disparaît en douceur au bord du tapis
              edge *= smoothstep(0.7, 2.2, distance(aOff.xy, cameraPosition.xz));                       // pas de brin collé à l'objectif
              float h = hgt * mix(0.65 + 0.7*vK, 0.92 + 0.16*vK, vField*tall) * mix(1.0, mix(1.0, 0.78, tall), vField) * edge * aOff.z;   // le blé mûr arrive à hauteur de capot
              // tiges couchées : poussées sur les côtés par la carrosserie, puis laissées à plat dans le sillage
              vec2 lay = vec2(0.0); float flat_ = 0.0;
              if (focus.z > 0.5 && distance(aOff.xy, focus.xy) < 60.0) {
                for (int i = 0; i < 6; i++) { vec2 d = aOff.xy - push[i].xy; float l = length(d); float k = push[i].w * smoothstep(push[i].z, push[i].z*0.4, l); lay += d / max(l, 0.05) * k; flat_ = max(flat_, k); }
                for (int i = 0; i < 20; i++) { vec2 d = aOff.xy - trail[i].xy; float st = length(trail[i].zw); if (st < 0.01) continue; float k = st * smoothstep(1.75, 0.75, length(d)); lay += trail[i].zw / st * k * 1.3 + d * 0.25 * k; flat_ = max(flat_, k); }
              }
              float ll = length(lay); if (ll > 1.0) lay /= ll; flat_ = min(flat_, 1.0);
              float h0 = h; h *= 1.0 - 0.8*flat_;
              vec3 p = position; p.y *= h;
              p = vec3(p.x*c - p.z*sn, p.y, p.x*sn + p.z*c);
              vec3 w = vec3(aOff.x, 0.0, aOff.y) + p;
              // vent : grandes rafales qui traversent le champ + frémissement
              float gust = gn(w.xz*0.045 + vec2(time*0.55, time*0.3)) * 2.0 - 0.6;
              float sway = gust + 0.35*sin(time*3.3 + w.x*1.1 + w.z*0.9 + vK*6.0);
              float bend = uv.y*uv.y * h * (1.0 + 0.5*vField*tall) * (1.0 - flat_);
              w.x += sway * 0.22 * bend; w.z += sway * 0.13 * bend; w.y -= abs(sway) * 0.05 * bend;
              w.y -= vField * tall * pow(uv.y, 5.0) * h * 0.10; w.x += vField * tall * pow(uv.y, 5.0) * h * 0.08 * (vK - 0.5) * 2.0;   // l'épi, lourd, fait pencher le haut de la tige
              w.xz += lay * uv.y * h0 * 0.85;                                                                                          // la tige se couche
              vWp = w; vec4 mv = viewMatrix * vec4(w, 1.0); vD = -mv.z; gl_Position = projectionMatrix * mv; }`,
          fragmentShader: `uniform vec3 sunDir; uniform vec3 sunCol; uniform vec3 amb; uniform vec3 fogColor; uniform float fogDensity;
            varying vec2 vU; varying float vD; varying float vK; varying float vField; varying vec3 vBase; varying vec3 vWp; varying float vCs;
            float gh(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
            void main(){
              // chaque carte porte 6 brins : hauteur, courbure et teinte propres à chacun
              float nb = 6.0, lean = (gh(vec2(vK*91.0, 3.0)) - 0.5) * 0.6;
              float u = vU.x - lean*vU.y*vU.y; if (u < 0.0 || u > 1.0) discard;
              float i = floor(u*nb), f = fract(u*nb) - 0.5;
              float seedB = gh(vec2(i*3.7 + vK*91.0, 5.3));
              bool ear = vField > 0.5 && seedB < 0.6;                 // épi de blé
              bool flower = vField < 0.35 && seedB > 0.92;            // fleur des prés
              float hi = 0.45 + 0.55*gh(vec2(i*7.31 + vK*57.0, 1.7)); if (ear) hi = max(hi, 0.82);
              float t = vU.y / hi;
              float wd = 0.47*(1.0 - pow(t, 1.7));
              if (ear && t > 0.6) { float e = (t - 0.6) / 0.28;
                wd = e < 1.0 ? 0.3*(0.7 + 0.3*abs(sin(e*17.0 + abs(f)*5.0)))*smoothstep(0.0, 0.12, e)*(1.0 - 0.45*smoothstep(0.7, 1.0, e))   // grains en chevrons
                  : (min(min(abs(f), abs(f - (t - 0.88)*2.6)), abs(f + (t - 0.88)*2.6)) < 0.045 ? 0.5 : 0.0); }                              // barbes : trois fils fins
              if (flower && t > 0.86) wd = 0.5;
              if (t > 1.0 || abs(f) > wd) discard;
              vec3 tip = mix(vBase*1.45 + vec3(0.03,0.06,0.0), vec3(0.62,0.52,0.26), vField*0.9);
              vec3 alb = mix(vBase*0.5, tip, t) * (0.78 + 0.44*gh(vec2(i, vK*13.0)));
              alb *= 1.0 - 0.3*abs(f)*2.0;                                                        // nervure centrale plus claire
              if (ear && t > 0.6) alb = t > 0.88 ? vec3(0.70, 0.60, 0.32) : vec3(0.60, 0.46, 0.19) * (0.7 + 0.45*abs(sin((t - 0.6)*60.0 + abs(f)*5.0)));   // grains dorés, barbes pâles
              if (flower && t > 0.86) { float fc = gh(vec2(seedB*31.0, i)); alb = fc < 0.4 ? vec3(0.85,0.06,0.04) : fc < 0.65 ? vec3(0.95,0.93,0.86) : fc < 0.85 ? vec3(0.95,0.78,0.12) : vec3(0.55,0.36,0.85); }
              vec3 V = normalize(cameraPosition - vWp);
              float back = pow(max(dot(-V, sunDir), 0.0), 3.0);                                    // contre-jour : la lumière traverse les brins
              float sheen = pow(max(dot(reflect(-sunDir, vec3(0.0,1.0,0.0)), V), 0.0), 9.0);       // reflet du soleil sur les brins
              vec3 col = alb * (amb*0.55 + sunCol*(0.5 + 0.6*t)*(1.0 - 0.45*vCs)) + tip*sunCol*(back*0.6 + sheen*0.25)*t;
              col = mix(col, fogColor, 1.0 - exp(-fogDensity*fogDensity*vD*vD));
              gl_FragColor = vec4(col, 1.0);
              #include <tonemapping_fragment>
              #include <colorspace_fragment>
            }`,
        });
        const mesh = new THREE.Mesh(g, mat); mesh.frustumCulled = false; mesh.layers.set(1); group.add(mesh); disposables.push(g, mat, base); timed.push(mat);
        let first = true;
        updaters.push((cam, dt, time, hint) => {
          const fx = cam.position.x, fz = cam.position.z; let dirty = false;
          for (let i = 0; i < n; i++) {
            const k4 = i * 4; let x = off[k4], z = off[k4 + 1], moved = first;
            while (x - fx > S / 2) { x -= S; moved = true; } while (x - fx < -S / 2) { x += S; moved = true; }
            while (z - fz > S / 2) { z -= S; moved = true; } while (z - fz < -S / 2) { z += S; moved = true; }
            if (!moved) continue;
            const j = T.nearest(x, z, hint, 30), lat = Math.abs(T.lateral(x, z, j));
            off[k4] = x; off[k4 + 1] = z; off[k4 + 2] = lat < HALF + (T.libre ? 1.3 : 1.0) ? 0 : 1; dirty = true;
          }
          first = false;
          if (dirty) attr.needsUpdate = true;
        });
      };
      const dens = th.grass.dens || 1;
      layer(Math.floor(q.grass * 2 * dens), 46, (th.grass.h || 1.3) * 0.34, 1.5, 0);   // tapis ras et dense
      layer(Math.floor(q.grass * dens), 72, th.grass.h || 1.3, 1.9, 1);                 // herbes hautes
    }
    group.add(new THREE.HemisphereLight(new THREE.Color().fromArray(th.sky[1]), gcol, (th.ambI ?? 1.5) * (sombre ? 1 : 0.8)));
    const sunL = new THREE.DirectionalLight(new THREE.Color().fromArray(th.sun.col), (th.sunI ?? 2.6) * (sombre ? 1 : 1.2)); sunL.position.copy(sunV).multiplyScalar(100); group.add(sunL);
  }

  // --- route réfléchissante ---
  const curbHex = stripeCol;
  const roadGeo = strip(T, (j, s) => {
    const o = HALF + (T.libre ? 2.4 : 0.7);
    return [T.px[j] + T.tz[j] * o, -(T.pz[j] - T.tx[j] * o), 0, T.px[j] - T.tz[j] * o, -(T.pz[j] + T.tx[j] * o), 0, -o, s, o, s];
  });
  const sunDir = th.sun ? new THREE.Vector3().fromArray(th.sun.dir).normalize() : new THREE.Vector3(0, 1, 0);
  const road = new Reflector(roadGeo, {
    textureWidth: 512, textureHeight: 256, clipBias: 0.003, multisample: 0,
    shader: {
      uniforms: { color: { value: null }, tDiffuse: { value: null }, textureMatrix: { value: null }, fogColor: { value: FOG }, fogDensity: { value: th.dens }, half_: { value: HALF },
        laneW: { value: T.libre ? 40 : 4.5 }, curbOn: { value: T.libre ? 0 : 1 }, wet: { value: th.rain === true ? 1 : th.wet ?? 1 }, day: { value: city && !th.jourVille ? 0 : 1 },
        time: { value: 0 }, rainK: { value: th.rain === true ? (th.orage ? 1 : 0.65) : 0 }, dry: { value: !th.rain && !th.nuit && (!city || th.jourVille) ? 1 : 0 },
        amb: { value: city && !th.jourVille ? new THREE.Color(th.c).lerp(new THREE.Color(0.55, 0.5, 0.9), 0.6).multiplyScalar(1.6) : new THREE.Color().fromArray(th.sky[1]).multiplyScalar(0.9) },
        sunDir: { value: sunDir }, sunCol: { value: th.sun ? new THREE.Color().fromArray(th.sun.col).multiplyScalar(th.sun.road ?? 1) : new THREE.Color(0, 0, 0) },
        offCol: { value: city ? new THREE.Color(0.02, 0.02, 0.03) : new THREE.Color(th.ground).multiplyScalar(0.5) },
        curbCol: { value: curbHex.clone().multiplyScalar(city ? 0.45 : 0.7) } },
      vertexShader: ROAD_VERT, fragmentShader: `#define TAPS ${q.taps}\n` + ROAD_FRAG,
    },
  });
  road.rotation.x = -Math.PI / 2; road.frustumCulled = false; road.renderOrder = 5;
  group.add(road); disposables.push(roadGeo, road.material);

  // --- pluie ou neige : gouttes calculées par la carte graphique ---
  // Chaque goutte est un petit rectangle tourné vers la caméra et étiré selon sa vitesse relative à la voiture
  // (flou de mouvement) : à l'arrêt elles tombent, à 200 km/h elles filent vers la caméra. Elles vivent dans une boîte
  // de 64 m autour de la caméra (repliée sur elle-même), s'estompent de près et de loin, et s'illuminent sous les éclairs.
  let rain = null;
  if (th.rain) {
    const snow = th.rain === 'neige', heavy = !!th.orage, n = Math.round((snow ? (heavy ? 5000 : 2600) : heavy ? 9000 : 4200) * (q.rainK ?? 1));
    const seed = new Float32Array(n * 4 * 4), corner = new Float32Array(n * 4 * 2), idx = new Uint32Array(n * 6);
    for (let i = 0; i < n; i++) {
      const a = rnd(), b = rnd(), c = rnd(), d = rnd();
      for (let k = 0; k < 4; k++) { seed.set([a, b, c, d], (i * 4 + k) * 4); corner.set([k & 1 ? 1 : -1, k >> 1], (i * 4 + k) * 2); }
      idx.set([i * 4, i * 4 + 1, i * 4 + 2, i * 4 + 1, i * 4 + 3, i * 4 + 2], i * 6);
    }
    const g = new THREE.BufferGeometry(); g.setAttribute('seed', new THREE.BufferAttribute(seed, 4)); g.setAttribute('corner', new THREE.BufferAttribute(corner, 2)); g.setIndex(new THREE.BufferAttribute(idx, 1));
    g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(n * 4 * 3), 3));
    const tint = city ? new THREE.Color(th.fog).lerp(new THREE.Color(th.lamp || 0xffffff), 0.55).multiplyScalar(1.5) : new THREE.Color().fromArray(th.sky[2]).lerp(new THREE.Color(0xffffff), 0.4);
    const mat = new THREE.ShaderMaterial({
      transparent: true, depthWrite: false, fog: false, side: THREE.DoubleSide,
      uniforms: { time: { value: 0 }, wind: { value: new THREE.Vector2() }, camVel: { value: new THREE.Vector3() }, fall: { value: snow ? (heavy ? 6 : 3.2) : heavy ? 34 : 26 },
        box: { value: 64 }, hgt: { value: 26 }, snow: { value: snow ? 1 : 0 }, col: { value: snow ? new THREE.Color(1.1, 1.1, 1.15) : tint }, alpha: { value: snow ? 0.85 : heavy ? 0.5 : 0.36 }, flash: { value: 0 } },
      vertexShader: `attribute vec4 seed; attribute vec2 corner; uniform float time; uniform vec2 wind; uniform vec3 camVel; uniform float fall; uniform float box; uniform float hgt; uniform float snow; uniform float flash;
        varying vec2 vC; varying float vA;
        void main(){
          float sp = fall * (0.75 + 0.5*seed.w);
          vec3 v = vec3(wind.x, -sp, wind.y);
          if (snow > 0.5) { v.x += sin(time*1.3 + seed.w*40.0) * 0.9; v.z += cos(time*1.1 + seed.w*23.0) * 0.9; }
          vec3 p;
          p.y = hgt - mod(seed.y*hgt + sp*time, hgt);
          p.xz = seed.xz*box + v.xz*time;
          p.xz = cameraPosition.xz + mod(p.xz - cameraPosition.xz + box*0.5, box) - box*0.5;
          vec3 rel = v - camVel;                                   // vitesse vue depuis la caméra
          float rl = length(rel) + 1e-3; vec3 dir = rel / rl;
          float L = snow > 0.5 ? 0.06 : clamp(rl * 0.028, 0.25, 4.5);   // flou de mouvement (exposition ~1/35 s)
          vec3 toCam = cameraPosition - p; float dist = length(toCam);
          vec3 side = normalize(cross(dir, toCam / dist));
          float w = snow > 0.5 ? 0.035 + 0.03*seed.w : max(0.006, dist*0.0016);
          vec3 wp = p - dir * L * corner.y + side * w * corner.x;
          if (snow > 0.5) wp = p + (side * corner.x + normalize(cross(side, toCam)) * (corner.y*2.0 - 1.0)) * w;
          vC = vec2(corner.x, corner.y);
          vA = smoothstep(0.6, 3.0, dist) * (1.0 - smoothstep(box*0.3, box*0.5, dist)) * (snow > 0.5 ? 1.0 : clamp(0.006 / w * 3.0, 0.25, 1.0)) * (0.6 + 0.4*seed.w) * (1.0 + flash*2.5);
          gl_Position = projectionMatrix * viewMatrix * vec4(wp, 1.0);
        }`,
      fragmentShader: `uniform vec3 col; uniform float alpha; uniform float snow; varying vec2 vC; varying float vA;
        void main(){
          float a;
          if (snow > 0.5) { vec2 q = vec2(vC.x, vC.y*2.0 - 1.0); a = smoothstep(1.0, 0.2, length(q)); }
          else a = (1.0 - vC.x*vC.x) * mix(1.0, 0.15, vC.y);       // tête de la goutte plus nette, traîne qui s'efface
          gl_FragColor = vec4(col, a * alpha * vA);
          #include <colorspace_fragment>
        }`,
    });
    rain = new THREE.Mesh(g, mat);
    rain.frustumCulled = false; rain.layers.set(1); rain.renderOrder = 20;
    group.add(rain); disposables.push(g, mat);
  }
  // cam : caméra ; vx, vz : vitesse de la voiture suivie ; wx, wz : vent
  const updateRain = (cam, dt, vx, vz, wx = 0, wz = 0) => {
    road.material.uniforms.time.value += dt;
    if (!rain || !rain.visible) return;
    const u = rain.material.uniforms; u.time.value += dt; u.wind.value.set(wx, wz); u.camVel.value.set(vx, 0, vz);
  };

  let env = null;
  const capture = (scene) => {
    env = captureEnv(renderer, scene, new THREE.Vector3(T.px[4], 1.6, T.pz[4]), q.env, road, city ? new THREE.Color(th.fog).multiplyScalar(0.5) : new THREE.Color(th.ground).multiplyScalar(0.6));
    return env;
  };
  const update = (cam, dt, time, hint) => { for (const u of updaters) u(cam, dt, time, hint); };
  const dispose = () => { for (const d of disposables) d.dispose && d.dispose(); road.dispose && road.dispose(); if (env) env.dispose(); };
  return { T, group, fog, road, rain, updateRain, update, captureEnv: capture, dispose, theme: th, bend, fieldAt, day: (!city && !th.nuit) || !!th.jourVille, lights: th.phares ?? city, sun: th.sun ? [th.sun.dir[0], th.sun.dir[2]] : null,
    sunDir: th.sun ? new THREE.Vector3().fromArray(th.sun.dir).normalize() : null, sunCol: th.sun ? new THREE.Color().fromArray(th.sun.col) : null };
}
