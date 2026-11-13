import * as THREE from 'three';
import { NOISE_GLSL } from './noise.glsl.js';
// Skybox nébuleuse stylisée (fbm + palette 3 couleurs + étoiles lointaines). 1 draw call, 0 texture.
export function createNebula({ a = '#2a0a5e', b = '#ff3c9a', c = '#2fd6ff', seed = 1 } = {}) {
  const mat = new THREE.ShaderMaterial({
    side: THREE.BackSide, depthWrite: false, fog: false,
    uniforms: { uA: { value: new THREE.Color(a) }, uB: { value: new THREE.Color(b) }, uC: { value: new THREE.Color(c) },
      uSeed: { value: seed }, uTime: { value: 0 } },
    vertexShader: `varying vec3 vDir; void main(){ vDir=normalize(position); vec4 p=modelViewMatrix*vec4(position,1.); gl_Position=(projectionMatrix*p).xyww; }`,
    fragmentShader: `${NOISE_GLSL}
      uniform vec3 uA,uB,uC; uniform float uSeed,uTime; varying vec3 vDir;
      void main(){
        vec3 d=normalize(vDir)*2.2+uSeed*7.3;
        float n1=fbm(d+vec3(0.,uTime*.01,0.)), n2=fbm(d*1.9+n1*1.5), n3=fbm(d*.8-n2);
        float cloud=smoothstep(.35,.85,n2*.9+n1*.3);
        vec3 col=mix(vec3(.012,.008,.03),uA,smoothstep(.2,.7,n1));
        col=mix(col,uB,cloud*smoothstep(.3,.9,n3)*.85);
        col=mix(col,uC,smoothstep(.55,.95,n2*n3*2.)*.7);
        // cartoon: posterisation douce
        col=floor(col*9.)/9.*.6+col*.4;
        gl_FragColor=vec4(col,1.);
        #include <colorspace_fragment>
      }`,
  });
  const m = new THREE.Mesh(new THREE.SphereGeometry(1, 32, 24), mat);
  m.scale.setScalar(5000); m.frustumCulled = false; m.renderOrder = -10;
  m.userData.update = (t) => { mat.uniforms.uTime.value = t; };
  return m;
}
