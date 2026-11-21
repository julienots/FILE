import * as THREE from 'three';
import { U } from './shared.js';
import { fbm, smoothstep, clamp } from './noise.js';
import { WORLD_R } from './terrain.js';

/*
 * HERBE HAUTE DÉFINITION (GPU) — v2
 * - Deux couches de densité : « proche » (brins très serrés, plus de segments) et « moyenne » (jusqu'à ~30 m).
 *   Les brins sont générés dans le vertex shader sur une grille qui suit le regard : aucune donnée par brin côté CPU.
 * - Touffes naturelles : les brins se regroupent autour de centres de touffe et s'ouvrent vers l'extérieur.
 * - INTERACTION : jusqu'à 16 « pousseurs » (habitant, villageois, moutons, créatures + traînée du joueur)
 *   écartent et couchent l'herbe ; elle frémit puis se relève lentement derrière eux.
 * - Éclairage : normales arrondies (brin cylindrique), translucidité à contre-jour, reflet spéculaire,
 *   occlusion à la racine, rafales de vent visibles en vagues claires sur la prairie.
 */
export const MAX_PUSH = 16;

export function buildGrassField(terrain, { layers }) {
  const { N, SIZE, heights, pathMask, wetMask } = terrain;
  // --- textures : hauteur (demi-flottant, filtrable) et masque de végétation ---
  const hData = new Uint16Array(N * N);
  for (let i = 0; i < N * N; i++) hData[i] = THREE.DataUtils.toHalfFloat(heights[i]);
  const hTex = new THREE.DataTexture(hData, N, N, THREE.RedFormat, THREE.HalfFloatType);
  hTex.magFilter = hTex.minFilter = THREE.LinearFilter; hTex.needsUpdate = true;
  const mData = new Uint8Array(N * N * 4);
  const STEP = SIZE / (N - 1);
  for (let iz = 0; iz < N; iz++) for (let ix = 0; ix < N; ix++) {
    const k = iz * N + ix, x = -SIZE / 2 + ix * STEP, z = -SIZE / 2 + iz * STEP;
    const h = heights[k];
    const hx = heights[iz * N + Math.min(N - 1, ix + 1)] - heights[iz * N + Math.max(0, ix - 1)];
    const hz = heights[Math.min(N - 1, iz + 1) * N + ix] - heights[Math.max(0, iz - 1) * N + ix];
    const slope = Math.hypot(hx, hz) / (2 * STEP);
    let dens = 1;
    dens *= 1 - smoothstep(0.15, 0.7, pathMask[k]);
    dens *= 1 - smoothstep(0.1, 0.5, wetMask[k]);
    dens *= 1 - smoothstep(0.45, 0.75, slope);
    dens *= 1 - smoothstep(32, 44, h);
    dens *= 1 - smoothstep(WORLD_R - 6, WORLD_R - 3, Math.hypot(x, z));
    dens *= 0.7 + 0.3 * smoothstep(-0.25, 0.25, fbm(x * 0.05 + 3, z * 0.05, 2));
    const dry = clamp(0.5 + fbm(x * 0.03 + 9, z * 0.03, 3) * 1.6, 0, 1);
    const tall = clamp(0.5 + fbm(x * 0.04 - 5, z * 0.04 + 2, 2) * 1.8, 0, 1);
    mData[k * 4] = dens * 255; mData[k * 4 + 1] = dry * 255; mData[k * 4 + 2] = terrain.forestMask(x, z) * 255; mData[k * 4 + 3] = tall * 255;
  }
  const mTex = new THREE.DataTexture(mData, N, N, THREE.RGBAFormat);
  mTex.magFilter = mTex.minFilter = THREE.LinearFilter; mTex.needsUpdate = true;

  const shared = {
    uHeight: { value: hTex }, uMask: { value: mTex }, uCenter: { value: new THREE.Vector3() },
    uSize: { value: SIZE }, uN: { value: N },
    uTime: U.uTime, uWind: U.uWind, uSunDir: U.uSunDir, uSunColor: U.uSunColor, uFade: { value: 1 },
    uPush: { value: Array.from({ length: MAX_PUSH }, () => new THREE.Vector4()) },
  };

  const group = new THREE.Group(); group.name = 'grassfield';
  layers.forEach((L, li) => group.add(makeLayer(L, li)));

  function makeLayer({ cell, rIn = 0, r, seg }, li) {
    // brin : ruban effilé de `seg` segments, x ∈ [-0.5, 0.5], y ∈ [0, 1]
    const pos = [], idx = [];
    for (let i = 0; i < seg; i++) { const t = i / seg; pos.push(-0.5, t, 0, 0.5, t, 0); }
    pos.push(0, 1, 0);
    for (let i = 0; i < seg - 1; i++) { const a = i * 2; idx.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); }
    idx.push((seg - 1) * 2, (seg - 1) * 2 + 1, seg * 2);
    const geo = new THREE.InstancedBufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    geo.setAttribute('normal', new THREE.Float32BufferAttribute(new Float32Array(pos.length), 3));
    geo.setIndex(idx);
    const side = Math.ceil((r * 2) / cell);
    geo.instanceCount = side * side;
    geo.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 1e6);
    const uniforms = { ...shared, uCell: { value: cell }, uSide: { value: side }, uRadius: { value: r }, uRIn: { value: rIn } };

    const mat = new THREE.MeshLambertMaterial({ side: THREE.DoubleSide });
    mat.onBeforeCompile = (s) => {
      Object.assign(s.uniforms, uniforms);
      s.vertexShader = s.vertexShader
        .replace('#include <common>', `#include <common>
uniform sampler2D uHeight; uniform sampler2D uMask; uniform vec3 uCenter; uniform float uCell, uSide, uRadius, uRIn, uSize, uN, uTime, uWind, uFade;
uniform vec3 uSunDir; uniform vec4 uPush[${MAX_PUSH}];
varying vec3 vGCol; varying float vTip; varying float vBack; varying float vSpec;
float gh(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float gn(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
  return mix(mix(gh(i),gh(i+vec2(1,0)),f.x), mix(gh(i+vec2(0,1)),gh(i+vec2(1,1)),f.x), f.y); }
vec2 gtex(vec2 w){ vec2 t = (w + uSize*0.5) / uSize; return t * (uN - 1.0) / uN + 0.5 / uN; }`)
        .replace('#include <beginnormal_vertex>', `
  float id = float(gl_InstanceID);
  vec2 g = vec2(mod(id, uSide), floor(id / uSide));
  vec2 cellW = (floor(uCenter.xz / uCell) + g - uSide * 0.5) * uCell;
  float r1 = gh(cellW), r2 = gh(cellW + 17.3), r3 = gh(cellW + 41.9), r4 = gh(cellW - 9.1), r5 = gh(cellW + 3.7);
  vec2 wp = cellW + (vec2(r1, r2) - 0.5) * uCell * 1.7;
  // touffes : les brins se regroupent autour d'un centre et s'ouvrent vers l'extérieur
  vec2 cc = floor(wp / 0.5) * 0.5 + 0.25 + (vec2(gh(floor(wp / 0.5)), gh(floor(wp / 0.5) + 7.7)) - 0.5) * 0.3;
  float clumpH = gh(cc + 2.2);
  wp = mix(wp, cc, 0.32);
  vec2 fromC = wp - cc; float lc = length(fromC);
  vec4 m = texture2D(uMask, gtex(wp));
  float dist = length(wp - uCenter.xz);
  float distK = (1.0 - smoothstep(uRadius * 0.7, uRadius, dist)) * smoothstep(uRIn - 1.2, uRIn + 0.3, dist);
  float keep = step(r3, m.r * mix(0.45, 1.0, 1.0 - smoothstep(uRadius * 0.4, uRadius, dist)));
  float hgt = (0.2 + r4 * 0.35 + m.a * 0.35 + clumpH * 0.25) * (0.55 + 0.45 * m.r) * distK * keep * uFade;
  float width = (0.018 + r5 * 0.018) * (1.0 + smoothstep(uRadius * 0.35, uRadius, dist) * 1.6) * step(0.001, hgt);
  float ground = texture2D(uHeight, gtex(wp)).r;
  float t = position.y;
  // orientation : en éventail depuis le centre de la touffe
  vec2 outDir = lc > 1e-3 ? fromC / lc : vec2(1.0, 0.0);
  float ang = r1 * 6.2831;
  vec2 dir = normalize(mix(vec2(cos(ang), sin(ang)), outDir, 0.6));
  vec2 perp = vec2(-dir.y, dir.x);
  // vent : houle + rafales
  float gust = smoothstep(0.3, 0.9, gn(wp * 0.06 - uTime * vec2(0.5, 0.2)));
  float sway = sin(uTime * 2.1 + wp.x * 0.35 + wp.y * 0.27 + r3 * 3.0) * 0.25 + sin(uTime * 3.7 + wp.x * 1.3 + r2) * 0.08;
  vec2 windDir = normalize(vec2(1.0, 0.45));
  vec2 B = dir * (0.18 + r2 * 0.4) * hgt + windDir * (sway + gust * 1.1 + 0.25) * uWind * hgt * 0.5;
  // interaction : l'herbe s'écarte et se couche autour des pousseurs, puis frémit
  float flatten = 0.0;
  vec2 pushV = vec2(0.0);
  for (int i = 0; i < ${MAX_PUSH}; i++) {
    vec4 P = uPush[i];
    if (P.w <= 0.0) continue;
    vec2 d = wp - P.xy; float l = length(d);
    float k = (1.0 - smoothstep(P.z * 0.25, P.z, l)) * P.w;
    if (k <= 0.0) continue;
    pushV += (l > 1e-3 ? d / l : dir) * k;
    flatten = max(flatten, k);
  }
  B += pushV * hgt * 1.1;
  B += dir * sin(uTime * 13.0 + r1 * 40.0) * 0.06 * flatten * hgt;
  float bl = length(B), maxB = hgt * 0.97;
  if (bl > maxB) { B *= maxB / bl; bl = maxB; }
  float lean = bl / max(hgt, 1e-3);
  float yScale = sqrt(max(0.04, 1.0 - lean * lean)) * (1.0 - flatten * 0.25);
  float w = width * (1.0 - t * 0.88);
  vec3 gPos = vec3(wp.x + perp.x * position.x * w + B.x * t * t,
                   ground - 0.04 + t * hgt * yScale,
                   wp.y + perp.y * position.x * w + B.y * t * t);
  // normale arrondie : un brin est un cylindre écrasé, pas un ruban plat
  vec3 faceN = normalize(vec3(dir.x, 0.0, dir.y) * -1.0 + vec3(perp.x, 0.0, perp.y) * position.x * 1.6);
  vec3 objectNormal = normalize(mix(vec3(0.0, 1.0, 0.0), faceN, 0.45));
  // couleur
  vec3 cFresh = vec3(0.25, 0.50, 0.08), cDeep = vec3(0.09, 0.27, 0.04), cDry = vec3(0.64, 0.58, 0.26), cForest = vec3(0.10, 0.25, 0.06), cTip = vec3(0.55, 0.68, 0.22);
  vec3 c = mix(cFresh, cDeep, r4 * 0.55 + (1.0 - clumpH) * 0.2);
  c = mix(c, cDry, smoothstep(0.62, 0.98, m.g) * (0.25 + r5 * 0.5));
  c = mix(c, cForest, m.b * 0.5);
  c = mix(c, cTip, t * t * 0.22 * (1.0 - m.b));
  c *= 0.88 + r2 * 0.24;
  vTip = t;
  float ao = mix(0.22, 1.0, smoothstep(0.0, 0.7, t)) * (1.0 - flatten * 0.15);
  vGCol = c * ao * (1.0 + gust * 0.3 * t);
  vec3 toCam = normalize(cameraPosition - gPos);
  vBack = pow(max(dot(-toCam, uSunDir), 0.0), 3.0);
  vSpec = pow(max(dot(normalize(uSunDir + toCam), normalize(objectNormal)), 0.0), 40.0) * t;
`)
        .replace('#include <begin_vertex>', 'vec3 transformed = gPos;');
      s.fragmentShader = s.fragmentShader
        .replace('#include <common>', '#include <common>\nvarying vec3 vGCol; varying float vTip; varying float vBack; varying float vSpec; uniform vec3 uSunColor;')
        .replace('#include <normal_fragment_begin>', '#include <normal_fragment_begin>\n normal = normalize(vNormal);')
        .replace('#include <color_fragment>', '#include <color_fragment>\n diffuseColor.rgb = vGCol;')
        .replace('#include <emissivemap_fragment>', '#include <emissivemap_fragment>\n totalEmissiveRadiance += vGCol * uSunColor * vBack * vTip * 1.8 + uSunColor * vSpec * 0.07;');
    };
    mat.customProgramCacheKey = () => `grassfield-v2-${li}-${seg}`;
    const mesh = new THREE.Mesh(geo, mat);
    mesh.frustumCulled = false; mesh.receiveShadow = true; mesh.castShadow = false;
    return mesh;
  }

  group.userData = {
    uniforms: shared,
    /** retire l'herbe sur un rectangle orienté (maison posée en mode Créateur) */
    clearRect(x0, z0, hw, hd, rot) {
      const c = Math.cos(rot), s = Math.sin(rot), m = hw + hd + 1;
      for (let iz = 0; iz < N; iz++) for (let ix = 0; ix < N; ix++) {
        const x = -SIZE / 2 + ix * STEP, z = -SIZE / 2 + iz * STEP;
        if (Math.abs(x - x0) > m || Math.abs(z - z0) > m) continue;
        const dx = x - x0, dz = z - z0, lx = dx * c - dz * s, lz = dx * s + dz * c;
        if (Math.abs(lx) < hw && Math.abs(lz) < hd) mData[(iz * N + ix) * 4] = 0;
      }
      mTex.needsUpdate = true;
    },
  };
  return group;
}

