import * as THREE from 'three';
// Champ d'étoiles scintillant (Points, 1 draw call). Taille/couleur par étoile, twinkle en vertex shader.
export function createStars(count = 6000, radius = 4000) {
  const pos = new Float32Array(count * 3), col = new Float32Array(count * 3), sz = new Float32Array(count), ph = new Float32Array(count);
  const c = new THREE.Color();
  for (let i = 0; i < count; i++) {
    const u = Math.random() * 2 - 1, t = Math.random() * Math.PI * 2, r = Math.sqrt(1 - u * u);
    pos.set([r * Math.cos(t) * radius, u * radius, r * Math.sin(t) * radius], i * 3);
    c.setHSL(Math.random() < .5 ? .6 : Math.random() < .5 ? .08 : .95, .5, .75 + Math.random() * .25);
    col.set([c.r, c.g, c.b], i * 3);
    sz[i] = Math.pow(Math.random(), 5) * 3.2 + .7; ph[i] = Math.random() * 6.28;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  g.setAttribute('size', new THREE.BufferAttribute(sz, 1));
  g.setAttribute('phase', new THREE.BufferAttribute(ph, 1));
  const mat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, fog: false,
    uniforms: { uTime: { value: 0 }, uPx: { value: 1 } },
    vertexShader: `attribute float size; attribute float phase; attribute vec3 color; uniform float uTime,uPx; varying vec3 vC; varying float vA;
      void main(){ vC=color; vA=.6+.4*sin(uTime*1.5+phase); vec4 mv=modelViewMatrix*vec4(position,1.); gl_Position=projectionMatrix*mv; gl_Position.z=gl_Position.w*.9999; gl_PointSize=size*uPx; }`,
    fragmentShader: `varying vec3 vC; varying float vA; void main(){ vec2 p=gl_PointCoord-.5; float d=length(p); float a=smoothstep(.5,0.,d); gl_FragColor=vec4(vC*vA,a*vA); }`,
  });
  const pts = new THREE.Points(g, mat); pts.frustumCulled = false; pts.renderOrder = -9;
  pts.userData.update = (t, px) => { mat.uniforms.uTime.value = t; mat.uniforms.uPx.value = px; };
  return pts;
}
