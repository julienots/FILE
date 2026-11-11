// Matériaux PBR et motifs procéduraux (couleurs de sommets pour les corps organiques,
// textures dessinées pour les surfaces dures). Tout est exportable en glTF.
import * as THREE from "three";

export const PRESETS = {
  plain: "Uni", metal: "Métal", gold: "Or", rust: "Rouille", wood: "Bois", bark: "Écorce", stone: "Pierre",
  brick: "Briques", skin: "Peau", scales: "Écailles", fabric: "Tissu", leather: "Cuir", organic: "Organique",
  plastic: "Plastique", glass: "Verre", ice: "Glace", crystal: "Cristal", lava: "Lave", emission: "Lumineux",
  ground: "Sol / herbe", fur: "Fourrure", eye: "Œil",
};

const BASE = {
  plain: { r: 0.5, m: 0 }, metal: { r: 0.3, m: 1 }, gold: { r: 0.25, m: 1 }, rust: { r: 0.8, m: 0.3 }, wood: { r: 0.65, m: 0 },
  bark: { r: 0.9, m: 0 }, stone: { r: 0.85, m: 0 }, brick: { r: 0.85, m: 0 }, skin: { r: 0.5, m: 0 }, scales: { r: 0.35, m: 0.05 },
  fabric: { r: 0.9, m: 0 }, leather: { r: 0.55, m: 0 }, organic: { r: 0.4, m: 0 }, plastic: { r: 0.3, m: 0 },
  glass: { r: 0.05, m: 0 }, ice: { r: 0.15, m: 0 }, crystal: { r: 0.05, m: 0 }, lava: { r: 0.8, m: 0 },
  emission: { r: 0.3, m: 0 }, ground: { r: 0.95, m: 0 }, fur: { r: 0.85, m: 0 }, eye: { r: 0.1, m: 0 },
};
const DEFAULT_COLOR = {
  wood: [0.5, 0.3, 0.15], bark: [0.3, 0.2, 0.12], stone: [0.5, 0.48, 0.45], brick: [0.6, 0.27, 0.17], metal: [0.65, 0.66, 0.68],
  gold: [1, 0.72, 0.25], rust: [0.55, 0.3, 0.15], skin: [0.85, 0.6, 0.48], scales: [0.15, 0.35, 0.15], organic: [0.45, 0.2, 0.25],
  fabric: [0.4, 0.15, 0.15], leather: [0.35, 0.2, 0.1], glass: [0.85, 0.92, 1], ice: [0.75, 0.9, 1], crystal: [0.5, 0.3, 0.9],
  lava: [0.08, 0.04, 0.03], ground: [0.25, 0.4, 0.12], fur: [0.45, 0.33, 0.22], emission: [0.2, 0.8, 1], plastic: [0.8, 0.1, 0.1],
  plain: [0.75, 0.75, 0.75], eye: [0.95, 0.75, 0.1],
};

const col = (c) => new THREE.Color().setRGB(c[0], c[1], c[2], THREE.SRGBColorSpace);

