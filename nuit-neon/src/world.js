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
  const half = def.half || 13.5;           // demi-largeur de la route
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
  uniform vec3 amb; uniform vec3 curbCol; uniform vec3 offCol; uniform vec3 sunDir; uniform vec3 sunCol; uniform float wet; uniform float day;
  varying vec4 vUv; varying vec3 vW; varying vec2 vT;
  float hash(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
  float noise(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
  float fbm(vec2 p){ float a=.5, s=0.; for(int i=0;i<3;i++){ s+=a*noise(p); p*=2.03; a*=.5; } return s; }
  void main(){
    vec3 toCam = cameraPosition - vW; float dist = length(toCam); vec3 V = toCam/dist;
    vec2 p = vW.xz; float ax = abs(vT.x); float s = vT.y;
    float pud = smoothstep(0.42, 0.62, fbm(p*0.16 + 7.3) + (wet - 1.0)*0.35);
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
    albedo = mix(albedo, vec3(0.42,0.42,0.46), paint*0.9);
    albedo = mix(albedo, mix(vec3(0.02), vec3(0.5), chk), startBand);
    albedo = mix(albedo, mix(curbCol, vec3(0.45), cs), curb);
    float off = step(half_, ax);
    albedo = mix(albedo, offCol*(0.8+0.4*grain), off);
    float rough = mix(0.55, 0.06, pud); rough = mix(rough, 0.5, max(max(paint, startBand)*0.6, curb*0.7)); rough = mix(rough, 0.9, off);
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
    float strength = mix(0.55, 1.0, pud) * mix(1.0, 0.45, rough) * mix(0.25, 1.0, wet) * (1.0 - off*0.85);
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
        if (rnd() < 0.3) { g.globalAlpha = 0.55; g.fillStyle = '#000'; g.fillRect(X, Y, WW, HH * rr(0.25, 0.6)); }
        if (rnd() < 0.25) { g.globalAlpha = 0.35; g.fillStyle = '#000'; g.fillRect(X + WW * rr(0.2, 0.6), Y + HH * 0.45, WW * 0.3, HH * 0.55); }
      }
    }
  });
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
        ${th.sun ? `c = mix(c, vec3(1.0)*${(th.cloud ?? 0.9).toFixed(2)} + HOR*0.25, cl*0.75);` : `vec2 sp = floor(d.xz/max(0.2,d.y+0.3)*90.); c += step(0.9965, h21(sp)) * smoothstep(0.25,0.6,h) * vec3(0.9,0.95,1.0)*0.8*(1.-cl); c = mix(c, HOR*0.5 + MID*0.35, cl*0.6);`}
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
    const winTex = th.win.map((p, i) => windowTex(p, [0.6, 0.55, 0.5, 0.7][i]));
    const bMats = winTex.map(t => new THREE.MeshStandardMaterial({ color: 0x0d0e1c, roughness: 0.22, metalness: 0.85, emissive: 0xffffff, emissiveMap: t, emissiveIntensity: 1.25, envMapIntensity: 0.9 }));
    disposables.push(...winTex);
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
    const ground = add(new THREE.PlaneGeometry(6000, 6000, 72, 72), new THREE.MeshStandardMaterial({ color: 0x08080d, roughness: 0.5, metalness: 0.0, envMapIntensity: 0.3, polygonOffset: true, polygonOffsetFactor: 2, polygonOffsetUnits: 8 }));
    ground.rotation.x = -Math.PI / 2; ground.position.set(cx, -0.1, cz); ground.renderOrder = -1; // découpé en dalles : un rectangle géant fausse la profondeur et passe devant la route
    group.add(new THREE.HemisphereLight(new THREE.Color(th.c).lerp(new THREE.Color(0x8080ff), 0.5), 0x140820, 1.1));
    const key = new THREE.DirectionalLight(0xbfd0ff, 1.2); key.position.set(-6, 14, 8); group.add(key);
    const rim = new THREE.DirectionalLight(th.b, 1.4); rim.position.set(10, 6, -12); group.add(rim);
  } else {
    // ================= NATURE DE JOUR =================
    const gcol = new THREE.Color(th.ground);
    const gtex = canvasTex(256, 256, (g) => {
      g.fillStyle = hex(gcol); g.fillRect(0, 0, 256, 256);
      for (let i = 0; i < 2600; i++) { const c = gcol.clone().multiplyScalar(rr(0.7, 1.3)); g.fillStyle = hex(c); g.globalAlpha = rr(0.2, 0.7); const s = rr(2, 9); g.fillRect(rr(0, 256), rr(0, 256), s, s * rr(0.5, 2)); }
    });
    gtex.repeat.set(500, 500);
    const ground = add(new THREE.PlaneGeometry(8000, 8000, 90, 90), new THREE.MeshStandardMaterial({ map: gtex, roughness: 1, metalness: 0, envMapIntensity: 0.15, polygonOffset: true, polygonOffsetFactor: 2, polygonOffsetUnits: 8 }));
    ground.rotation.x = -Math.PI / 2; ground.position.set(cx, -0.1, cz); ground.renderOrder = -1;
    disposables.push(gtex);
    // montagnes à l'horizon
    {
      const pos = [], idx = [], col = [], seg = 120;
      const far = new THREE.Color(th.hill).lerp(FOG, 0.45), base = new THREE.Color(th.ground).lerp(FOG, 0.55);
      for (let ring = 0; ring < 2; ring++) {
        const rad = R + 900 + ring * 500, o = pos.length / 3;
        for (let i = 0; i <= seg; i++) {
          const a = i / seg * Math.PI * 2, x = cx + Math.cos(a) * rad, z = cz + Math.sin(a) * rad;
          const h = (th.hillH || 120) * (0.35 + 0.65 * Math.abs(Math.sin(a * 3.1 + ring * 2) * Math.sin(a * 7.3 + 1.3 + ring) + 0.35 * Math.sin(a * 17.0))) * (1 + ring * 0.9);
          pos.push(x, -2, z, x, h, z);
          const c1 = ring ? far.clone().lerp(FOG, 0.35) : far; col.push(base.r, base.g, base.b, c1.r, c1.g, c1.b);
          if (i < seg) { const k2 = o + i * 2; idx.push(k2, k2 + 1, k2 + 2, k2 + 1, k2 + 3, k2 + 2); }
        }
      }
      const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3)); g.setIndex(idx);
      const m = add(g, new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.DoubleSide, fog: false })); m.frustumCulled = false;
    }
    // arbres, rochers, cactus
    const kinds = {
      feuillu: () => mergeGeometries([colored(new THREE.CylinderGeometry(0.25, 0.42, 4, 6).translate(0, 2, 0), 0x4a3524),
        colored(new THREE.IcosahedronGeometry(2.5, 1).translate(0, 5.6, 0), 0x3f7d2c), colored(new THREE.IcosahedronGeometry(1.9, 1).translate(1.4, 4.6, 0.6), 0x4c8f32), colored(new THREE.IcosahedronGeometry(1.8, 1).translate(-1.2, 4.9, -0.6), 0x356b26)]),
      sapin: () => mergeGeometries([colored(new THREE.CylinderGeometry(0.22, 0.34, 2.4, 6).translate(0, 1.2, 0), 0x3a2a1c),
        colored(new THREE.ConeGeometry(2.5, 4.2, 8).translate(0, 4, 0), 0x1f4a30), colored(new THREE.ConeGeometry(1.9, 3.6, 8).translate(0, 6.3, 0), 0x2a5a3a), colored(new THREE.ConeGeometry(1.2, 3, 8).translate(0, 8.4, 0), th.snowTop ? 0xe8f0f8 : 0x34684a)]),
      cactus: () => mergeGeometries([colored(new THREE.CylinderGeometry(0.42, 0.48, 5, 8).translate(0, 2.5, 0), 0x3d7a3a),
        colored(new THREE.CylinderGeometry(0.28, 0.28, 1.6, 7).rotateZ(Math.PI / 2).translate(0.9, 2.6, 0), 0x3d7a3a), colored(new THREE.CylinderGeometry(0.28, 0.28, 1.8, 7).translate(1.6, 3.4, 0), 0x468a40),
        colored(new THREE.CylinderGeometry(0.26, 0.26, 1.3, 7).rotateZ(Math.PI / 2).translate(-0.8, 1.8, 0), 0x3d7a3a), colored(new THREE.CylinderGeometry(0.26, 0.26, 1.4, 7).translate(-1.4, 2.4, 0), 0x468a40)]),
      rocher: () => colored(new THREE.DodecahedronGeometry(2, 0).scale(1.5, 0.95, 1.15).translate(0, 0.9, 0), th.rock || 0x777a80),
    };
    const treeMat = new THREE.MeshStandardMaterial({ vertexColors: true, flatShading: true, roughness: 1, metalness: 0, envMapIntensity: 0.2 });
    disposables.push(treeMat);
    const d = new THREE.Object3D();
    for (const [kind, count, near, farD, s0, s1] of th.flore) {
      const spots = [];
      for (let t = 0; t < count * 3 && spots.length < count; t++) {
        const i = Math.floor(rnd() * T.N), side = rnd() < 0.5 ? -1 : 1, off = side * (HALF + near + Math.pow(rnd(), 1.6) * farD);
        const x = T.px[i] - T.tz[i] * off, z = T.pz[i] + T.tx[i] * off;
        if (farFromTrack(x, z, HALF + near - 1)) spots.push([x, z]);
      }
      const geo = kinds[kind](); geo.computeVertexNormals();
      const im = new THREE.InstancedMesh(geo, treeMat, spots.length);
      spots.forEach(([x, z], i) => { d.position.set(x, 0, z); d.rotation.set(0, rr(0, 6.28), 0); const s = rr(s0, s1); d.scale.set(s, s * rr(0.85, 1.2), s); d.updateMatrix(); im.setMatrixAt(i, d.matrix); });
      group.add(im); disposables.push(geo);
    }
    // bords de route
    if (T.libre) {
      // piquets de clôture et poteaux le long de la route de campagne
      const wood = [];
      for (let i = 0; i < T.N; i += 3) for (const s of [-1, 1]) { const o = s * (HALF + 9); wood.push(box(0.16, 1.2, 0.16, T.px[i] - T.tz[i] * o, 0.6, T.pz[i] + T.tx[i] * o)); }
      for (let i = 0; i < T.N; i += 24) { const o = -(HALF + 6); wood.push(box(0.3, 9, 0.3, T.px[i] - T.tz[i] * o, 4.5, T.pz[i] + T.tx[i] * o), box(2.2, 0.16, 0.16, T.px[i] - T.tz[i] * o, 8.4, T.pz[i] + T.tx[i] * o, Math.atan2(-T.tx[i], -T.tz[i]))); }
      add(mergeGeometries(wood), new THREE.MeshStandardMaterial({ color: 0x5a4632, roughness: 1 }));
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
      // panneaux de sponsors posés au bord de la piste
      for (let i = 30; i < T.N; i += 46) { const s = rnd() < 0.5 ? -1 : 1, o = s * (HALF + 2.2); adQuads.push({ cx: T.px[i] - T.tz[i] * o, cz: T.pz[i] + T.tx[i] * o, y: 0.5, pw: 9, ph: 2.2, ang: Math.atan2(-T.tx[i], -T.tz[i]) + (s > 0 ? -Math.PI / 2 : Math.PI / 2), cell: 1 + Math.floor(rnd() * 3) }); }
    }
    flushAds(0.9);
    // herbe haute : un tapis de touffes qui suit la caméra
    if (th.grass && q.grass > 0) {
      const n = Math.floor(q.grass * (th.grass.dens || 1)), S = 72;
      const g = new THREE.InstancedBufferGeometry();
      const base = mergeGeometries([new THREE.PlaneGeometry(1.9, 1).translate(0, 0.5, 0), new THREE.PlaneGeometry(1.9, 1).translate(0, 0.5, 0).rotateY(Math.PI / 2 * 0.9)]);
      g.index = base.index; g.setAttribute('position', base.attributes.position); g.setAttribute('uv', base.attributes.uv);
      const off = new Float32Array(n * 4);
      for (let i = 0; i < n; i++) { off[i * 4] = rr(-S / 2, S / 2); off[i * 4 + 1] = rr(-S / 2, S / 2); off[i * 4 + 2] = 0; off[i * 4 + 3] = rr(0, 6.28); }
      const attr = new THREE.InstancedBufferAttribute(off, 4); attr.setUsage(THREE.DynamicDrawUsage); g.setAttribute('aOff', attr);
      g.instanceCount = n;
      const mat = new THREE.ShaderMaterial({
        side: THREE.DoubleSide,
        uniforms: { time: { value: 0 }, fogColor: { value: FOG }, fogDensity: { value: th.dens }, c0: { value: new THREE.Color(th.grass.c0) }, c1: { value: new THREE.Color(th.grass.c1) }, c2: { value: new THREE.Color(th.grass.c2) }, sunCol: { value: new THREE.Color().fromArray(th.sun.col) }, hgt: { value: th.grass.h || 1.3 } },
        vertexShader: `attribute vec4 aOff; uniform float time; uniform float hgt; varying vec2 vU; varying float vD; varying float vK; varying float vH;
          float hash(vec2 p){ p = fract(p*vec2(123.34,456.21)); p += dot(p,p+45.32); return fract(p.x*p.y); }
          void main(){ vU = uv; float s = aOff.z; float c = cos(aOff.w), sn = sin(aOff.w);
            vK = hash(floor(aOff.xy*0.7)); vH = smoothstep(0.35, 0.65, sin(aOff.x*0.021)*sin(aOff.y*0.017)*0.5 + 0.5);
            vec3 p = position; p.y *= hgt * (0.7 + 0.6*vK) * (1.0 + 0.35*vH); p.xz *= 1.0;
            p = vec3(p.x*c - p.z*sn, p.y, p.x*sn + p.z*c) * s;
            vec3 w = vec3(aOff.x, 0.0, aOff.y) + p;
            float sway = sin(time*1.7 + w.x*0.35 + w.z*0.27) + 0.5*sin(time*3.1 + w.x*0.9);
            w.x += sway * 0.13 * uv.y * uv.y * hgt; w.z += sway * 0.08 * uv.y * uv.y * hgt;
            vec4 mv = viewMatrix * vec4(w, 1.0); vD = -mv.z; gl_Position = projectionMatrix * mv; }`,
        fragmentShader: `uniform vec3 fogColor; uniform float fogDensity; uniform vec3 c0; uniform vec3 c1; uniform vec3 c2; uniform vec3 sunCol; varying vec2 vU; varying float vD; varying float vK; varying float vH;
          void main(){ float bl = abs(fract(vU.x*7.0 + vK*3.0) - 0.5) * 2.0; if (bl > (1.0 - vU.y) * 1.25 + 0.04) discard;
            vec3 tip = mix(c1, c2, vH); vec3 col = mix(c0, tip, vU.y) * (0.75 + 0.5*vK); col *= 0.55 + 0.75*sunCol*(0.35 + 0.65*vU.y);
            float f = 1.0 - exp(-fogDensity*fogDensity*vD*vD); col = mix(col, fogColor, f);
            gl_FragColor = vec4(col, 1.0);
            #include <tonemapping_fragment>
            #include <colorspace_fragment>
          }`,
      });
      const mesh = new THREE.Mesh(g, mat); mesh.frustumCulled = false; mesh.layers.set(1); group.add(mesh); disposables.push(g, mat, base);
      let first = true;
      // les touffes qui sortent du carré centré sur la caméra réapparaissent de l'autre côté ; aucune sur la route
      updaters.push((cam, dt, time, hint) => {
        mat.uniforms.time.value = time;
        const fx = cam.position.x, fz = cam.position.z; let dirty = false;
        for (let i = 0; i < n; i++) {
          const k4 = i * 4; let x = off[k4], z = off[k4 + 1], moved = first;
          while (x - fx > S / 2) { x -= S; moved = true; } while (x - fx < -S / 2) { x += S; moved = true; }
          while (z - fz > S / 2) { z -= S; moved = true; } while (z - fz < -S / 2) { z += S; moved = true; }
          if (!moved) continue;
          const j = T.nearest(x, z, hint, 30), lat = Math.abs(T.lateral(x, z, j));
          off[k4] = x; off[k4 + 1] = z; off[k4 + 2] = lat < HALF + (T.libre ? 1.2 : 0.9) ? 0 : 1; dirty = true;
        }
        first = false;
        if (dirty) attr.needsUpdate = true;
      });
    }
    const sunV = new THREE.Vector3().fromArray(th.sun.dir);
    group.add(new THREE.HemisphereLight(new THREE.Color().fromArray(th.sky[1]), gcol, th.ambI ?? 1.5));
    const sunL = new THREE.DirectionalLight(new THREE.Color().fromArray(th.sun.col), th.sunI ?? 2.6); sunL.position.copy(sunV).multiplyScalar(100); group.add(sunL);
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
        laneW: { value: T.libre ? 40 : 4.5 }, curbOn: { value: T.libre ? 0 : 1 }, wet: { value: th.wet ?? 1 }, day: { value: city ? 0 : 1 },
        amb: { value: city ? new THREE.Color(th.c).lerp(new THREE.Color(0.55, 0.5, 0.9), 0.6).multiplyScalar(1.6) : new THREE.Color().fromArray(th.sky[1]).multiplyScalar(0.9) },
        sunDir: { value: sunDir }, sunCol: { value: th.sun ? new THREE.Color().fromArray(th.sun.col).multiplyScalar(th.sun.road ?? 1) : new THREE.Color(0, 0, 0) },
        offCol: { value: city ? new THREE.Color(0.02, 0.02, 0.03) : new THREE.Color(th.ground).multiplyScalar(0.5) },
        curbCol: { value: curbHex.clone().multiplyScalar(city ? 0.45 : 0.7) } },
      vertexShader: ROAD_VERT, fragmentShader: `#define TAPS ${q.taps}\n` + ROAD_FRAG,
    },
  });
  road.rotation.x = -Math.PI / 2; road.frustumCulled = false; road.renderOrder = 5;
  group.add(road); disposables.push(roadGeo, road.material);

  // --- pluie ou neige ---
  let rain = null;
  if (th.rain) {
    const snow = th.rain === 'neige';
    const n = 420, pos = new Float32Array(n * 6), base = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { base[i * 3] = rr(-22, 22); base[i * 3 + 1] = rr(0, 18); base[i * 3 + 2] = rr(-22, 22); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    rain = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ color: snow ? 0xffffff : 0xaac4ff, transparent: true, opacity: snow ? 0.7 : 0.22, depthWrite: false }));
    rain.frustumCulled = false; rain.layers.set(1); rain.userData = { base, n, fall: snow ? 5 : 26, len: snow ? 0.12 : 0.9 };
    group.add(rain); disposables.push(g, rain.material);
  }
  const updateRain = (cam, dt, vx, vz) => {
    if (!rain || !rain.visible) return;
    const { base, n, fall, len } = rain.userData, p = rain.geometry.attributes.position.array;
    for (let i = 0; i < n; i++) {
      let y = base[i * 3 + 1] - fall * dt; if (y < 0) y += 18; base[i * 3 + 1] = y;
      let x = base[i * 3] - vx * dt, z = base[i * 3 + 2] - vz * dt;
      if (x < -22) x += 44; else if (x > 22) x -= 44; if (z < -22) z += 44; else if (z > 22) z -= 44;
      base[i * 3] = x; base[i * 3 + 2] = z;
      const X = cam.position.x + x, Z = cam.position.z + z, k6 = i * 6;
      p[k6] = X; p[k6 + 1] = y; p[k6 + 2] = Z;
      p[k6 + 3] = X - vx * 0.03; p[k6 + 4] = y + len; p[k6 + 5] = Z - vz * 0.03;
    }
    rain.geometry.attributes.position.needsUpdate = true;
  };

  let env = null;
  const capture = (scene) => {
    env = captureEnv(renderer, scene, new THREE.Vector3(T.px[4], 1.6, T.pz[4]), q.env, road, city ? new THREE.Color(th.fog).multiplyScalar(0.5) : new THREE.Color(th.ground).multiplyScalar(0.6));
    return env;
  };
  const update = (cam, dt, time, hint) => { for (const u of updaters) u(cam, dt, time, hint); };
  const dispose = () => { for (const d of disposables) d.dispose && d.dispose(); road.dispose && road.dispose(); if (env) env.dispose(); };
  return { T, group, fog, road, rain, updateRain, update, captureEnv: capture, dispose, theme: th, day: !city };
}
