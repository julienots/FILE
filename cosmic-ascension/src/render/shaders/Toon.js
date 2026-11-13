import * as THREE from 'three';
import { NOISE_GLSL } from './noise.glsl.js';
// Matériau cartoon: ramp 4 tons + lumière de contour (rim) + balancement au vent optionnel (végétation, instancing OK).
// Basé sur MeshToonMaterial (lumières/fog/instancing) + injection GLSL. `shaderTime` est mis à jour par Game à chaque frame.
export const shaderTime = { value: 0 };
let ramp;
function getRamp(steps = 4) {
  if (ramp) return ramp;
  const d = new Uint8Array(steps); for (let i = 0; i < steps; i++) d[i] = Math.round(255 * (0.3 + 0.7 * i / (steps - 1)));
  ramp = new THREE.DataTexture(d, steps, 1, THREE.RedFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
  return ramp;
}
export function toonMaterial({ color = '#ffffff', vertexColors = false, rim = '#9fd8ff', rimPower = 2.5, rimStrength = 0.55,
  emissive = '#000000', emissiveIntensity = 1, flatShading = false, sway = 0, detail = 0, ...rest } = {}) {
  const m = new THREE.MeshToonMaterial({ color, vertexColors, gradientMap: getRamp(), emissive, emissiveIntensity, flatShading, ...rest });
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
      .replace('#include <color_fragment>', '#include <color_fragment>' + (detail ? `
        float dn = vnoise(vObjP * .45) * .6 + vnoise(vObjP * 1.6) * .4;   // taches peintes (2 échelles, posterisées)
        diffuseColor.rgb *= .9 + .2 * smoothstep(.42, .46, dn) + .08 * step(.62, vnoise(vObjP * 4.2));` : ''))
      .replace('#include <opaque_fragment>', `
        float rimF = pow(1.0 - saturate(dot(normalize(vViewPosition), normal)), uRimP);
        outgoingLight += uRim * rimF * uRimS * (0.35 + 0.65 * saturate(dot(normal, directionalLights[0].direction) + .4));
        #include <opaque_fragment>`);
  };
  m.customProgramCacheKey = () => 'toon-rim-sway' + (detail ? '-d' : '');
  return m;
}
// Contour "inverted hull" — à réserver au héros / créatures importantes.
export function addOutline(mesh, color = '#120a2a', thickness = 0.04) {
  const mat = new THREE.ShaderMaterial({
    side: THREE.BackSide, uniforms: { uC: { value: new THREE.Color(color) }, uT: { value: thickness } },
    vertexShader: 'uniform float uT; void main(){ vec3 p=position+normal*uT; gl_Position=projectionMatrix*modelViewMatrix*vec4(p,1.); }',
    fragmentShader: 'uniform vec3 uC; void main(){ gl_FragColor=vec4(uC,1.); }',
  });
  const o = new THREE.Mesh(mesh.geometry, mat); mesh.add(o); return o;
}