export function makeMaterial(preset = "plain", p = {}) {
  const b = BASE[preset] || BASE.plain;
  const c = p.color || DEFAULT_COLOR[preset] || DEFAULT_COLOR.plain;
  const isGlass = ["glass", "ice", "crystal"].includes(preset);
  const physical = isGlass || ["skin", "scales", "organic", "leather", "fur", "fabric", "eye", "plastic"].includes(preset);
  const M = physical ? THREE.MeshPhysicalMaterial : THREE.MeshStandardMaterial;
  const m = new M({
    color: col(c), roughness: p.roughness ?? b.r, metalness: p.metallic ?? b.m,
    side: p.doubleSide ? THREE.DoubleSide : THREE.FrontSide,
  });
  if (preset === "scales") { m.clearcoat = 0.35; m.clearcoatRoughness = 0.4; }
  if (preset === "skin") { m.sheen = 0.25; m.sheenRoughness = 0.7; m.sheenColor = new THREE.Color(1, 0.75, 0.65); m.roughness = Math.min(m.roughness, 0.55); }
  if (preset === "organic") { m.clearcoat = 0.5; m.clearcoatRoughness = 0.3; }
  if (preset === "fur" || preset === "fabric") { m.sheen = 0.8; m.sheenRoughness = 0.5; m.sheenColor = col(c).multiplyScalar(1.4); }
  if (preset === "eye") { m.clearcoat = 1; m.clearcoatRoughness = 0.03; m.roughness = 0.35; }
  if (isGlass) { m.transmission = 0.9; m.thickness = 0.2; m.ior = preset === "ice" ? 1.31 : 1.5; m.transparent = true; m.opacity = 0.85; }
  if (p.wet) { m.roughness *= 1 - 0.7 * p.wet; if (m.isMeshPhysicalMaterial) m.clearcoat = 1; }
  const glow = p.emission || (["emission", "lava"].includes(preset) ? (preset === "lava" ? [1, 0.35, 0.03] : c) : null);
  if (glow && (p.emission_strength ?? 1) > 0) { m.emissive = col(glow); m.emissiveIntensity = p.emission_strength ?? (preset === "lava" ? 1.5 : 2); }
  m.userData = { preset, params: { ...p, color: c } };
  m.name = PRESETS[preset] || preset;
  if (p.detail !== false) addDetail(m, preset, { scale: p.detailScale, bump: p.detailBump });
  if (["skin", "organic", "scales", "leather"].includes(preset) && m.isMeshPhysicalMaterial === false) {
    m.roughness = Math.min(m.roughness, 0.6);
  }
  return m;
}

// ---------------------------------------------------------------- bruit
function hash3(x, y, z) {
  let h = (x * 374761393 + y * 668265263 + z * 2147483647) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967295;
}
function vnoise(x, y, z) {
  const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z);
  const xf = x - xi, yf = y - yi, zf = z - zi;
  const u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf), w = zf * zf * (3 - 2 * zf);
  const L = (a, b, t) => a + (b - a) * t;
  return L(L(L(hash3(xi, yi, zi), hash3(xi + 1, yi, zi), u), L(hash3(xi, yi + 1, zi), hash3(xi + 1, yi + 1, zi), u), v),
    L(L(hash3(xi, yi, zi + 1), hash3(xi + 1, yi, zi + 1), u), L(hash3(xi, yi + 1, zi + 1), hash3(xi + 1, yi + 1, zi + 1), u), v), w);
}
export function fbm(x, y, z, oct = 4) {
  let a = 0, f = 1, amp = 0.5, s = 0;
  for (let i = 0; i < oct; i++) { a += vnoise(x * f, y * f, z * f) * amp; s += amp; f *= 2.03; amp *= 0.5; }
  return a / s;
}
export function voronoi(x, y, z) {
  const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z);
  let d1 = 9, d2 = 9;
  for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) for (let k = -1; k <= 1; k++) {
    const cx = xi + i, cy = yi + j, cz = zi + k;
    const px = cx + hash3(cx, cy, cz), py = cy + hash3(cy, cz, cx), pz = cz + hash3(cz, cx, cy);
    const d = Math.hypot(px - x, py - y, pz - z);
    if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
  }
  return [d1, d2];
}

