import * as THREE from 'three';
// Matériau cartoon: ramp 3-4 tons + lumière de contour (rim) + option émissive pulsée.
// Basé sur MeshToonMaterial (compatible lumières/fog/instancing) + injection GLSL.
let ramp;
function getRamp(steps = 4) {
  if (ramp) return ramp;
  const d = new Uint8Array(steps); for (let i = 0; i < steps; i++) d[i] = Math.round(255 * (0.28 + 0.72 * i / (steps - 1)));
  ramp = new THREE.DataTexture(d, steps, 1, THREE.RedFormat); ramp.minFilter = ramp.magFilter = THREE.NearestFilter; ramp.needsUpdate = true;
  return ramp;
}
export function toonMaterial({ color = '#ffffff', vertexColors = false, rim = '#9fd8ff', rimPower = 2.5, rimStrength = 0.55,
  emissive = '#000000', emissiveIntensity = 1, flatShading = false } = {}) {
  const m = new THREE.MeshToonMaterial({ color, vertexColors, gradientMap: getRamp(), emissive, emissiveIntensity, flatShading });
  const rimColor = new THREE.Color(rim);
  m.onBeforeCompile = (s) => {
    s.uniforms.uRim = { value: rimColor }; s.uniforms.uRimP = { value: rimPower }; s.uniforms.uRimS = { value: rimStrength };
    s.fragmentShader = s.fragmentShader
      .replace('#include <common>', '#include <common>\nuniform vec3 uRim; uniform float uRimP,uRimS;')
      .replace('#include <opaque_fragment>', `
        float rimF = pow(1.0 - saturate(dot(normalize(vViewPosition), normal)), uRimP);
        outgoingLight += uRim * rimF * uRimS;
        #include <opaque_fragment>`);
  };
  m.customProgramCacheKey = () => 'toon-rim';
  return m;
}
// Contour "inverted hull" (1 draw call de plus par mesh) — à réserver au héros / créatures importantes.
export function addOutline(mesh, color = '#120a2a', thickness = 0.04) {
  const mat = new THREE.ShaderMaterial({
    side: THREE.BackSide, uniforms: { uC: { value: new THREE.Color(color) }, uT: { value: thickness } },
    vertexShader: 'uniform float uT; void main(){ vec3 p=position+normal*uT; gl_Position=projectionMatrix*modelViewMatrix*vec4(p,1.); }',
    fragmentShader: 'uniform vec3 uC; void main(){ gl_FragColor=vec4(uC,1.); }',
  });
  const o = new THREE.Mesh(mesh.geometry, mat); mesh.add(o); return o;
}
