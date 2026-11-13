import * as THREE from 'three';
// Voile atmosphérique vu du sol: dégradé horizon → zénith par-dessus la nébuleuse, + disque solaire stylisé. Suit la caméra.
export function createSkyDome(color = '#7fd0ff') {
  const mat = new THREE.ShaderMaterial({
    side: THREE.BackSide, transparent: true, depthWrite: false, fog: false,
    uniforms: { uC: { value: new THREE.Color(color) }, uUp: { value: new THREE.Vector3(0, 1, 0) }, uSun: { value: new THREE.Vector3(0, 1, 0) } },
    vertexShader: 'varying vec3 vD; void main(){ vD=normalize(position); vec4 p=modelViewMatrix*vec4(position,1.); gl_Position=(projectionMatrix*p).xyww; }',
    fragmentShader: `uniform vec3 uC,uUp,uSun; varying vec3 vD;
      void main(){ vec3 d=normalize(vD); float h=dot(d,uUp); float day=smoothstep(-.25,.35,dot(uUp,uSun));
        float a=(1.-smoothstep(-.1,.42,h))*mix(.35,.8,day);
        vec3 col=mix(uC*uC*.9,uC,pow(1.-clamp(h,0.,1.),4.))*mix(.2,.55,day);
        float s=dot(d,uSun); float disc=smoothstep(.9965,.9975,s); float halo=pow(max(s,0.),24.)*.5;
        col+=vec3(1.,.85,.55)*(disc*2.5+halo*.8); a=clamp(a+disc+halo*.6,0.,1.);
        gl_FragColor=vec4(col,a);
        #include <colorspace_fragment>
      }`,
  });
  const m = new THREE.Mesh(new THREE.SphereGeometry(1, 24, 16), mat); m.scale.setScalar(4500); m.frustumCulled = false; m.renderOrder = -8;
  return m;
}
