import * as THREE from 'three';
// Halo atmosphérique fresnel additif (planètes, soleils, boss cosmiques).
export function createAtmosphere(radius, color = '#6cc8ff', power = 3.0, intensity = 1.2) {
  const mat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.BackSide,
    uniforms: { uC: { value: new THREE.Color(color) }, uP: { value: power }, uI: { value: intensity } },
    vertexShader: 'varying vec3 vN; varying vec3 vV; void main(){ vN=normalize(normalMatrix*normal); vec4 mv=modelViewMatrix*vec4(position,1.); vV=-mv.xyz; gl_Position=projectionMatrix*mv; }',
    fragmentShader: 'uniform vec3 uC; uniform float uP,uI; varying vec3 vN; varying vec3 vV; void main(){ float f=pow(clamp(dot(normalize(vV),-vN)*.5+.5,0.,1.),uP*2.2); gl_FragColor=vec4(uC*uI,f); }',
  });
  const m = new THREE.Mesh(new THREE.SphereGeometry(radius * 1.14, 40, 28), mat); m.renderOrder = 2; return m;
}