/** Motif procédural en couleurs de sommets (+ relief léger le long des normales). */
export function applyPattern(geometry, preset, p = {}, { scale = 1, relief = 0 } = {}) {
  const pos = geometry.attributes.position, nrm = geometry.attributes.normal;
  const n = pos.count;
  const c1 = p.color || DEFAULT_COLOR[preset] || DEFAULT_COLOR.plain;
  const c2 = p.color2 || c1.map((x) => x * 0.45);
  const vein = p.veins || null;
  const out = new Float32Array(n * 3);
  const tmp = new THREE.Color();
  const s = scale;
  for (let i = 0; i < n; i++) {
    const x = pos.getX(i) * s, y = pos.getY(i) * s, z = pos.getZ(i) * s;
    let t = 0.5, v = 0, h = 0;
    switch (preset) {
      case "scales": { const [d1, d2] = voronoi(x * 9, y * 9, z * 9); t = Math.min(1, (d2 - d1) * 2.2); h = t; t = 1 - t; break; }
      case "stone": case "ground": { t = fbm(x * 3, y * 3, z * 3, 5); const [d1, d2] = voronoi(x * 2, y * 2, z * 2); if (d2 - d1 < 0.05) t *= 0.5; h = t; break; }
      case "wood": case "bark": { const r = Math.hypot(x, z) * 14 + fbm(x * 2, y * 0.5, z * 2) * 4; t = 0.5 + 0.5 * Math.sin(r); h = t; break; }
      case "fur": t = fbm(x * 18, y * 4, z * 18, 3); break;
      case "rust": t = fbm(x * 3, y * 3, z * 3, 5) > 0.55 ? 0.1 : 0.9; break;
      case "lava": { const [d1, d2] = voronoi(x * 4, y * 4, z * 4); t = d2 - d1 < 0.12 ? 0 : 1; break; }
      case "organic": case "skin": case "leather": {
        t = fbm(x * 5, y * 5, z * 5, 4);
        if (vein || preset === "organic") { const [d1, d2] = voronoi(x * 2.5 + t, y * 2.5, z * 2.5); v = d2 - d1 < 0.04 ? 1 : 0; }
        h = t; break;
      }
      default: t = 0.5 + (fbm(x * 6, y * 6, z * 6, 2) - 0.5) * 0.3;
    }
    const k = Math.max(0, Math.min(1, t));
    let r = c2[0] + (c1[0] - c2[0]) * k, g = c2[1] + (c1[1] - c2[1]) * k, b = c2[2] + (c1[2] - c2[2]) * k;
    if (preset === "lava" && k < 0.5) { r = 1; g = 0.35; b = 0.05; }
    if (v && (vein || preset === "organic")) { const vc = vein || c1.map((q) => Math.min(1, q * 1.6 + 0.05)); r = vc[0]; g = vc[1]; b = vc[2]; }
    tmp.setRGB(r, g, b, THREE.SRGBColorSpace);
    out[i * 3] = tmp.r; out[i * 3 + 1] = tmp.g; out[i * 3 + 2] = tmp.b;
    if (relief && nrm) {
      const o = (h - 0.5) * relief;
      pos.setXYZ(i, pos.getX(i) + nrm.getX(i) * o, pos.getY(i) + nrm.getY(i) * o, pos.getZ(i) + nrm.getZ(i) * o);
    }
  }
  geometry.setAttribute("color", new THREE.BufferAttribute(out, 3));
  if (relief) geometry.computeVertexNormals();
}

/** Texture dessinée (briques, planches, tuiles, dalles, panneaux métal) pour les surfaces dures. */
export function canvasTexture(kind, c1, c2, repeat = [1, 1]) {
  const S = 256;
  const cv = document.createElement("canvas");
  cv.width = cv.height = S;
  const g = cv.getContext("2d");
  const hex = (c, k = 1) => `rgb(${Math.round(Math.min(1, c[0] * k) * 255)},${Math.round(Math.min(1, c[1] * k) * 255)},${Math.round(Math.min(1, c[2] * k) * 255)})`;
  g.fillStyle = hex(c2);
  g.fillRect(0, 0, S, S);
  const rnd = (i) => hash3(i, 7, 13);
  if (kind === "brick") {
    const bh = S / 8, bw = S / 4;
    for (let r = 0; r < 8; r++) for (let c = -1; c < 5; c++) {
      g.fillStyle = hex(c1, 0.8 + rnd(r * 9 + c) * 0.4);
      g.fillRect(c * bw + (r % 2) * bw / 2 + 3, r * bh + 3, bw - 6, bh - 6);
    }
  } else if (kind === "planks" || kind === "wood") {
    const n = 6, w = S / n;
    for (let i = 0; i < n; i++) {
      g.fillStyle = hex(c1, 0.8 + rnd(i) * 0.4);
      g.fillRect(i * w + 2, 0, w - 4, S);
      g.strokeStyle = hex(c1, 0.6);
      for (let k = 0; k < 6; k++) { g.beginPath(); const x = i * w + 6 + rnd(i * 10 + k) * (w - 12); g.moveTo(x, 0); g.bezierCurveTo(x + 6, S / 3, x - 6, S * 2 / 3, x, S); g.stroke(); }
    }
  } else if (kind === "tiles") {
    for (let r = 0; r < 8; r++) for (let c = 0; c < 8; c++) {
      g.fillStyle = hex(c1, 0.75 + rnd(r * 8 + c) * 0.5);
      g.beginPath(); g.arc(c * 32 + 16 + (r % 2) * 16, r * 32 + 8, 18, 0, Math.PI); g.fill();
    }
  } else if (kind === "stone") {
    for (let i = 0; i < 40; i++) {
      g.fillStyle = hex(c1, 0.7 + rnd(i) * 0.5);
      const x = rnd(i * 3) * S, y = rnd(i * 5) * S, w = 30 + rnd(i * 7) * 40, h = 20 + rnd(i * 11) * 30;
      g.fillRect(x, y, w, h); g.fillRect(x - S, y, w, h); g.fillRect(x, y - S, w, h);
    }
  } else if (kind === "metal") {
    g.fillStyle = hex(c1); g.fillRect(0, 0, S, S);
    g.strokeStyle = hex(c1, 0.6); g.lineWidth = 3; g.strokeRect(4, 4, S - 8, S - 8);
    g.fillStyle = hex(c1, 0.5);
    for (const [x, y] of [[14, 14], [S - 14, 14], [14, S - 14], [S - 14, S - 14]]) { g.beginPath(); g.arc(x, y, 4, 0, 7); g.fill(); }
  }
  const t = new THREE.CanvasTexture(cv);
  t.colorSpace = THREE.SRGBColorSpace;
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(repeat[0], repeat[1]);
  t.anisotropy = 4;
  return t;
}

