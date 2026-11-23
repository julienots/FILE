// Matériaux et géométries partagés : blocs glossy cartoon, palettes débloquables,
// shader du monstre (noir profond, liseré rouge, distorsion glitch), visages et yeux.
import * as THREE from 'three';
import { RoundedBoxGeometry } from '../lib/addons/geometries/RoundedBoxGeometry.js';

export const PALETTES = {
  classic: { name: 'Classique', I: 0x19d3ff, O: 0xffd21f, T: 0xa24bff, S: 0x3be36b, Z: 0xff3b5c, J: 0x2f6bff, L: 0xff8a1f },
  neon: { name: 'Néon', I: 0x00fff0, O: 0xfff200, T: 0xff00e6, S: 0x39ff14, Z: 0xff073a, J: 0x0099ff, L: 0xff6a00 },
  pastel: { name: 'Pastel', I: 0x9ee7ff, O: 0xffe7a0, T: 0xd7b5ff, S: 0xb3f5c4, Z: 0xffb3c1, J: 0xa8c1ff, L: 0xffc89e },
  candy: { name: 'Bonbon', I: 0x4dd0e1, O: 0xffeb3b, T: 0xf06292, S: 0xaed581, Z: 0xff8a80, J: 0x7986cb, L: 0xffb74d },
  sunset: { name: 'Crépuscule', I: 0xff7eb3, O: 0xffc371, T: 0xc471f5, S: 0xfa71cd, Z: 0xff5e62, J: 0x7f7fd5, L: 0xff9966 },
  obsidian: { name: 'Obsidienne', I: 0x2a3a44, O: 0x4a4228, T: 0x3a2a48, S: 0x22402c, Z: 0x48222a, J: 0x222c48, L: 0x48321e },
  gold: { name: 'Or du Roi', I: 0xffd36b, O: 0xffc533, T: 0xe8b04a, S: 0xf5d27a, Z: 0xd99a2b, J: 0xc9a227, L: 0xffe08a },
};
export const SPECIAL = { G: 0x6b7080, X: 0x080809, K: 0xffd700, W: 0x252530, M: 0x0a0a0c };

/** Lueur propre à la couleur du bloc (fonctionne aussi avec les couleurs par instance). */
function vivid(m, k = 0.16) {
  const prev = m.onBeforeCompile;
  m.onBeforeCompile = (sh, r) => {
    prev && prev(sh, r);
    sh.fragmentShader = sh.fragmentShader.replace('#include <emissivemap_fragment>', `#include <emissivemap_fragment>
      totalEmissiveRadiance += diffuseColor.rgb * ${k.toFixed(2)};`);
  };
  m.customProgramCacheKey = () => 'vivid' + k + m.type;
  m.envMapIntensity = 0.55;
  return m;
}

export const MATERIALS = {
  glossy: { name: 'Glossy', make: (c) => vivid(new THREE.MeshPhysicalMaterial({ color: c, roughness: 0.3, metalness: 0, clearcoat: 1, clearcoatRoughness: 0.06, emissive: 0x000000 })) },
  matte: { name: 'Mat doux', make: (c) => vivid(new THREE.MeshStandardMaterial({ color: c, roughness: 0.85, metalness: 0, emissive: 0x000000 }), 0.12) },
  metal: { name: 'Métal', make: (c) => vivid(new THREE.MeshPhysicalMaterial({ color: c, roughness: 0.22, metalness: 0.85, clearcoat: 0.6, emissive: 0x000000 }), 0.1) },
  jelly: { name: 'Gelée', make: (c) => vivid(new THREE.MeshPhysicalMaterial({ color: c, roughness: 0.1, metalness: 0, clearcoat: 1, transparent: true, opacity: 0.8, emissive: 0x000000, sheen: 1, sheenColor: new THREE.Color(0xffffff) }), 0.3) },
  toon: { name: 'Cartoon plat', make: (c) => vivid(new THREE.MeshToonMaterial({ color: c, gradientMap: toonRamp(), emissive: 0x000000 }), 0.1) },
};

let _ramp = null;
function toonRamp() {
  if (_ramp) return _ramp;
  const d = new Uint8Array([90, 170, 255]);
  _ramp = new THREE.DataTexture(d, 3, 1, THREE.RedFormat);
  _ramp.minFilter = _ramp.magFilter = THREE.NearestFilter; _ramp.needsUpdate = true;
  return _ramp;
}

const geoCache = {};
export function blockGeo(size = 0.92, radius = 0.15, seg = 3) {
  const k = `${size}|${radius}|${seg}`;
  return geoCache[k] || (geoCache[k] = new RoundedBoxGeometry(size, size, size, seg, radius));
}
export function boxGeo(w, h, d, r = 0.1, seg = 2) {
  const k = `b${w}|${h}|${d}|${r}|${seg}`;
  return geoCache[k] || (geoCache[k] = new RoundedBoxGeometry(w, h, d, seg, Math.min(r, w / 2, h / 2, d / 2)));
}

