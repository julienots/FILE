import * as THREE from 'three';
// Traits de vitesse (warp) attachés à la caméra: 1 draw call, entièrement en vertex shader.
export function createSpeedLines(n = 140) {
  const pos = new Float32Array(n * 6), end = new Float32Array(n * 2), seed = new Float32Array(n * 2);
  for (let i = 0; i < n; i++) { const a = Math.random() * 6.283, r = 6 + Math.random() * 22, x = Math.cos(a) * r, y = Math.sin(a) * r, s = Math.random();
    pos.set([x, y, 0, x, y, 0], i * 6); end.set([0, 1], i * 2); seed.set([s, s], i * 2); }
  const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(pos, 3)); g.setAttribute('end', new THREE.BufferAttribute(end, 1)); g.setAttribute('seed', new THREE.BufferAttribute(seed, 1));
  const mat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, depthTest: false, blending: THREE.AdditiveBlending, fog: false, uniforms: { uT: { value: 0 }, uK: { value: 0 }, uC: { value: new THREE.Color('#9fe8ff') } },
    vertexShader: 'attribute float end; attribute float seed; uniform float uT,uK; varying float vA; void main(){ float z=-140.+mod(uT*(90.+seed*120.)+seed*140.,140.); vec3 p=position; p.z=z-end*(4.+uK*26.); vA=(1.-end)*uK*smoothstep(-140.,-90.,z)*smoothstep(0.,-25.,z); gl_Position=projectionMatrix*modelViewMatrix*vec4(p,1.); }',
    fragmentShader: 'uniform vec3 uC; varying float vA; void main(){ gl_FragColor=vec4(uC*vA,vA); }' });
  const l = new THREE.LineSegments(g, mat); l.frustumCulled = false; l.renderOrder = 6;
  l.userData.update = (t, k) => { mat.uniforms.uT.value = t; mat.uniforms.uK.value = k; l.visible = k > .02; };
  return l;
}