/** Projection UV « boîte » (pour appliquer une texture dessinée à n'importe quel maillage). */
export function boxUV(geometry, scale = 1) {
  const p = geometry.attributes.position, n = geometry.attributes.normal;
  const uv = new Float32Array(p.count * 2);
  for (let i = 0; i < p.count; i++) {
    const ax = Math.abs(n.getX(i)), ay = Math.abs(n.getY(i)), az = Math.abs(n.getZ(i));
    let u, v;
    if (ay >= ax && ay >= az) { u = p.getX(i); v = p.getZ(i); } else if (ax >= az) { u = p.getZ(i); v = p.getY(i); } else { u = p.getX(i); v = p.getY(i); }
    uv[i * 2] = u * scale; uv[i * 2 + 1] = v * scale;
  }
  geometry.setAttribute("uv", new THREE.BufferAttribute(uv, 2));
}

// ------------------------------------------------------------------ détail réaliste au pixel (shader)
// Relief (normale perturbée), variation de couleur et de rugosité calculés par pixel dans l'espace de
// l'objet : les écailles, pores et grains restent collés à la peau même pendant l'animation.
const DETAIL = {
  scales: { scale: 18, bump: 0.8, color: 0.28, rough: 0.3, mode: 1 },
  skin: { scale: 22, bump: 0.12, color: 0.06, rough: 0.2, mode: 0 },
  organic: { scale: 30, bump: 0.6, color: 0.22, rough: 0.3, mode: 2 },
  leather: { scale: 30, bump: 0.35, color: 0.12, rough: 0.3, mode: 1 },
  stone: { scale: 6, bump: 1.0, color: 0.35, rough: 0.2, mode: 3 },
  bark: { scale: 9, bump: 1.2, color: 0.35, rough: 0.2, mode: 4 },
  wood: { scale: 14, bump: 0.25, color: 0.25, rough: 0.2, mode: 4 },
  fur: { scale: 40, bump: 0.4, color: 0.18, rough: 0.15, mode: 5 },
  fabric: { scale: 60, bump: 0.3, color: 0.1, rough: 0.1, mode: 6 },
  metal: { scale: 20, bump: 0.1, color: 0.05, rough: 0.4, mode: 7 },
  rust: { scale: 10, bump: 0.6, color: 0.3, rough: 0.3, mode: 3 },
  ground: { scale: 4, bump: 0.8, color: 0.35, rough: 0.2, mode: 3 },
  brick: { scale: 8, bump: 0.35, color: 0.15, rough: 0.2, mode: 3 },
  gold: { scale: 40, bump: 0.08, color: 0.04, rough: 0.4, mode: 7 },
  plastic: { scale: 30, bump: 0.05, color: 0.03, rough: 0.2, mode: 0 },
};

