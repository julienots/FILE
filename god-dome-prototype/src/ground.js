import * as THREE from 'three';
import { makeSimplex, mulberry32 } from './noise.js';

/*
 * Matériaux de sol réalistes, entièrement procéduraux :
 * - une texture de détail RGB (R = sol herbeux, G = terre / gravier, B = roche stratifiée) générée sur canvas ;
 * - mélange par sommet (roche / chemin / berge / neige) calculé lors de la sculpture du terrain ;
 * - roche en projection triplanaire (pas d'étirement sur les falaises) ;
 * - relief de surface (bump) calculé par dérivées écran : les cailloux, mottes et strates accrochent la lumière.
 */
export function makeDetailTexture(size = 512) {
  const sn = makeSimplex(321), r = mulberry32(17);
  const c = document.createElement('canvas'); c.width = c.height = size;
  const g = c.getContext('2d');
  const chan = () => new Float32Array(size * size);
  const tile = (fn) => { // bruit tuilable
    const out = chan();
    for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) {
      const u = x / size, v = y / size;
      const a = fn(u * size, v * size), b = fn((u - 1) * size, v * size), cc = fn(u * size, (v - 1) * size), d = fn((u - 1) * size, (v - 1) * size);
      out[y * size + x] = a * (1 - u) * (1 - v) + b * u * (1 - v) + cc * (1 - u) * v + d * u * v;
    }
    return out;
  };
  const fb = (x, y, f, o = 4) => { let s = 0, a = 0.5; for (let i = 0; i < o; i++) { s += a * sn(x * f, y * f); f *= 2.1; a *= 0.5; } return s; };

  // --- R : sol herbeux (mottes, brins couchés, trèfle) ---
  const R = tile((x, y) => 0.5 + fb(x, y, 0.02) * 0.35 + sn(x * 0.25, y * 0.25) * 0.08);
  // --- G : terre battue, gravier ---
  const G = tile((x, y) => 0.48 + fb(x, y, 0.015) * 0.25 + sn(x * 0.4, y * 0.4) * 0.1);
  // --- B : roche stratifiée + fissures ---
  const B = tile((x, y) => {
    const warp = fb(x, y, 0.01, 3) * 40;
    const strata = Math.sin((y + warp) * 0.11) * 0.5 + 0.5;
    const cr = 1 - Math.abs(sn(x * 0.03 + 5, y * 0.012)); // fissures
    return 0.42 + strata * 0.16 + fb(x, y, 0.03) * 0.25 - Math.pow(cr, 18) * 0.35;
  });
  // détails dessinés (brins, cailloux) sur canvas puis fusionnés
  const draw = (fn) => {
    g.fillStyle = '#808080'; g.fillRect(0, 0, size, size); fn();
    const d = g.getImageData(0, 0, size, size).data, out = chan();
    for (let i = 0; i < out.length; i++) out[i] = (d[i * 4] - 128) / 255;
    return out;
  };
  const strokes = draw(() => {
    for (let i = 0; i < 5200; i++) {
      const x = r() * size, y = r() * size, a = r() * 6.28, l = 3 + r() * 9, v = 90 + r() * 90;
      g.strokeStyle = `rgb(${v},${v},${v})`; g.lineWidth = 0.6 + r() * 1.2;
      for (const ox of [-size, 0, size]) for (const oy of [-size, 0, size]) { g.beginPath(); g.moveTo(x + ox, y + oy); g.lineTo(x + ox + Math.cos(a) * l, y + oy + Math.sin(a) * l); g.stroke(); }
    }
  });
  const pebbles = draw(() => {
    for (let i = 0; i < 900; i++) {
      const x = r() * size, y = r() * size, rr = 1.5 + r() ** 2 * 7, ang = r() * 3;
      for (const ox of [-size, 0, size]) for (const oy of [-size, 0, size]) {
        const gr = g.createRadialGradient(x + ox - rr * 0.3, y + oy - rr * 0.35, 0, x + ox, y + oy, rr);
        const v = 140 + r() * 80;
        gr.addColorStop(0, `rgb(${v + 30},${v + 30},${v + 30})`); gr.addColorStop(0.7, `rgb(${v - 30},${v - 30},${v - 30})`); gr.addColorStop(1, 'rgb(60,60,60)');
        g.fillStyle = gr; g.beginPath(); g.ellipse(x + ox, y + oy, rr, rr * (0.6 + r() * 0.4), ang, 0, 7); g.fill();
      }
    }
  });
  const img = g.createImageData(size, size);
  for (let i = 0; i < size * size; i++) {
    img.data[i * 4] = Math.max(0, Math.min(255, (R[i] + strokes[i] * 0.9) * 255));
    img.data[i * 4 + 1] = Math.max(0, Math.min(255, (G[i] + pebbles[i] * 0.85) * 255));
    img.data[i * 4 + 2] = Math.max(0, Math.min(255, B[i] * 255));
    img.data[i * 4 + 3] = 255;
  }
  g.putImageData(img, 0, 0);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.anisotropy = 8;
  t.colorSpace = THREE.NoColorSpace;
  return t;
}

