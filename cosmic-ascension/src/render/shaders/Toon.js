import * as THREE from 'three';
import { NOISE_GLSL } from './noise.glsl.js';
// Matériau cartoon: ramp 4 tons + lumière de contour (rim) + balancement au vent optionnel (végétation, instancing OK).
// Basé sur MeshToonMaterial (lumières/fog/instancing) + injection GLSL. `shaderTime` est mis à jour par Game à chaque frame.
export const shaderTime = { value: 0 };
// Animation corporelle SANS squelette (les GLB Tripo n'ont ni os ni clips): déformation en vertex shader par zones du corps.
// uAnim = (marche 0..1, phase du pas, en l'air 0..1, attaque 0..1). anim: {type:'biped'|'blob'|'serpent'|'hover', side:'x'|'z', fwd:±1,
// tpose: angle (rad) pour rabattre des bras en croix, armAt: début des bras (fraction de la demi-largeur), shoulder/hip: hauteurs relatives, min/max: boîte locale}.
function animGLSL(a) {
  const S = a.side === 'z' ? 'z' : 'x', F = S === 'x' ? 'z' : 'x', fwd = (a.fwd ?? 1).toFixed(1), f = (v, d) => (v ?? d).toFixed(3);
  const head = `
        { vec3 bsz = uBMax - uBMin; float h = (transformed.y - uBMin.y) / bsz.y; float halfW = .5 * bsz.${S}, cS = uBMin.${S} + halfW;
          float sd = transformed.${S} - cS, fw = transformed.${F} * ${fwd}, yy = transformed.y; float sN = sd / halfW;
          float walk = uAnim.x, ph = uAnim.y, air = uAnim.z, atk = uAnim.w;`;
  const tail = `
          transformed.${S} = sd + cS; transformed.${F} = fw * ${fwd}; transformed.y = yy; }`;
  if (a.type === 'blob') return head + `
          float wob = sin(uTime * 5. + h * 4.) * .03 * (1. + walk * 2.); sd *= 1. + wob; fw *= 1. + wob;
          float sq = sin(ph * 2.) * .09 * walk + sin(uTime * 2.2) * .02 - atk * .12; yy = uBMin.y + (yy - uBMin.y) * (1. + sq); sd *= 1. - sq * .5; fw = fw * (1. - sq * .5) + h * (walk * .16 + atk * .3) * bsz.y;` + tail;
  if (a.type === 'serpent') return head + `
          float along = fw / (.5 * bsz.${F}); sd += sin(along * 4.5 - uTime * 3.5 - ph) * .1 * bsz.${S} * (.3 + walk) * smoothstep(-1., .2, -along);
          yy += sin(along * 3. - uTime * 2.5) * .03 * bsz.y + atk * max(0., along) * .2 * bsz.y;` + tail;
  if (a.type === 'hover') return head + `
          yy += sin(uTime * 3.) * .04 * bsz.y; vec2 tl = rot2(vec2(fw, yy), walk * .18 + atk * .35); fw = tl.x; yy = tl.y; sd *= 1. + sin(uTime * 9.) * .01;` + tail;
  return head + `
          float sg = sN > 0. ? 1. : -1., pYs = uBMin.y + ${f(a.shoulder, .72)} * bsz.y, pYh = uBMin.y + ${f(a.hip, .46)} * bsz.y;
          // bras: rabattus depuis la pose en croix, balancier oppose aux jambes, coup de poing a l'attaque, leves en l'air
          float armW = smoothstep(${f(a.armAt, .3)}, ${f(a.armAt, .3)} + .16, abs(sN)) * smoothstep(${f(a.hip, .46)} - .06, ${f(a.hip, .46)} + .08, h);
          vec2 d1 = vec2(sd - sg * ${f(a.armAt, .3)} * halfW, yy - pYs); d1 = mix(d1, rot2(d1, -sg * (${f(a.tpose, 0)} * (1. - air * .7) + sin(uTime * 1.7 + sg) * .05)), armW);
          sd = d1.x + sg * ${f(a.armAt, .3)} * halfW; yy = d1.y + pYs;
          float aSw = (sin(ph + (sg > 0. ? 3.1416 : 0.)) * .75 * walk - (sg > 0. ? atk * 1.7 : 0.)) * armW; vec2 d2 = rot2(vec2(fw, yy - pYs), aSw); fw = d2.x; yy = d2.y + pYs;
          // jambes: pas alternes autour des hanches (melange doux a l'entrejambe)
          float legW = 1. - smoothstep(${f(a.hip, .46)} - .1, ${f(a.hip, .46)} + .04, h), lr = smoothstep(-.1, .1, sN) * 2. - 1.;
          vec2 d3 = rot2(vec2(fw, yy - pYh), (sin(ph) * .8 * walk + air * .35) * lr * legW); fw = d3.x; yy = d3.y + pYh;
          // buste: torsion, inclinaison vers l'avant, respiration
          float up = smoothstep(${f(a.hip, .46)}, .95, h); vec2 d4 = rot2(vec2(sd, fw), sin(ph) * .13 * walk * up); sd = d4.x; fw = d4.y;
          fw += (walk * .1 + atk * .08) * bsz.y * up; sd *= 1. + sin(uTime * 2.) * .012 * up;` + tail;
}
let ramp;
function getRamp(steps = 4) {
  if (ramp) return ramp;
  const d = new Uint8Array(steps); for (let i = 0; i < steps; i++) d[i] = [64, 118, 184, 255][i] ?? 255;   // ombres profondes (direction artistique sombre)
  ramp = new THREE.DataTexture(d, steps, 1, THREE.RedFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
  return ramp;
}
export function toonMaterial({ color = '#ffffff', vertexColors = false, rim = '#9fd8ff', rimPower = 2.5, rimStrength = 0.55,
  emissive = '#000000', emissiveIntensity = 1, flatShading = false, sway = 0, detail = 0, anim = null, animU = null, ...rest } = {}) {
  // Direction « SF réaliste »: base PBR (MeshStandardMaterial) au lieu de la rampe toon; le nom `toonMaterial` est gardé pour compatibilité.
  const m = new THREE.MeshStandardMaterial({ color, vertexColors, emissive, emissiveIntensity, flatShading, roughness: rest.roughness ?? .82, metalness: rest.metalness ?? .04, ...rest });
  const A = anim && animGLSL(anim);
  const u = { ...(A ? { uAnim: animU?.uAnim ?? { value: new THREE.Vector4() }, uBMin: { value: new THREE.Vector3().fromArray(anim.min) }, uBMax: { value: new THREE.Vector3().fromArray(anim.max) } } : {}), uRim: { value: new THREE.Color(rim) }, uRimP: { value: rimPower }, uRimS: { value: rimStrength }, uSway: { value: sway }, uTime: shaderTime };
  m.userData.uniforms = u;
  m.onBeforeCompile = (s) => {
    Object.assign(s.uniforms, u);
    s.vertexShader = s.vertexShader
      .replace('#include <common>', '#include <common>\nuniform float uSway,uTime; varying vec3 vObjP;' + (A ? '\nuniform vec4 uAnim; uniform vec3 uBMin,uBMax; vec2 rot2(vec2 v,float a){float c=cos(a),s=sin(a);return vec2(c*v.x-s*v.y,s*v.x+c*v.y);}' : ''))
      .replace('#include <begin_vertex>', `#include <begin_vertex>
        vObjP = position;
        #ifdef USE_INSTANCING
          float swPh = instanceMatrix[3].x * .7 + instanceMatrix[3].z * .9 + instanceMatrix[3].y * .5;
        #else
          float swPh = 0.;
        #endif
        float swW = uSway * max(0., position.y);
        transformed.x += sin(uTime * 1.7 + swPh) * swW; transformed.z += cos(uTime * 1.3 + swPh * 1.3) * swW;` + (A ?? ''));
    s.fragmentShader = s.fragmentShader
      .replace('#include <common>', '#include <common>\nuniform vec3 uRim; uniform float uRimP,uRimS; varying vec3 vObjP;' + (detail ? NOISE_GLSL : ''))
      .replace('#include <color_fragment>', '#include <color_fragment>\n        ' + (detail ? `
        float dn = vnoise(vObjP * .35) * .5 + vnoise(vObjP * 1.3) * .3 + vnoise(vObjP * 5.) * .2;
        diffuseColor.rgb *= .72 + .56 * dn;` : ''))
      .replace('#include <opaque_fragment>', `
        float rimF = pow(1.0 - saturate(dot(normalize(vViewPosition), normal)), uRimP);
        outgoingLight += uRim * rimF * uRimS * 0.3 * (0.2 + 0.8 * saturate(dot(normal, directionalLights[0].direction) + .4));
        #include <opaque_fragment>`);
  };
  m.customProgramCacheKey = () => 'toon-rim-sway' + (detail ? '-d' : '') + (A ? '-a' + JSON.stringify([anim.type, anim.side, anim.fwd, anim.tpose, anim.armAt, anim.shoulder, anim.hip]) : '');
  return m;
}
// Contour "inverted hull" — à réserver au héros / créatures importantes.
// Contours retirés (direction réaliste): fonction conservée pour compatibilité, sans effet.
export function addOutline() { return null; }
