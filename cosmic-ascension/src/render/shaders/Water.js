import * as THREE from 'three';
import { NOISE_GLSL } from './noise.glsl.js';
import { shaderTime } from './Toon.js';
// Eau cartoon: dégradé de profondeur (attribut `depth` 0 rivage → 1 large), bandes d'écume animées, scintillements, fresnel, tons posterisés.
export function waterMaterial({ shallow = '#5fe3ff', deep = '#1e5fd0', foam = '#ffffff', sun = new THREE.Vector3(.5, .7, .4) } = {}) {
  return new THREE.ShaderMaterial({
    transparent: true, fog: true,
    uniforms: THREE.UniformsUtils.merge([THREE.UniformsLib.fog, {
      uShallow: { value: new THREE.Color(shallow) }, uDeep: { value: new THREE.Color(deep) }, uFoam: { value: new THREE.Color(foam) },
      uSun: { value: sun.clone().normalize() } }, ]),
    onBeforeCompile: (s) => { s.uniforms.uTime = shaderTime; },
    vertexShader: `attribute float depth; uniform float uTime; varying float vD; varying vec3 vP; varying vec3 vN; varying vec3 vV;
      #include <fog_pars_vertex>
      void main(){ vD=depth; vP=position; vN=normalize(position);
        vec3 p=position+vN*(sin(uTime*1.2+position.x*.35+position.z*.3)*.06+sin(uTime*.9+position.y*.4)*.05);
        vec4 mvPosition=modelViewMatrix*vec4(p,1.); vV=-mvPosition.xyz; gl_Position=projectionMatrix*mvPosition;
        #include <fog_vertex>
      }`,
    fragmentShader: `${NOISE_GLSL}
      uniform vec3 uShallow,uDeep,uFoam,uSun; uniform float uTime; varying float vD; varying vec3 vP; varying vec3 vN; varying vec3 vV;
      #include <fog_pars_fragment>
      void main(){
        if(vD<-.15) discard;
        float n=vnoise(vP*.55+vec3(uTime*.25,0.,uTime*.18)), n2=vnoise(vP*1.7-vec3(0.,uTime*.3,uTime*.2));
        float d=clamp(vD,0.,1.);
        vec3 col=mix(uShallow,uDeep,smoothstep(0.,.75,d+(n-.5)*.25));
        float ndl=clamp(dot(vN,uSun)*.6+.5,0.,1.); col*=.45+.55*(floor(ndl*4.)/3.);
        float shore=1.-smoothstep(0.,.22,d);
        float bands=step(.8,fract(d*5.-uTime*.3+n*1.2))*shore*.8;
        float edge=smoothstep(.07,.0,d+(n2-.5)*.07);
        float spark=step(.93,n2*n+.4*n)*.35*ndl;
        col=mix(col,uFoam*.92,clamp(bands*.7+edge+spark,0.,1.));
        vec3 nn=normalize(cross(dFdx(vV),dFdy(vV)));
        float fr=pow(1.-abs(dot(normalize(vV),nn)),3.);
        col+=fr*.25*uShallow;
        gl_FragColor=vec4(col,mix(.72,.95,max(d,edge)));
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
        #include <fog_fragment>
      }`,
  });
}
