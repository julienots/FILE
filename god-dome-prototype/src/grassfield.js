import * as THREE from 'three';
import { U } from './shared.js';
import { fbm, smoothstep, clamp } from './noise.js';
import { WORLD_R } from './terrain.js';

/*
 * HERBE HAUTE DÉFINITION (GPU)
 * Des dizaines de milliers de brins individuels sont générés directement dans le vertex shader,
 * sur une grille qui suit le joueur (aucune donnée par brin côté CPU).
 * Chaque brin lit la hauteur du terrain et un masque de végétation dans deux textures :
 * il pousse là où le monde le permet (pas sur les chemins, l'eau, la roche ni dans les maisons).
 * Ombres reçues, translucidité à contre-jour, rafales de vent qui couchent l'herbe en vagues,
 * couleur par brin (vert tendre, vert profond, herbe sèche), occlusion à la racine.
 */
export function buildGrassField(terrain, { cell = 0.12, radius = 22 } = {}) {
  const { N, SIZE, heights, pathMask, wetMask } = terrain;
  // --- textures : hauteur (demi-flottant, filtrable) et masque ---
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
    dens *= 1 - smoothstep(0.42, 0.7, slope);
    dens *= 1 - smoothstep(30, 42, h);
    dens *= 1 - smoothstep(WORLD_R - 6, WORLD_R - 3, Math.hypot(x, z));
    dens *= 0.55 + 0.45 * smoothstep(-0.25, 0.25, fbm(x * 0.05 + 3, z * 0.05, 2)); // clairières, touffes
    const dry = clamp(0.5 + fbm(x * 0.03 + 9, z * 0.03, 3) * 1.6, 0, 1);
    const tall = clamp(0.5 + fbm(x * 0.04 - 5, z * 0.04 + 2, 2) * 1.8, 0, 1);
    mData[k * 4] = dens * 255; mData[k * 4 + 1] = dry * 255; mData[k * 4 + 2] = terrain.forestMask(x, z) * 255; mData[k * 4 + 3] = tall * 255;
  }
  const mTex = new THREE.DataTexture(mData, N, N, THREE.RGBAFormat);
  mTex.magFilter = mTex.minFilter = THREE.LinearFilter; mTex.needsUpdate = true;

  // --- un brin : ruban effilé de 6 segments, courbé ---
  const SEG = 6, pos = [], uv = [], idx = [];
  for (let i = 0; i <= SEG; i++) {
    const t = i / SEG;
    if (i < SEG) { pos.push(-0.5, t, 0, 0.5, t, 0); uv.push(0, t, 1, t); }
    else { pos.push(0, 1, 0); uv.push(0.5, 1); }
  }
  for (let i = 0; i < SEG - 1; i++) { const a = i * 2; idx.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); }
  idx.push((SEG - 1) * 2, (SEG - 1) * 2 + 1, SEG * 2);
  const blade = new THREE.InstancedBufferGeometry();
  blade.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  blade.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  blade.setAttribute('normal', new THREE.Float32BufferAttribute(new Array(pos.length / 3).fill(0).flatMap(() => [0, 1, 0]), 3));
  blade.setIndex(idx);
  const side = Math.ceil((radius * 2) / cell);
  blade.instanceCount = side * side;
  blade.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 1e6);

  const uniforms = {
    uHeight: { value: hTex }, uMask: { value: mTex }, uCenter: { value: new THREE.Vector3() },
    uCell: { value: cell }, uSide: { value: side }, uRadius: { value: radius }, uSize: { value: SIZE }, uN: { value: N },
    uTime: U.uTime, uWind: U.uWind, uSunDir: U.uSunDir, uSunColor: U.uSunColor, uFade: { value: 1 },
  };
  const mat = new THREE.MeshLambertMaterial({ side: THREE.DoubleSide });
  mat.onBeforeCompile = (s) => {
    Object.assign(s.uniforms, uniforms);
    s.vertexShader = s.vertexShader
      .replace('#include <common>', `#include <common>
uniform sampler2D uHeight; uniform sampler2D uMask; uniform vec3 uCenter; uniform float uCell, uSide, uRadius, uSize, uN, uTime, uWind, uFade;
uniform vec3 uSunDir;
varying vec3 vGCol; varying float vTip; varying float vBack;
float gh(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float gn(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
  return mix(mix(gh(i),gh(i+vec2(1,0)),f.x), mix(gh(i+vec2(0,1)),gh(i+vec2(1,1)),f.x), f.y); }
vec2 gtex(vec2 w){ vec2 t = (w + uSize*0.5) / uSize; return t * (uN - 1.0) / uN + 0.5 / uN; }`)
      .replace('#include <begin_vertex>', `
  float id = float(gl_InstanceID);
  vec2 g = vec2(mod(id, uSide), floor(id / uSide));
  vec2 cellW = (floor(uCenter.xz / uCell) + g - uSide * 0.5) * uCell;
  float r1 = gh(cellW), r2 = gh(cellW + 17.3), r3 = gh(cellW + 41.9), r4 = gh(cellW - 9.1), r5 = gh(cellW + 3.7);
  vec2 wp = cellW + (vec2(r1, r2) - 0.5) * uCell * 1.6;
  vec4 m = texture2D(uMask, gtex(wp));
  float dist = length(wp - uCenter.xz);
  float distK = 1.0 - smoothstep(uRadius * 0.6, uRadius, dist);
  // densité réduite au loin (LOD) puis disparition douce
  float keep = step(r3, m.r * mix(0.35, 1.0, 1.0 - smoothstep(uRadius*0.3, uRadius*0.85, dist)));
  float hgt = (0.18 + r4 * 0.42 + m.a * 0.38) * (0.5 + 0.5 * m.r) * distK * keep * uFade;
  float width = (0.022 + r5 * 0.02) * (1.0 + smoothstep(uRadius*0.4, uRadius, dist) * 1.8) * step(0.001, hgt);
  float ground = texture2D(uHeight, gtex(wp)).r;
  float t = position.y;
  // orientation et courbure naturelles
  float ang = r1 * 6.2831;
  vec2 dir = vec2(cos(ang), sin(ang));
  vec2 perp = vec2(-dir.y, dir.x);
  float lean = (0.15 + r2 * 0.45) * t * t * hgt;
  // vent : houle de fond + rafales qui parcourent la prairie
  float gust = smoothstep(0.3, 0.9, gn(wp * 0.06 - uTime * vec2(0.5, 0.2)));
  float sway = sin(uTime * 2.1 + wp.x * 0.35 + wp.y * 0.27 + r3 * 3.0) * 0.25 + sin(uTime * 3.7 + wp.x * 1.3) * 0.08;
  vec2 windDir = normalize(vec2(1.0, 0.45));
  vec2 bend = dir * lean + windDir * (sway + gust * 1.1 + 0.25) * uWind * t * t * hgt * 0.55;
  vec3 transformed = vec3(wp.x + perp.x * position.x * width * (1.0 - t * 0.85) + bend.x,
                          ground - 0.03 + t * hgt * (1.0 - 0.25 * length(bend) / max(hgt, 0.01)),
                          wp.y + perp.y * position.x * width * (1.0 - t * 0.85) + bend.y);
  // couleur du brin
  vec3 cFresh = vec3(0.30, 0.52, 0.10), cDeep = vec3(0.12, 0.30, 0.05), cDry = vec3(0.62, 0.58, 0.25), cForest = vec3(0.10, 0.25, 0.06);
  vec3 c = mix(cFresh, cDeep, r4 * 0.6);
  c = mix(c, cDry, smoothstep(0.55, 0.95, m.g) * (0.4 + r5 * 0.6));
  c = mix(c, cForest, m.b * 0.5);
  c *= 0.85 + r2 * 0.3;
  vTip = t;
  vGCol = c * mix(0.28, 1.0, smoothstep(0.0, 0.75, t)) * (1.0 + gust * 0.25 * t);
  vec3 toCam = normalize(transformed - cameraPosition);
  vBack = pow(max(dot(toCam, uSunDir), 0.0), 3.0);
`)
      .replace('#include <beginnormal_vertex>', 'vec3 objectNormal = vec3(0.0, 1.0, 0.0);');
    s.fragmentShader = s.fragmentShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vGCol; varying float vTip; varying float vBack; uniform vec3 uSunColor;')
      .replace('#include <color_fragment>', '#include <color_fragment>\n diffuseColor.rgb = vGCol;')
      .replace('#include <emissivemap_fragment>', '#include <emissivemap_fragment>\n totalEmissiveRadiance += vGCol * uSunColor * vBack * vTip * 1.6;');
  };
  mat.customProgramCacheKey = () => 'grassfield-v1';
  const mesh = new THREE.Mesh(blade, mat);
  mesh.frustumCulled = false; mesh.receiveShadow = true; mesh.castShadow = false;
  mesh.renderOrder = 0;
  mesh.userData = {
    uniforms,
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
  return mesh;
}