const BUMP = /* glsl */`
{
  vec3 gd_vp = -vViewPosition;
  vec3 gd_dpx = dFdx(gd_vp), gd_dpy = dFdy(gd_vp);
  float gd_hx = dFdx(gdH), gd_hy = dFdy(gdH);
  vec3 gd_r1 = cross(gd_dpy, normal), gd_r2 = cross(normal, gd_dpx);
  float gd_det = dot(gd_dpx, gd_r1);
  vec3 gd_g = sign(gd_det) * (gd_hx * gd_r1 + gd_hy * gd_r2);
  normal = normalize(abs(gd_det) * normal - gd_g * uBump);
}`;

/** Shader de sol du terrain. */
export function applyGroundShader(material, detail) {
  material.onBeforeCompile = (s) => {
    s.uniforms.uDetail = { value: detail };
    s.uniforms.uBump = { value: 0.9 };
    s.vertexShader = s.vertexShader
      .replace('#include <common>', '#include <common>\nattribute vec4 aMat; varying vec4 vMat; varying vec3 vWPos; varying vec3 vWN;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvMat = aMat; vWPos = (modelMatrix * vec4(transformed, 1.0)).xyz; vWN = normalize(mat3(modelMatrix) * normal);');
    s.fragmentShader = s.fragmentShader
      .replace('#include <common>', `#include <common>
uniform sampler2D uDetail; uniform float uBump; varying vec4 vMat; varying vec3 vWPos; varying vec3 vWN;
float gd_h(vec2 p){ p = fract(p*vec2(123.34, 456.21)); p += dot(p, p+45.32); return fract(p.x*p.y); }
float gd_n(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
  return mix(mix(gd_h(i),gd_h(i+vec2(1,0)),f.x), mix(gd_h(i+vec2(0,1)),gd_h(i+vec2(1,1)),f.x), f.y); }`)
      .replace('void main() {', 'void main() {\n  float gdH = 0.5; vec3 gdBumpW = vec3(0.0);')
      .replace('#include <color_fragment>', `#include <color_fragment>
{
  float camD = length(cameraPosition - vWPos);
  vec3 dA = texture2D(uDetail, vWPos.xz * 0.42).rgb;
  vec3 dB = texture2D(uDetail, vWPos.xz * 0.085 + 0.37).rgb;
  vec3 dC = texture2D(uDetail, vWPos.xz * 0.9).rgb;
  float near = 1.0 - smoothstep(6.0, 22.0, camD);
  float grassD = mix(mix(dA.r, dB.r, 0.4), dC.r, 0.35 * near);
  float dirtD = mix(mix(dA.g, dB.g, 0.35), dC.g, 0.45 * near);
  vec3 bw = pow(abs(normalize(vWN)), vec3(4.0)); bw /= dot(bw, vec3(1.0));
  float rockD = texture2D(uDetail, vWPos.zy * vec2(0.09, 0.16)).b * bw.x + texture2D(uDetail, vWPos.xz * 0.09).b * bw.y + texture2D(uDetail, vWPos.xy * vec2(0.09, 0.16)).b * bw.z;
  rockD = mix(rockD, texture2D(uDetail, vWPos.xz * 0.37 + vWPos.y*0.1).b, 0.3);
  float rock = vMat.x, path = vMat.y, wet = vMat.z, snow = vMat.w;
  float d = mix(grassD, dirtD, clamp(max(path, wet * 0.8), 0.0, 1.0));
  d = mix(d, rockD, rock);
  d = mix(d, 0.5 + (grassD - 0.5) * 0.25, snow);
  gdH = d;
  {
    float e = 1.0 / 512.0;
    vec2 u0 = vWPos.xz * 0.42;
    float mixG = clamp(max(path, wet * 0.8), 0.0, 1.0);
    vec3 sx = texture2D(uDetail, u0 + vec2(e * 2.0, 0.0)).rgb, sz = texture2D(uDetail, u0 + vec2(0.0, e * 2.0)).rgb;
    float h0 = mix(dA.r, dA.g, mixG), hx = mix(sx.r, sx.g, mixG), hz = mix(sz.r, sz.g, mixG);
    float k = 2.2 * (1.0 - rock) * (1.0 - snow) * (1.0 - smoothstep(8.0, 30.0, camD));
    gdBumpW = vec3(-(hx - h0) * k, 0.0, -(hz - h0) * k);
  }
  float macro = 0.86 + 0.28 * gd_n(vWPos.xz * 0.045) - 0.08 * gd_n(vWPos.xz * 0.6);
  diffuseColor.rgb *= (0.42 + 1.16 * d) * macro;
  // berges humides plus sombres, chemins poussiéreux
  diffuseColor.rgb *= 1.0 - wet * 0.22 * (1.0 - rock);
  // occlusion au pied des pentes
  diffuseColor.rgb *= mix(1.0, 0.85, rock * (1.0 - bw.y));
}`)
      .replace('#include <roughnessmap_fragment>', `#include <roughnessmap_fragment>
roughnessFactor = mix(roughnessFactor, 0.55, vMat.z * 0.6 * (1.0 - vMat.x));`)
      .replace('#include <normal_fragment_maps>', '#include <normal_fragment_maps>\n normal = normalize(normal + (viewMatrix * vec4(gdBumpW, 0.0)).xyz);');
  };
  material.customProgramCacheKey = () => 'ground-v3';
  return material;
}

