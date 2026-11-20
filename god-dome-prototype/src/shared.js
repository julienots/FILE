import * as THREE from 'three';

// Uniformes partagés par tous les shaders du monde : une seule horloge, un seul vent, un seul soleil.
export const U = {
  uTime: { value: 0 },
  uWind: { value: 1 },
  uSunDir: { value: new THREE.Vector3(0.4, 0.8, 0.3).normalize() },
  uSunColor: { value: new THREE.Color(1, 0.95, 0.85) },
  uSkyTop: { value: new THREE.Color('#4f8fd6') },
  uSkyHorizon: { value: new THREE.Color('#cfe6f2') },
  uFogColor: { value: new THREE.Color('#cfe6f2') },
  uFogDensity: { value: 0.004 },
  uInside: { value: 1 },
  uNight: { value: 0 },
};

// GLSL commun : bruit de valeur 2D
export const GLSL_NOISE = /* glsl */`
float gd_h21(vec2 p){ p = fract(p*vec2(123.34, 456.21)); p += dot(p, p+45.32); return fract(p.x*p.y); }
float gd_vn(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
  return mix(mix(gd_h21(i),gd_h21(i+vec2(1,0)),f.x), mix(gd_h21(i+vec2(0,1)),gd_h21(i+vec2(1,1)),f.x), f.y); }
float gd_fbm(vec2 p){ float s=0., a=.5; for(int i=0;i<4;i++){ s+=a*gd_vn(p); p=p*2.03+17.1; a*=.5; } return s; }
`;

/**
 * Ajoute une animation de vent à un matériau standard/lambert (compatible instancing et ombres).
 * La flexion est proportionnelle à la hauteur locale du sommet, calculée en espace monde
 * puis ramenée en espace local de l'instance pour que toutes les herbes plient dans le même sens.
 */
export function addWind(material, { strength = 0.25, heightRef = 1, gust = true, sheen = 0, flutter = 0, translucent = 0 } = {}) {
  material.onBeforeCompile = (s) => {
    s.uniforms.uTime = U.uTime;
    s.uniforms.uWind = U.uWind; s.uniforms.uSunDir = U.uSunDir; s.uniforms.uSunColor = U.uSunColor;
    s.vertexShader = s.vertexShader
      .replace('#include <common>', `#include <common>
uniform float uTime; uniform float uWind;
varying float vGust; varying float vTip; varying vec3 vWindW;
${GLSL_NOISE}`)
      .replace('#include <begin_vertex>', `#include <begin_vertex>
{
#ifdef USE_INSTANCING
  mat4 im = instanceMatrix;
#else
  mat4 im = mat4(1.0);
#endif
  vec3 ip = (modelMatrix * im[3]).xyz;
  float hN = clamp(position.y / ${heightRef.toFixed(3)}, 0.0, 1.5);
  float bend = hN * hN;
  float g = ${gust ? 'smoothstep(0.35, 0.85, gd_vn(ip.xz*0.035 - uTime*vec2(0.55, 0.22)))' : '0.0'};
  vGust = g; vTip = hN;
  vec2 dir = normalize(vec2(1.0, 0.45));
  float sway = sin(uTime*1.6 + ip.x*0.21 + ip.z*0.17) * 0.35 + sin(uTime*2.7 + ip.x*0.5) * 0.15;
  vec3 wWorld = vec3(dir.x, 0.0, dir.y) * (sway + g*1.6 + 0.4) * ${strength.toFixed(3)} * uWind * bend;
  ${flutter ? `wWorld += vec3(sin(uTime*7.0+position.x*3.0+ip.z), sin(uTime*6.0+position.z*4.0)*0.5, cos(uTime*7.5+position.y*3.0)) * ${flutter.toFixed(3)} * bend * uWind;` : ''}
  mat3 r = mat3(im);
  vec3 wl = transpose(r) * wWorld / max(dot(r[0], r[0]), 1e-4);
  transformed += wl;
  transformed.y -= length(wl.xz) * 0.25 * hN;
  vWindW = (modelMatrix * im * vec4(transformed, 1.0)).xyz;
}`);
    s.fragmentShader = s.fragmentShader
      .replace('#include <common>', `#include <common>
varying float vGust; varying float vTip; varying vec3 vWindW; uniform vec3 uSunDir; uniform vec3 uSunColor;`)
      .replace('#include <color_fragment>', `#include <color_fragment>
diffuseColor.rgb *= 1.0 + vGust * ${sheen.toFixed(3)} * vTip;`)
      .replace('#include <emissivemap_fragment>', `#include <emissivemap_fragment>
      totalEmissiveRadiance += diffuseColor.rgb * uSunColor * pow(max(dot(normalize(vWindW - cameraPosition), uSunDir), 0.0), 4.0) * ${translucent.toFixed(3)};`);
  };
  material.customProgramCacheKey = () => `wind-${strength}-${heightRef}-${gust}-${sheen}-${flutter}-${translucent}`;
  return material;
}