// ——— Matériau du monstre : uniformes partagés ———
export const monsterUniforms = { uTime: { value: 0 }, uGlitch: { value: 0 }, uRim: { value: 1 }, uRimColor: { value: new THREE.Color(0xff1a2a) } };

export function monsterMaterial(opts = {}) {
  const m = new THREE.MeshPhysicalMaterial({
    color: opts.color || 0x060608, roughness: 0.18, metalness: 0.1, clearcoat: 1, clearcoatRoughness: 0.05,
    emissive: 0x000000,
  });
  m.onBeforeCompile = (sh) => {
    sh.uniforms.uTime = monsterUniforms.uTime; sh.uniforms.uGlitch = monsterUniforms.uGlitch;
    sh.uniforms.uRim = monsterUniforms.uRim; sh.uniforms.uRimColor = monsterUniforms.uRimColor;
    sh.vertexShader = sh.vertexShader.replace('#include <common>', `#include <common>
      uniform float uTime; uniform float uGlitch;
      float mh(float n){ return fract(sin(n) * 43758.5453); }`)
      .replace('#include <begin_vertex>', `#include <begin_vertex>
      #ifdef USE_INSTANCING
        float seed = instanceMatrix[3][0] * 3.1 + instanceMatrix[3][1] * 7.7;
      #else
        float seed = modelMatrix[3][0] * 3.1 + modelMatrix[3][1] * 7.7 + modelMatrix[3][2];
      #endif
      float slice = floor(position.y * 6.0 + floor(uTime * 14.0));
      float g = step(1.0 - uGlitch * 0.35, mh(slice + seed + floor(uTime * 20.0)));
      transformed.x += g * (mh(slice * 1.7 + seed) - 0.5) * 0.6 * uGlitch;
      transformed += normal * sin(uTime * 3.0 + seed) * 0.015 * (1.0 + uGlitch * 4.0);`);
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', `#include <common>
      uniform float uRim; uniform vec3 uRimColor; uniform float uTime;`)
      .replace('#include <emissivemap_fragment>', `#include <emissivemap_fragment>
      float fres = pow(1.0 - clamp(dot(normal, normalize(vViewPosition)), 0.0, 1.0), 2.5);
      totalEmissiveRadiance += uRimColor * fres * uRim * (0.9 + 0.1 * sin(uTime * 7.0));`);
  };
  m.customProgramCacheKey = () => 'monster';
  return m;
}

export const eyeMaterial = new THREE.MeshBasicMaterial({ color: 0xff0010, toneMapped: false });
export const eyeCoreMaterial = new THREE.MeshBasicMaterial({ color: 0xffe0d0, toneMapped: false });
export const mouthMaterial = new THREE.MeshBasicMaterial({ color: 0xff1020, toneMapped: false });

// Texture de visage pour les blocs « vivants » / visage furtif
export function faceTexture(kind = 'stare') {
  const c = document.createElement('canvas'); c.width = c.height = 128;
  const g = c.getContext('2d');
  g.clearRect(0, 0, 128, 128);
  g.fillStyle = '#ff2030'; g.shadowColor = '#ff0000'; g.shadowBlur = 12;
  if (kind === 'stare') {
    g.fillRect(28, 40, 22, 26); g.fillRect(78, 40, 22, 26);
    g.fillStyle = '#ffe8e0'; g.shadowBlur = 0; g.fillRect(34, 44, 7, 8); g.fillRect(84, 44, 7, 8);
    g.strokeStyle = '#ff2030'; g.lineWidth = 6; g.shadowBlur = 8; g.beginPath();
    for (let i = 0; i <= 6; i++) g.lineTo(30 + i * 11.3, i % 2 ? 98 : 88); g.stroke();
  } else if (kind === 'cute') {
    g.fillStyle = '#ffffff'; g.shadowBlur = 0;
    g.beginPath(); g.arc(42, 58, 12, 0, 7); g.arc(86, 58, 12, 0, 7); g.fill();
    g.fillStyle = '#111'; g.beginPath(); g.arc(44, 60, 6, 0, 7); g.arc(88, 60, 6, 0, 7); g.fill();
    g.strokeStyle = '#111'; g.lineWidth = 5; g.beginPath(); g.arc(64, 80, 12, 0.2, Math.PI - 0.2); g.stroke();
  } else if (kind === 'scream') {
    g.fillRect(26, 30, 26, 18); g.fillRect(76, 30, 26, 18);
    g.fillStyle = '#000'; g.shadowBlur = 0; g.fillRect(40, 62, 48, 50);
    g.strokeStyle = '#ff2030'; g.lineWidth = 5; g.shadowBlur = 10; g.strokeRect(40, 62, 48, 50);
  }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