/** Détail de roche triplanaire pour les objets instanciés (rochers, troncs). */
export function applyRockDetail(material, detail, { scale = 0.6, channel = 'b', bump = 0.6, strength = 1.0, stretch = 1.0 } = {}) {
  material.onBeforeCompile = (s) => {
    s.uniforms.uDetail = { value: detail };
    s.uniforms.uBump = { value: bump };
    s.vertexShader = s.vertexShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vLPos; varying vec3 vLN;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvLPos = position; vLN = normal;');
    s.fragmentShader = s.fragmentShader
      .replace('#include <common>', '#include <common>\nuniform sampler2D uDetail; uniform float uBump; varying vec3 vLPos; varying vec3 vLN;')
      .replace('void main() {', 'void main() {\n  float gdH = 0.5;')
      .replace('#include <color_fragment>', `#include <color_fragment>
{
  vec3 bw = pow(abs(normalize(vLN)), vec3(4.0)); bw /= dot(bw, vec3(1.0));
  vec3 p = vLPos * ${scale.toFixed(3)} * vec3(1.0, ${stretch.toFixed(3)}, 1.0);
  float d = texture2D(uDetail, p.zy).${channel} * bw.x + texture2D(uDetail, p.xz).${channel} * bw.y + texture2D(uDetail, p.xy).${channel} * bw.z;
  d = mix(d, texture2D(uDetail, p.xz * 3.1 + p.y).${channel}, 0.3);
  gdH = d;
  diffuseColor.rgb *= mix(1.0, 0.4 + 1.2 * d, ${strength.toFixed(3)});
}`)
      ;
  };
  material.customProgramCacheKey = () => `rockdetail-${scale}-${channel}-${bump}-${strength}-${stretch}`;
  return material;
}
