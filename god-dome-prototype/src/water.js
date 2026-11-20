import * as THREE from 'three';
import { U, GLSL_NOISE } from './shared.js';
import { LAKE } from './terrain.js';

// Shader d'eau stylisé : normales procédurales animées dans le sens du courant,
// Fresnel ciel/profondeur, reflet spéculaire du soleil, écume des berges et des chutes.
const waterVS = /* glsl */`
attribute float aSteep; attribute float aEdge;
varying vec3 vW; varying vec2 vUv; varying float vSteep; varying float vEdge;
#include <common>
#include <shadowmap_pars_vertex>
void main(){
  vUv = uv; vSteep = aSteep; vEdge = aEdge;
  vec4 wp = modelMatrix * vec4(position, 1.0); vW = wp.xyz;
  gl_Position = projectionMatrix * viewMatrix * wp;
}`;
const waterFS = /* glsl */`
uniform float uTime; uniform vec3 uSunDir; uniform vec3 uSunColor; uniform vec3 uSkyTop; uniform vec3 uSkyHorizon;
uniform vec3 uFogColor; uniform float uFogDensity; uniform float uNight; uniform float uFlow;
varying vec3 vW; varying vec2 vUv; varying float vSteep; varying float vEdge;
${GLSL_NOISE}
float wh(vec2 p){
  float t = uTime;
  return gd_vn(p*vec2(1.0,0.6) - vec2(0.0, t*uFlow*1.6)) * 0.5 + gd_vn(p*2.3 + vec2(t*0.3, -t*uFlow*2.4)) * 0.3 + gd_vn(p*5.1 - vec2(t*0.6, t*uFlow*3.5))*0.2;
}
void main(){
  // coordonnées : le long du courant pour la rivière, monde pour le lac
  vec2 p = mix(vW.xz*0.35, vec2(vUv.x*3.0, vUv.y*0.6), step(0.5, uFlow));
  float e = 0.05;
  float h0 = wh(p), hx = wh(p+vec2(e,0.)), hz = wh(p+vec2(0.,e));
  vec3 n = normalize(vec3((h0-hx)*1.4, 1.0, (h0-hz)*1.4));
  vec3 V = normalize(cameraPosition - vW);
  float fres = pow(1.0 - max(dot(n, V), 0.0), 4.0);
  vec3 R = reflect(-V, n);
  vec3 sky = mix(uSkyHorizon, uSkyTop, clamp(R.y*1.3, 0.0, 1.0));
  vec3 deep = mix(vec3(0.04, 0.22, 0.26), vec3(0.08,0.38,0.40), h0) * (0.35 + 0.65*max(uSunDir.y,0.05));
  vec3 col = mix(deep, sky, 0.18 + fres*0.75);
  float spec = pow(max(dot(R, uSunDir), 0.0), 220.0) * 6.0 + pow(max(dot(R, uSunDir), 0.0), 18.0)*0.25;
  col += uSunColor * spec * (1.0 - uNight);
  // écume : berges + cascade
  float foamN = gd_vn(p*4.0 - vec2(0., uTime*uFlow*4.0));
  float foam = smoothstep(0.55, 1.0, vEdge + foamN*0.35) * 0.9;
  foam = max(foam, smoothstep(0.25, 0.8, vSteep) * (0.65 + 0.35*foamN));
  col = mix(col, vec3(0.93,0.97,1.0) * (0.5 + 0.5*max(uSunDir.y,0.1)), foam);
  float alpha = mix(0.82, 0.97, fres) ;
  alpha = max(alpha, foam);
  float d = length(cameraPosition - vW);
  float f = 1.0 - exp(-pow(uFogDensity*d, 2.0));
  col = mix(col, uFogColor, f);
  gl_FragColor = vec4(col, alpha);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

function waterMaterial(flow) {
  return new THREE.ShaderMaterial({
    vertexShader: waterVS, fragmentShader: waterFS, transparent: true, depthWrite: false,
    uniforms: { ...U, uFlow: { value: flow } },
  });
}

export function buildWater(terrain) {
  const group = new THREE.Group(); group.name = 'water';
  const { river, bed, riverWidth } = terrain;
  // ruban de rivière qui suit le lit : 5 sommets en travers
  const pos = [], uv = [], steep = [], edge = [], idx = [];
  let along = 0;
  const ACROSS = 6;
  for (let i = 0; i < river.length; i++) {
    const a = river[Math.max(0, i - 1)], b = river[Math.min(river.length - 1, i + 1)];
    const tx = b.x - a.x, tz = b.y - a.y, tl = Math.hypot(tx, tz) || 1;
    const nx = -tz / tl, nz = tx / tl;
    const w = riverWidth(i) + 2.2;
    const y = bed[i] + 1.0;
    const sl = Math.max(0, (bed[Math.max(0, i - 2)] - bed[Math.min(bed.length - 1, i + 2)]) / 3.2);
    if (i > 0) along += Math.hypot(river[i].x - river[i - 1].x, river[i].y - river[i - 1].y);
    for (let k = 0; k <= ACROSS; k++) {
      const t = k / ACROSS * 2 - 1;
      // en cascade, la nappe d'eau "tombe" : on la décolle légèrement de la paroi
      pos.push(river[i].x + nx * w * t, y + Math.min(sl, 3) * 0.25, river[i].y + nz * w * t);
      uv.push(k / ACROSS, along * 0.25);
      steep.push(Math.min(1, sl * 0.9));
      edge.push(Math.abs(t) ** 2);
    }
  }
  for (let i = 0; i < river.length - 1; i++) for (let k = 0; k < ACROSS; k++) {
    const a = i * (ACROSS + 1) + k, b = a + ACROSS + 1;
    idx.push(a, b, a + 1, a + 1, b, b + 1);
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  g.setAttribute('aSteep', new THREE.Float32BufferAttribute(steep, 1));
  g.setAttribute('aEdge', new THREE.Float32BufferAttribute(edge, 1));
  g.setIndex(idx);
  const riverMesh = new THREE.Mesh(g, waterMaterial(1));
  riverMesh.renderOrder = 1;
  group.add(riverMesh);

  // lac
  const lg = new THREE.CircleGeometry(LAKE.r + 7, 64); lg.rotateX(-Math.PI / 2);
  const n = lg.attributes.position.count;
  lg.setAttribute('aSteep', new THREE.Float32BufferAttribute(new Float32Array(n), 1));
  const le = new Float32Array(n);
  for (let i = 0; i < n; i++) le[i] = Math.hypot(lg.attributes.position.getX(i), lg.attributes.position.getZ(i)) / (LAKE.r + 7) * 0.55;
  lg.setAttribute('aEdge', new THREE.BufferAttribute(le, 1));
  const lake = new THREE.Mesh(lg, waterMaterial(0));
  lake.position.set(LAKE.x, LAKE.level, LAKE.z);
  lake.renderOrder = 1;
  group.add(lake);

  // brume au pied des chutes : particules
  const falls = [];
  for (let i = 2; i < river.length - 2; i++) if (bed[i - 2] - bed[i + 2] > 4) falls.push(i);
  const mistPts = [];
  if (falls.length) {
    // pied de chaque chute = dernier point raide d'une série
    const feet = falls.filter((v, j) => falls[j + 1] !== v + 1);
    feet.forEach(fi => { for (let k = 0; k < 90; k++) mistPts.push(river[fi + 2].x, bed[fi + 2] + 1, river[fi + 2].y, Math.random()); });
  }
  const mg = new THREE.BufferGeometry();
  mg.setAttribute('position', new THREE.Float32BufferAttribute(mistPts.filter((_, i) => i % 4 !== 3), 3));
  mg.setAttribute('aSeed', new THREE.Float32BufferAttribute(mistPts.filter((_, i) => i % 4 === 3), 1));
  const mist = new THREE.Points(mg, new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, uniforms: { uTime: U.uTime, uSunColor: U.uSunColor },
    vertexShader: `attribute float aSeed; uniform float uTime; varying float vA;
      void main(){ float t = fract(uTime*0.35 + aSeed);
        vec3 p = position + vec3(sin(aSeed*91.0)*3.0*t, t*4.5, cos(aSeed*53.0)*3.0*t);
        vA = (1.0-t)*smoothstep(0.0,0.15,t);
        vec4 mv = modelViewMatrix*vec4(p,1.0); gl_Position = projectionMatrix*mv; gl_PointSize = min(160.0, (12.0 + 26.0*t) * 30.0 / -mv.z); }`,
    fragmentShader: `uniform vec3 uSunColor; varying float vA; void main(){ float d = length(gl_PointCoord-0.5); float a = smoothstep(0.5,0.0,d)*vA*0.2; gl_FragColor = vec4(mix(vec3(1.0), uSunColor, 0.3), a); }`,
  }));
  mist.frustumCulled = false; mist.renderOrder = 2;
  group.add(mist);
  return group;
}