/**
 * Suit les « pousseurs » : entités vivantes proches + traînée laissée par l'habitant
 * (l'herbe couchée se relève en ~3 s).
 */
export function createPushers() {
  const trail = [];
  let acc = 0, last = null;
  return function update(dt, player, others, uniforms) {
    const arr = uniforms.uPush.value;
    const p = player.root.position;
    acc += dt;
    if (acc > 0.12) {
      acc = 0;
      if (!last || Math.hypot(p.x - last.x, p.z - last.z) > 0.25) { trail.push({ x: p.x, z: p.z, age: 0 }); last = { x: p.x, z: p.z }; }
    }
    for (const t of trail) t.age += dt;
    while (trail.length && (trail[0].age > 3.2 || trail.length > 9)) trail.shift();
    let i = 0;
    arr[i++].set(p.x, p.z, 0.75, 1);
    const near = others.filter(o => o.root.visible !== false)
      .map(o => ({ o, d: o.root.position.distanceToSquared(p) })).filter(e => e.d < 900).sort((a, b) => a.d - b.d).slice(0, MAX_PUSH - 10);
    for (const { o } of near) { const q = o.root.position; arr[i++].set(q.x, q.z, o.pushR || 0.6, 1); }
    for (const t of trail) { if (i >= MAX_PUSH) break; arr[i++].set(t.x, t.z, 0.65, Math.pow(1 - t.age / 3.2, 1.5) * 0.9); }
    while (i < MAX_PUSH) arr[i++].set(0, 0, 0, 0);
  };
}