const NOISE_GLSL = `
varying vec3 vSeObj;
uniform float seScale; uniform float seBump; uniform float seColor; uniform float seRough; uniform int seMode;
float seHash(vec3 p){ p = fract(p*0.3183099 + 0.1); p *= 17.0; return fract(p.x*p.y*p.z*(p.x+p.y+p.z)); }
float seNoise(vec3 x){ vec3 i = floor(x); vec3 f = fract(x); f = f*f*(3.0-2.0*f);
  return mix(mix(mix(seHash(i+vec3(0,0,0)),seHash(i+vec3(1,0,0)),f.x), mix(seHash(i+vec3(0,1,0)),seHash(i+vec3(1,1,0)),f.x),f.y),
             mix(mix(seHash(i+vec3(0,0,1)),seHash(i+vec3(1,0,1)),f.x), mix(seHash(i+vec3(0,1,1)),seHash(i+vec3(1,1,1)),f.x),f.y), f.z); }
float seFbm(vec3 p){ float a = 0.0, w = 0.5; for (int i = 0; i < 5; i++){ a += w*seNoise(p); p *= 2.03; w *= 0.5; } return a; }
vec2 seVoronoi(vec3 x){ vec3 p = floor(x); vec3 f = fract(x); float d1 = 8.0, d2 = 8.0;
  for (int k=-1;k<=1;k++) for (int j=-1;j<=1;j++) for (int i=-1;i<=1;i++){ vec3 b = vec3(float(i),float(j),float(k));
    vec3 r = b - f + vec3(seHash(p+b), seHash(p+b+17.3), seHash(p+b+41.7)); float d = dot(r,r);
    if (d < d1){ d2 = d1; d1 = d; } else if (d < d2) d2 = d; }
  return vec2(sqrt(d1), sqrt(d2)); }
float seHeight(vec3 p){
  if (seMode == 1){ vec2 v = seVoronoi(p); float edge = smoothstep(0.0, 0.18, v.y - v.x); return edge*0.85 + seNoise(p*3.0)*0.15; }
  if (seMode == 2){ vec2 v = seVoronoi(p*0.4 + seFbm(p*0.3)); return seFbm(p)*0.6 + smoothstep(0.0, 0.06, v.y - v.x)*0.4; }
  if (seMode == 3){ vec2 v = seVoronoi(p*0.6); return seFbm(p)*0.7 + smoothstep(0.0, 0.08, v.y - v.x)*0.3; }
  if (seMode == 4){ return 0.5 + 0.5*sin(length(p.xz)*3.0 + seFbm(p*vec3(1.0,0.15,1.0))*6.0) * 0.6 + seNoise(p*vec3(4.0,0.3,4.0))*0.4; }
  if (seMode == 5){ return seNoise(p*vec3(1.0,0.12,1.0)) * 0.7 + seNoise(p*2.7)*0.3; }
  if (seMode == 6){ return abs(sin(p.x*3.14)) * abs(sin(p.y*3.14 + 1.57)) * 0.7 + seNoise(p)*0.3; }
  if (seMode == 7){ return seNoise(p*vec3(0.05,1.0,1.0))*0.6 + seNoise(p*vec3(3.0,0.2,3.0))*0.4; }
  return seFbm(p);
}`;

export function addDetail(material, kind, opts = {}) {
  const d = DETAIL[kind];
  if (!d || material.userData.seDetail) return material;
  const u = { seScale: { value: (opts.scale ?? 1) * d.scale }, seBump: { value: (opts.bump ?? 1) * d.bump }, seColor: { value: d.color }, seRough: { value: d.rough }, seMode: { value: d.mode } };
  material.userData.seDetail = { kind, uniforms: u };
  material.onBeforeCompile = (shader) => {
    Object.assign(shader.uniforms, u);
    shader.vertexShader = shader.vertexShader.replace("#include <common>", "#include <common>\nvarying vec3 vSeObj;")
      .replace("#include <begin_vertex>", "#include <begin_vertex>\nvSeObj = position;");
    shader.fragmentShader = shader.fragmentShader.replace("#include <common>", "#include <common>\n" + NOISE_GLSL)
      .replace("#include <color_fragment>", `#include <color_fragment>
        float seFw = length(fwidth(vSeObj * seScale));
        float seFade = 1.0 - smoothstep(0.18, 0.6, seFw);
        float seH = mix(0.5, seHeight(vSeObj * seScale), seFade);
        float seC = seFbm(vSeObj * seScale * 0.15);
        diffuseColor.rgb *= mix(1.0 - seColor, 1.0 + seColor * 0.6, mix(seH, seC, 0.5 + 0.5 * (1.0 - seFade)));`)
      .replace("#include <roughnessmap_fragment>", `#include <roughnessmap_fragment>
        roughnessFactor = clamp(roughnessFactor * mix(1.0 + seRough, 1.0 - seRough, seH), 0.03, 1.0);`)
      .replace("#include <normal_fragment_maps>", `#include <normal_fragment_maps>
        {
          float hC = seHeight(vSeObj * seScale);
          vec2 dHdxy = vec2(dFdx(hC), dFdy(hC)) * seBump * 0.08 * seFade;
          vec3 surfPos = -vViewPosition;
          vec3 sx = dFdx(surfPos), sy = dFdy(surfPos);
          vec3 R1 = cross(sy, normal), R2 = cross(normal, sx);
          float det = dot(sx, R1) * faceDirection;
          vec3 grad = sign(det) * (dHdxy.x * R1 + dHdxy.y * R2);
          normal = normalize(abs(det) * normal - grad);
        }`);
  };
  material.customProgramCacheKey = () => `se_detail_${kind}`;
  material.needsUpdate = true;
  return material;
}

