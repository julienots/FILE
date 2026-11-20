import * as THREE from 'three';
import { U, GLSL_NOISE } from './shared.js';
import { DOME_R } from './terrain.js';
import { mulberry32 } from './noise.js';

/*
 * LE DÔME
 * Géométrie : coupole géodésique (icosaèdre subdivisé) — chaque triangle porte des coordonnées
 * barycentriques pour dessiner les joints des panneaux.
 * Deux maillages partagent la même géométrie :
 *  - face extérieure (FrontSide) : verre — Fresnel, reflets de l'environnement cosmique, reflet du soleil,
 *    joints dorés, onde lumineuse lors du passage de la caméra ;
 *  - face intérieure (BackSide)  : "ciel simulé" projeté sur la paroi — dégradé atmosphérique,
 *    halo solaire, joints hexagonaux qui scintillent, transparence croissante vers le zénith
 *    qui laisse deviner le cosmos et le Créateur au-dehors.
 */
function geodesicDome(R, detail) {
  const ico = new THREE.IcosahedronGeometry(1, detail);
  const p = ico.attributes.position;
  const pos = [], bary = [], panel = [];
  const v = [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()];
  const r = mulberry32(9);
  for (let t = 0; t < p.count; t += 3) {
    for (let k = 0; k < 3; k++) v[k].fromBufferAttribute(p, t + k);
    const cy = (v[0].y + v[1].y + v[2].y) / 3;
    if (cy < -0.03) continue;
    const pr = r();
    for (let k = 0; k < 3; k++) {
      const q = v[k].clone();
      if (q.y < 0) { q.y = 0; q.normalize(); }
      q.multiplyScalar(R);
      pos.push(q.x, q.y, q.z);
      bary.push(k === 0 ? 1 : 0, k === 1 ? 1 : 0, k === 2 ? 1 : 0);
      panel.push(pr);
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('aBary', new THREE.Float32BufferAttribute(bary, 3));
  g.setAttribute('aPanel', new THREE.Float32BufferAttribute(panel, 1));
  g.computeVertexNormals();
  // normales lisses = direction radiale
  const n = g.attributes.normal, pp = g.attributes.position, tmp = new THREE.Vector3();
  for (let i = 0; i < n.count; i++) { tmp.fromBufferAttribute(pp, i).normalize(); n.setXYZ(i, tmp.x, tmp.y, tmp.z); }
  return g;
}

const domeVS = /* glsl */`
attribute vec3 aBary; attribute float aPanel;
varying vec3 vW; varying vec3 vN; varying vec3 vBary; varying float vPanel;
void main(){
  vBary = aBary; vPanel = aPanel;
  vec4 wp = modelMatrix * vec4(position, 1.0); vW = wp.xyz;
  vN = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * viewMatrix * wp;
}`;

const domeCommon = /* glsl */`
uniform float uTime; uniform vec3 uSunDir; uniform vec3 uSunColor; uniform vec3 uSkyTop; uniform vec3 uSkyHorizon;
uniform float uInside; uniform float uNight; uniform vec3 uRipplePos; uniform float uRippleT; uniform float uReveal; uniform vec3 uCreatorPos;
varying vec3 vW; varying vec3 vN; varying vec3 vBary; varying float vPanel;
${GLSL_NOISE}
float edgeLine(float w){
  vec3 d = fwidth(vBary);
  vec3 a = smoothstep(vec3(0.0), d*w, vBary);
  return 1.0 - min(min(a.x, a.y), a.z);
}
// environnement "cosmique" extérieur (pour les reflets du verre)
vec3 cosmos(vec3 d){
  float h = d.y;
  vec3 c = mix(vec3(0.95,0.62,0.42), vec3(0.10,0.10,0.28), smoothstep(-0.05, 0.6, h));
  c = mix(c, vec3(0.03,0.03,0.08), smoothstep(-0.05,-0.5,h));
  c += vec3(1.0,0.75,0.5) * pow(max(0.0, 1.0 - abs(h)), 12.0) * 0.6;
  return c;
}
float ripple(){
  float d = distance(vW, uRipplePos);
  float front = uRippleT * 140.0;
  return exp(-pow((d - front) / 6.0, 2.0)) * (1.0 - uRippleT) + exp(-d*0.08) * (1.0-uRippleT) * step(0.001, uRippleT) * 0.6;
}`;

const outsideFS = /* glsl */`
${domeCommon}
void main(){
  vec3 N = normalize(vN);
  vec3 V = normalize(cameraPosition - vW);
  float ndv = max(dot(N, V), 0.0);
  float fres = 0.04 + 0.96 * pow(1.0 - ndv, 5.0);
  vec3 R = reflect(-V, N);
  vec3 env = cosmos(R);
  // reflet du soleil intérieur vu à travers + reflet spéculaire d'une lumière extérieure chaude
  vec3 keyDir = normalize(vec3(-0.5, 0.55, 0.65));
  float spec = pow(max(dot(R, keyDir), 0.0), 600.0) * 18.0 + pow(max(dot(R, keyDir), 0.0), 40.0) * 0.35;
  float spec2 = pow(max(dot(R, normalize(vec3(0.7,0.3,-0.6))), 0.0), 300.0) * 6.0;
  float e = edgeLine(1.0);
  float e2 = edgeLine(3.0) * 0.35;
  // irisation légère (film mince)
  vec3 irid = 0.5 + 0.5*cos(6.2831*(vec3(0.0,0.33,0.67) + ndv*1.7 + vPanel*0.3));
  vec3 col = env * fres * 1.4 + irid * fres * 0.15;
  vec3 seam = mix(vec3(1.0,0.82,0.45), vec3(0.6,0.95,1.0), 0.35);
  col += seam * (e * 0.55 + e2 * 0.12);
  col += vec3(1.0, 0.92, 0.8) * (spec + spec2);
  float rp = ripple();
  col += vec3(0.4, 0.9, 1.0) * rp * (1.5 + e * 4.0);
  // léger voile bleuté de "champ de force" sur certains panneaux
  float shimmer = smoothstep(0.92, 1.0, sin(uTime*0.6 + vPanel*40.0)) * 0.12;
  col += vec3(0.5,0.85,1.0) * shimmer;
  float a = clamp(fres * 0.7 + e * 0.32 + e2 * 0.06 + spec + spec2 + rp * 0.8 + shimmer + 0.035, 0.0, 1.0);
  gl_FragColor = vec4(col, a);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

const insideFS = /* glsl */`
${domeCommon}
void main(){
  vec3 V = normalize(vW - cameraPosition);       // direction du regard
  float el = V.y;
  // ciel atmosphérique projeté
  vec3 sky = mix(uSkyHorizon, uSkyTop, pow(smoothstep(-0.05, 0.85, el), 0.65));
  float sd = max(dot(V, uSunDir), 0.0);
  sky += uSunColor * (pow(sd, 6.0) * 0.35 + pow(sd, 64.0) * 0.6) * (1.0 - uNight*0.8);
  // fines lignes de balayage + scintillement : la "projection" trahit la simulation
  float scan = 0.5 + 0.5*sin(vW.y*1.3 - uTime*1.5);
  float glitch = smoothstep(0.985, 1.0, sin(uTime*0.35 + vPanel*61.0)) ;
  float e = edgeLine(0.9);
  float hex = edgeLine(2.4);
  // transparence : opaque à l'horizon (atmosphère), translucide au zénith
  float clarity = smoothstep(0.15, 0.95, el);
  float aSky = mix(0.97, 0.5 - uReveal*0.2, clarity);
  aSky = mix(aSky, aSky*0.35, glitch);
  aSky *= mix(1.0, 0.55, uNight);
  // là où regarde le Créateur, la projection du ciel s'amincit : on le devine derrière la voûte
  float gaze = smoothstep(0.86, 0.985, dot(V, normalize(uCreatorPos - cameraPosition)));
  aSky *= 1.0 - gaze * (0.45 + 0.4 * uReveal);
  vec3 lineCol = mix(vec3(0.75, 0.95, 1.0), vec3(1.0, 0.85, 0.55), 0.3);
  float lineA = (e * 0.32 + hex * 0.06) * (0.25 + 0.75*clarity) * (0.75 + 0.25*scan) * (0.7 + uReveal*0.6);
  vec3 col = sky * aSky + lineCol * lineA * 1.2 + vec3(0.5,0.9,1.0) * glitch * 0.25;
  float a = clamp(aSky + lineA, 0.0, 1.0);
  // vu de l'extérieur (le dôme est vide de ciel) : simple verre
  vec3 colOut = vec3(0.7,0.85,1.0) * (e*0.35);
  float aOut = e * 0.2 + 0.02;
  col = mix(colOut, col, uInside); a = mix(aOut, a, uInside);
  float rp = ripple();
  col += vec3(0.4,0.9,1.0) * rp * (1.0 + e*3.0); a = clamp(a + rp*0.5, 0.0, 1.0);
  gl_FragColor = vec4(col / max(a, 0.001), a);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

export function buildDome() {
  const geo = geodesicDome(DOME_R, 9);
  const uniforms = { ...U, uRipplePos: { value: new THREE.Vector3(0, DOME_R, 0) }, uRippleT: { value: 0 }, uReveal: { value: 0 }, uCreatorPos: { value: new THREE.Vector3(0, 400, 0) } };
  const inner = new THREE.Mesh(geo, new THREE.ShaderMaterial({
    vertexShader: domeVS, fragmentShader: insideFS, uniforms, transparent: true, depthWrite: false, side: THREE.BackSide,
  }));
  inner.renderOrder = -1; // derrière nuages / eau : c'est le "ciel"
  const outer = new THREE.Mesh(geo, new THREE.ShaderMaterial({
    vertexShader: domeVS, fragmentShader: outsideFS, uniforms, transparent: true, depthWrite: false, side: THREE.FrontSide,
  }));
  outer.renderOrder = 10; // par-dessus tout : c'est le verre
  const group = new THREE.Group(); group.name = 'dome';
  group.add(inner, outer);
  return { group, uniforms };
}

// ---------- Cosmos extérieur ----------
export function buildCosmos() {
  const m = new THREE.ShaderMaterial({
    side: THREE.BackSide, depthWrite: false,
    uniforms: { uTime: U.uTime },
    vertexShader: `varying vec3 vD; void main(){ vD = normalize(position); vec4 p = projectionMatrix * modelViewMatrix * vec4(position,1.0); gl_Position = p.xyww; }`,
    fragmentShader: `uniform float uTime; varying vec3 vD;
      ${GLSL_NOISE}
      float star(vec3 d, float scale, float thr){ vec3 p = d*scale; vec3 i = floor(p); vec3 f = fract(p)-0.5;
        float h = fract(sin(dot(i, vec3(12.9898,78.233,37.719)))*43758.5453);
        float s = smoothstep(thr, 1.0, h) * smoothstep(0.5, 0.0, length(f)*(2.5+h*3.0));
        return s * (0.6 + 0.4*sin(uTime*2.0 + h*50.0)); }
      void main(){
        vec3 d = normalize(vD); float h = d.y;
        vec3 c = mix(vec3(0.98,0.60,0.40), vec3(0.16,0.13,0.36), smoothstep(-0.02, 0.45, h));
        c = mix(c, vec3(0.04,0.04,0.12), smoothstep(0.35, 1.0, h));
        c = mix(c, vec3(0.06,0.04,0.10), smoothstep(0.0,-0.4,h));
        c += vec3(1.0,0.72,0.45) * pow(max(0.0, 1.0 - abs(h-0.02)*1.0), 18.0) * 0.8;
        // nébuleuse
        vec2 uv = vec2(atan(d.x, d.z), h) * vec2(1.6, 3.0);
        float n = gd_fbm(uv*1.3 + vec2(uTime*0.004, 0.0));
        float n2 = gd_fbm(uv*2.7 - 4.0);
        vec3 neb = mix(vec3(0.45,0.25,0.75), vec3(0.15,0.55,0.85), n2);
        c += neb * smoothstep(0.45, 0.85, n) * smoothstep(0.05, 0.4, h) * 0.55;
        float s = star(d, 260.0, 0.93) + star(d, 520.0, 0.96)*0.7;
        c += vec3(1.0, 0.95, 0.9) * s * smoothstep(0.08, 0.35, h) * 2.5;
        gl_FragColor = vec4(c, 1.0);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }`,
  });
  const mesh = new THREE.Mesh(new THREE.SphereGeometry(4000, 48, 24), m);
  mesh.frustumCulled = false; mesh.renderOrder = -10;
  return mesh;
}

// ---------- Piédestal du Créateur ----------
export function buildPedestal(T) {
  const g = new THREE.Group(); g.name = 'pedestal';
  const stone = new THREE.MeshStandardMaterial({ map: T.stone, color: '#b9b2a8', roughness: 0.85 });
  const dark = new THREE.MeshStandardMaterial({ color: '#2a2633', roughness: 0.35, metalness: 0.3 });
  const gold = new THREE.MeshStandardMaterial({ color: '#d9ac55', roughness: 0.28, metalness: 1.0 });
  T.runes.wrapS = THREE.RepeatWrapping; T.runes.repeat.set(6, 1);
  const runes = new THREE.MeshStandardMaterial({ color: '#141320', emissive: '#55d6ff', emissiveMap: T.runes, emissiveIntensity: 2.2, roughness: 0.4 });
  const add = (geo, m, y) => { const x = new THREE.Mesh(geo, m); x.position.y = y; x.castShadow = true; x.receiveShadow = true; g.add(x); return x; };
  const ring = new THREE.RingGeometry(DOME_R - 6, DOME_R + 6, 128, 1); ring.rotateX(-Math.PI / 2);
  add(ring, stone, 0.9);
  add(new THREE.CylinderGeometry(DOME_R + 6, DOME_R + 6, 0.8, 128, 1, true), stone, 0.5);
  add(new THREE.TorusGeometry(DOME_R + 0.4, 1.1, 10, 160).rotateX(Math.PI / 2), gold, 1.2);
  add(new THREE.TorusGeometry(DOME_R + 6, 0.7, 8, 160).rotateX(Math.PI / 2), gold, 0.9);
  add(new THREE.CylinderGeometry(DOME_R + 6, DOME_R + 3, 10, 128, 1, true), dark, -4.6);
  add(new THREE.CylinderGeometry(DOME_R + 3.3, DOME_R + 3.3, 3, 128, 1, true), runes, -4.6);
  add(new THREE.TorusGeometry(DOME_R + 4.2, 0.5, 8, 160).rotateX(Math.PI / 2), gold, -2.6);
  add(new THREE.TorusGeometry(DOME_R + 4.0, 0.5, 8, 160).rotateX(Math.PI / 2), gold, -6.6);
  add(new THREE.CylinderGeometry(DOME_R + 3, DOME_R + 14, 14, 128, 1), dark, -16.5);
  add(new THREE.TorusGeometry(DOME_R + 9, 0.8, 8, 160).rotateX(Math.PI / 2), gold, -17);
  add(new THREE.CylinderGeometry(DOME_R + 16, DOME_R + 22, 4, 128, 1), stone, -25);
  // dessous du monde (masque l'épaisseur du terrain)
  add(new THREE.CircleGeometry(DOME_R + 6, 64).rotateX(Math.PI / 2), dark, -9.6);
  // fragments flottants autour du piédestal
  const frags = new THREE.Group();
  const r = mulberry32(12);
  for (let i = 0; i < 16; i++) {
    const s = 2 + r() * 5;
    const m = new THREE.Mesh(new THREE.IcosahedronGeometry(s, 0), r() > 0.3 ? stone : dark);
    const a = (i / 16) * Math.PI * 2 + r() * 0.3, rr = DOME_R + 35 + r() * 50;
    m.position.set(Math.cos(a) * rr, -30 + r() * 50, Math.sin(a) * rr);
    m.rotation.set(r() * 3, r() * 3, r() * 3);
    m.userData.spin = (r() - 0.5) * 0.3; m.userData.bob = r() * 6;
    m.castShadow = false;
    frags.add(m);
    if (r() > 0.5) { const c = new THREE.Mesh(new THREE.TorusGeometry(s * 0.9, 0.15, 6, 24), gold); c.position.copy(m.position); c.rotation.x = Math.PI / 2; frags.add(c); }
  }
  g.add(frags);
  g.userData.frags = frags;
  g.userData.runes = runes;
  return g;
}

// ---------- Présence du Créateur (visible depuis l'intérieur à travers la voûte) ----------
export function buildCreator(T) {
  const g = new THREE.Group(); g.name = 'creator';
  const hoodMat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, side: THREE.DoubleSide,
    uniforms: { uFade: { value: 0 }, uTime: U.uTime },
    vertexShader: `varying vec3 vN; varying vec3 vW; void main(){ vN = normalize(mat3(modelMatrix)*normal); vec4 w = modelMatrix*vec4(position,1.0); vW = w.xyz; gl_Position = projectionMatrix*viewMatrix*w; }`,
    fragmentShader: `uniform float uFade; uniform float uTime; varying vec3 vN; varying vec3 vW;
      void main(){ vec3 V = normalize(cameraPosition - vW); float rim = pow(1.0 - abs(dot(normalize(vN), V)), 2.5);
        vec3 c = vec3(0.01,0.008,0.03) + vec3(1.0,0.7,0.35)*pow(rim, 2.0)*0.35;
        gl_FragColor = vec4(c, 0.97 * uFade); }`,
  });
  // capuche / silhouette gigantesque
  const hood = new THREE.Mesh(new THREE.SphereGeometry(70, 32, 20, 0, Math.PI * 2, 0, Math.PI * 0.62), hoodMat);
  hood.rotation.x = Math.PI / 2 + 0.25; hood.scale.set(1, 1.25, 1);
  g.add(hood);
  const face = new THREE.Mesh(new THREE.SphereGeometry(48, 24, 16), hoodMat); face.position.z = -8; face.scale.set(1, 1.25, 0.6);
  g.add(face);
  const shoulders = new THREE.Mesh(new THREE.SphereGeometry(140, 32, 16, 0, Math.PI * 2, 0, Math.PI * 0.5), hoodMat);
  shoulders.position.set(0, -120, -60); shoulders.scale.set(1.3, 0.7, 0.8); g.add(shoulders);
  // yeux
  const eyeMat = new THREE.SpriteMaterial({ fog: false, map: T.glow, color: '#ffd38a', blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: 0 });
  const coreMat = new THREE.SpriteMaterial({ fog: false, map: T.glow, color: '#ffffff', blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: 0 });
  const eyes = [];
  for (const sx of [-1, 1]) {
    const halo = new THREE.Sprite(eyeMat); halo.scale.set(70, 38, 1); halo.position.set(sx * 18, 6, 32); g.add(halo);
    const core = new THREE.Sprite(coreMat); core.scale.set(22, 10, 1); core.position.set(sx * 18, 6, 33); g.add(core);
    eyes.push(halo, core);
  }
  g.children.forEach(c => { c.renderOrder = -3; c.frustumCulled = false; });
  g.visible = false;
  g.userData = {
    setFade(f) {
      hoodMat.uniforms.uFade.value = f; eyeMat.opacity = f * 0.9; coreMat.opacity = f;
      g.visible = f > 0.001;
    },
    blink(t) { const b = Math.max(0.08, Math.abs(Math.sin(t))) ; eyes.forEach(e => { e.scale.y = (e.material === coreMat ? 10 : 38) * (Math.sin(t * 0.37) > 0.985 ? 0.1 : 1); }); },
  };
  return g;
}

// ---------- Soleil & lune artificiels (orbitent sous la voûte) ----------
export function buildSunMoon(T) {
  const sun = new THREE.Group();
  const core = new THREE.Mesh(new THREE.SphereGeometry(2.6, 24, 16), new THREE.MeshBasicMaterial({ color: new THREE.Color(6, 5.2, 3.6), fog: false }));
  sun.add(core);
  const halo = new THREE.Sprite(new THREE.SpriteMaterial({ fog: false, map: T.glow, color: '#ffe2a8', blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
  halo.scale.setScalar(34); sun.add(halo);
  const moon = new THREE.Group();
  moon.add(new THREE.Mesh(new THREE.SphereGeometry(2.0, 20, 14), new THREE.MeshBasicMaterial({ color: new THREE.Color(1.6, 1.8, 2.2), fog: false })));
  const mh = new THREE.Sprite(new THREE.SpriteMaterial({ fog: false, map: T.glow, color: '#9cc4ff', blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: 0.6 }));
  mh.scale.setScalar(20); moon.add(mh);
  [sun, moon].forEach(o => o.traverse(c => { c.renderOrder = 0; }));
  return { sun, moon };
}

// ---------- Nuages (sous la voûte, partagés par les deux vues) ----------
export function buildClouds(T, count = 16) {
  const r = mulberry32(55);
  const puffs = [];
  const clouds = [];
  for (let c = 0; c < count; c++) {
    const a = r() * 6.28, rr = 20 + r() * 70, y = 62 + r() * 18;
    const n = 6 + Math.floor(r() * 6), size = 10 + r() * 8;
    clouds.push({ a, rr, y });
    for (let i = 0; i < n; i++) puffs.push({ c, ox: (r() - 0.5) * size * 1.6, oy: (r() - 0.3) * size * 0.35, oz: (r() - 0.5) * size * 0.8, s: size * (0.6 + r() * 0.7), rot: r() * 6.28 });
  }
  const geo = new THREE.InstancedBufferGeometry();
  const base = new THREE.PlaneGeometry(1, 1);
  geo.index = base.index; geo.attributes.position = base.attributes.position; geo.attributes.uv = base.attributes.uv;
  const off = new Float32Array(puffs.length * 3), sz = new Float32Array(puffs.length * 2);
  geo.setAttribute('aOffset', new THREE.InstancedBufferAttribute(off, 3));
  geo.setAttribute('aSize', new THREE.InstancedBufferAttribute(sz, 2));
  geo.instanceCount = puffs.length;
  const mat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false,
    uniforms: { map: { value: T.cloud }, uSunColor: U.uSunColor, uSunDir: U.uSunDir, uSkyTop: U.uSkyTop, uNight: U.uNight, uSkyHorizon: U.uSkyHorizon },
    vertexShader: `attribute vec3 aOffset; attribute vec2 aSize; varying vec2 vUv; varying float vH; varying vec3 vW;
      void main(){ vUv = uv; vH = position.y + 0.5;
        float c = cos(aSize.y), s = sin(aSize.y); vec2 p = mat2(c,-s,s,c) * position.xy * aSize.x;
        vec4 mv = viewMatrix * vec4(aOffset, 1.0); mv.xy += p; vW = aOffset;
        gl_Position = projectionMatrix * mv; }`,
    fragmentShader: `uniform sampler2D map; uniform vec3 uSunColor; uniform vec3 uSunDir; uniform vec3 uSkyTop; uniform vec3 uSkyHorizon; uniform float uNight;
      varying vec2 vUv; varying float vH; varying vec3 vW;
      void main(){ vec4 t = texture2D(map, vUv);
        float lit = mix(0.62, 1.08, vUv.y);
        vec3 shade = mix(uSkyTop*0.9 + vec3(0.15), uSkyHorizon, 0.4);
        vec3 col = mix(shade, uSunColor*1.15, lit*0.75) * mix(1.0, 0.35, uNight);
        gl_FragColor = vec4(col, t.a * 0.95);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }`,
  });
  const mesh = new THREE.Mesh(geo, mat);
  mesh.frustumCulled = false; mesh.renderOrder = 3;
  function update(dt, speed = 1) {
    clouds.forEach(cl => { cl.a += dt * 0.006 * speed; });
    puffs.forEach((p, i) => {
      const cl = clouds[p.c];
      off[i * 3] = Math.cos(cl.a) * cl.rr + p.ox; off[i * 3 + 1] = cl.y + p.oy; off[i * 3 + 2] = Math.sin(cl.a) * cl.rr + p.oz;
      // garder les nuages sous la voûte
      const d = Math.hypot(off[i * 3], off[i * 3 + 1], off[i * 3 + 2]);
      if (d > DOME_R - 10) { const k = (DOME_R - 10) / d; off[i * 3] *= k; off[i * 3 + 1] *= k; off[i * 3 + 2] *= k; }
      sz[i * 2] = p.s; sz[i * 2 + 1] = p.rot;
    });
    geo.attributes.aOffset.needsUpdate = true; geo.attributes.aSize.needsUpdate = true;
  }
  update(0);
  return { mesh, update };
}
