import * as THREE from 'three';
import { NOISE_GLSL } from './noise.glsl.js';
// Matériau cartoon: ramp 4 tons + lumière de contour (rim) + balancement au vent optionnel (végétation, instancing OK).
// Basé sur MeshToonMaterial (lumières/fog/instancing) + injection GLSL. `shaderTime` est mis à jour par Game à chaque frame.
export const shaderTime = { value: 0 };
let ramp;
function getRamp(steps = 4) {
  if (ramp) return ramp;
  const d = new Uint8Array(steps); for (let i = 0; i < steps; i++) d[i] = [64, 118, 184, 255][i] ?? 255;   // ombres profondes (direction artistique sombre)
  ramp = new THREE.DataTexture(d, steps, 1, THREE.RedFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
  return ramp;
}
export function toonMaterial({ color = '#ffffff', vertexColors = false, rim = '#9fd8ff', rimPower = 2.5, rimStrength = 0.55,
  emissive = '#000000', emissiveIntensity = 1, flatShading = false, sway = 0, detail = 0, ...rest } = {}) {
  // Direction « SF réaliste »: base PBR (MeshStandardMaterial) au lieu de la rampe toon; le nom `toonMaterial` est gardé pour compatibilité.
  const m = new THREE.MeshStandardMaterial({ color, vertexColors, emissive, emissiveIntensity, flatShading, roughness: rest.roughness ?? .82, metalness: rest.metalness ?? .04, ...rest });
  const u = { uRim: { value: new THREE.Color(rim) }, uRimP: { value: rimPower }, uRimS: { value: rimStrength }, uSway: { value: sway }, uTime: shaderTime };
  m.userData.uniforms = u;
  m.onBeforeCompile = (s) => {
    Object.assign(s.uniforms, u);
    s.vertexShader = s.vertexShader
      .replace('#include <common>', '#include <common>\nuniform float uSway,uTime; varying vec3 vObjP;')
      .replace('#include <begin_vertex>', `#include <begin_vertex>
        vObjP = position;
        #ifdef USE_INSTANCING
          float swPh = instanceMatrix[3].x * .7 + instanceMatrix[3].z * .9 + instanceMatrix[3].y * .5;
        #else
          float swPh = 0.;
        #endif
        float swW = uSway * max(0., position.y);
        transformed.x += sin(uTime * 1.7 + swPh) * swW; transformed.z += cos(uTime * 1.3 + swPh * 1.3) * swW;`);
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
  m.customProgramCacheKey = () => 'toon-rim-sway' + (detail ? '-d' : '');
  return m;
}
// Contour "inverted hull" — à réserver au héros / créatures importantes.
// Contours retirés (direction réaliste): fonction conservée pour compatibilité, sans effet.
export function addOutline() { return null; }