/** Œil réaliste en couleurs de sommets : pupille, iris strié, anneau limbal, sclère légèrement veinée. */
export function eyeGeometry(radius, look, irisColor = [0.3, 0.55, 0.2], glow = false) {
  const g = new THREE.SphereGeometry(radius, 40, 28);
  const p = g.attributes.position, n = p.count;
  const out = new Float32Array(n * 3);
  const L = look.clone().normalize();
  const iris = new THREE.Color().setRGB(irisColor[0], irisColor[1], irisColor[2], THREE.SRGBColorSpace);
  const dark = iris.clone().multiplyScalar(0.35), light = iris.clone().lerp(new THREE.Color(1, 1, 1), 0.25);
  const sclera = new THREE.Color().setRGB(0.93, 0.9, 0.86, THREE.SRGBColorSpace), vein = new THREE.Color().setRGB(0.8, 0.35, 0.35, THREE.SRGBColorSpace);
  const v = new THREE.Vector3(), c = new THREE.Color();
  // repère pour l'angle autour de l'axe du regard (stries radiales)
  const up = Math.abs(L.y) < 0.9 ? new THREE.Vector3(0, 1, 0) : new THREE.Vector3(1, 0, 0);
  const ax = up.clone().cross(L).normalize(), ay = L.clone().cross(ax);
  for (let i = 0; i < n; i++) {
    v.fromBufferAttribute(p, i).normalize();
    const cosA = v.dot(L), ang = Math.acos(Math.max(-1, Math.min(1, cosA)));
    const phi = Math.atan2(v.dot(ay), v.dot(ax));
    if (ang < 0.2) c.setRGB(0.01, 0.01, 0.012);
    else if (ang < 0.55) {
      const t = (ang - 0.2) / 0.35;
      const streak = 0.5 + 0.5 * Math.sin(phi * 38 + Math.sin(phi * 7) * 2);
      c.copy(light).lerp(dark, t * 0.6 + streak * 0.3);
      if (t > 0.82) c.lerp(new THREE.Color(0.02, 0.02, 0.02), (t - 0.82) / 0.18 * 0.85);
    } else {
      c.copy(sclera);
      const vv = Math.abs(Math.sin(phi * 9 + ang * 6)) < 0.04 && ang > 1.2 ? 0.45 : 0;
      c.lerp(vein, vv);
      if (ang > 2.2) c.multiplyScalar(0.8);
    }
    if (glow) c.copy(iris).lerp(new THREE.Color(1, 1, 1), ang < 0.55 ? 0.3 : 0);
    out[i * 3] = c.r; out[i * 3 + 1] = c.g; out[i * 3 + 2] = c.b;
  }
  g.setAttribute("color", new THREE.BufferAttribute(out, 3));
  return g;
}

export function eyeMaterial(irisColor, glow) {
  const m = makeMaterial("eye", { color: [1, 1, 1], detail: false });
  m.vertexColors = true;
  if (glow) { m.emissive = new THREE.Color().setRGB(irisColor[0], irisColor[1], irisColor[2], THREE.SRGBColorSpace); m.emissiveIntensity = 2.5; }
  return m;
}
