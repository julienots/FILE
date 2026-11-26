// Imposteurs : végétation photographiée sous 8 azimuts x N élévations (modèles scannés Poly Haven),
// affichée en panneaux orientés vers la caméra, avec fondu entre les vues voisines, éclairage dynamique
// par normales cuites, translucidité du feuillage, ombres portées et état calciné.
import * as THREE from 'three';
import { G } from './core.js';

const loader = new THREE.TextureLoader();

export async function loadImpostor(name) {
  const meta = await (await fetch(`img/veg/${name}.json`)).json();
  const alb = await loader.loadAsync(`img/veg/${name}_albedo.webp`);
  const nor = await loader.loadAsync(`img/veg/${name}_normal.webp`);
  alb.colorSpace = THREE.SRGBColorSpace;
  for (const t of [alb, nor]) { t.anisotropy = 4; t.minFilter = THREE.LinearMipmapLinearFilter; t.generateMipmaps = true; }
  return { meta, alb, nor };
}

const COMMON = (meta) => `
  #define AZ ${meta.az.toFixed(1)}
  #define NEL ${meta.el.length.toFixed(1)}
  uniform float uEl[${meta.el.length}];
  uniform float uR, uCY;
  varying vec2 vUv; varying vec4 vF; varying vec2 vW; varying float vYaw; varying float vBurn; varying float vDist;`;

const VFUNC = `
  attribute vec3 aPos; attribute vec4 aP; // échelle, rotation, calcination, visibilité
  void impostorVertex(out vec3 wp){
    float s = aP.x * aP.w; float yaw = aP.y; vYaw = yaw; vBurn = aP.z;
    vec3 c = aPos + vec3(0.0, uCY * aP.x, 0.0);
    vec3 toCam = cameraPosition - c;
    vDist = length(toCam);
    vec3 d = toCam / max(vDist, 1e-4);
    // direction dans le repère de l'arbre
    float cy = cos(yaw), sy = sin(yaw);
    vec3 ld = vec3(cy * d.x - sy * d.z, d.y, sy * d.x + cy * d.z);
    float az = atan(ld.x, ld.z); if (az < 0.0) az += 6.2831853;
    float fa = az / 6.2831853 * AZ;
    float el = asin(clamp(ld.y, -1.0, 1.0));
    float fe = 0.0;
    for (int i = 0; i < int(NEL) - 1; i++) { if (el > uEl[i]) fe = float(i) + clamp((el - uEl[i]) / (uEl[i + 1] - uEl[i]), 0.0, 1.0); }
    vF = vec4(floor(fa), mod(floor(fa) + 1.0, AZ), floor(fe), min(floor(fe) + 1.0, NEL - 1.0));
    vW = vec2(fract(fa), fract(fe));
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), d) + vec3(1e-5, 0.0, 0.0));
    vec3 up = cross(d, right);
    wp = c + (right * position.x + up * position.y) * (2.0 * uR * s);
    vUv = uv;
  }`;

const FRAG_SAMPLE = `
  uniform sampler2D tAlb, tNor;
  vec2 tileUV(float a, float e){ return (vec2(a, e) + clamp(vUv, 0.004, 0.996)) / vec2(AZ, NEL); }
  vec4 sampleAtlas(sampler2D t){
    vec4 a = mix(texture2D(t, tileUV(vF.x, vF.z)), texture2D(t, tileUV(vF.y, vF.z)), vW.x);
    vec4 b = mix(texture2D(t, tileUV(vF.x, vF.w)), texture2D(t, tileUV(vF.y, vF.w)), vW.x);
    return mix(a, b, vW.y);
  }
  float coverage(float a){ return a * (1.0 + max(0.0, log2(vDist / 60.0)) * 0.35); }`;

export function impostorMaterial(imp, opts = {}) {
  const { meta } = imp;
  const uniforms = THREE.UniformsUtils.merge([THREE.UniformsLib.fog, {
    uEl: { value: meta.el.map((e) => e * Math.PI / 180) }, uR: { value: meta.radius }, uCY: { value: meta.center[1] },
    tAlb: { value: imp.alb }, tNor: { value: imp.nor },
    uSunDir: { value: new THREE.Vector3(0, 1, 0) }, uSunCol: { value: new THREE.Color(1, 1, 1) },
    uSky: { value: new THREE.Color(0.5, 0.6, 0.8) }, uGround: { value: new THREE.Color(0.2, 0.18, 0.14) },
    uTint: { value: new THREE.Color(opts.tint || 0xffffff) }, uTime: G.uniforms.time,
  }]);
  const mat = new THREE.ShaderMaterial({
    uniforms, fog: true,
    vertexShader: `${COMMON(meta)}${VFUNC}
      #include <fog_pars_vertex>
      uniform float uTime;
      void main(){ vec3 wp; impostorVertex(wp);
        // léger balancement dans le vent (haut de l'arbre seulement)
        float sway = sin(uTime * 1.3 + aPos.x * 0.07 + aPos.z * 0.05) * 0.012 * aP.x * max(position.y + 0.3, 0.0);
        wp.x += sway; wp.z += sway * 0.6;
        vec4 mvPosition = viewMatrix * vec4(wp, 1.0); gl_Position = projectionMatrix * mvPosition;
        #include <fog_vertex>
      }`,
    fragmentShader: `${COMMON(meta)}
      ${FRAG_SAMPLE}
      #include <common>
      #include <fog_pars_fragment>
      uniform vec3 uSunDir, uSunCol, uSky, uGround, uTint;
      void main(){
        vec4 c = sampleAtlas(tAlb);
        float burn = vBurn;
        float a = coverage(c.a);
        if (a < mix(0.5, 0.93, burn)) discard;
        vec3 nl = sampleAtlas(tNor).xyz * 2.0 - 1.0;
        float cy = cos(vYaw), sy = sin(vYaw);
        vec3 n = normalize(vec3(cy * nl.x + sy * nl.z, nl.y, -sy * nl.x + cy * nl.z));
        float ndl = dot(n, uSunDir);
        float diff = max(ndl, 0.0) * 0.8 + max(-ndl, 0.0) * 0.22 + 0.12; // translucidité des feuilles
        vec3 amb = mix(uGround, uSky, n.y * 0.5 + 0.5);
        vec3 alb = c.rgb * uTint;
        alb = mix(alb, vec3(0.035, 0.03, 0.028), burn);
        vec3 col = alb * (uSunCol * diff + amb);
        gl_FragColor = vec4(col, 1.0);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
        #include <fog_fragment>
      }`,
  });
  // ombres : même panneau, orienté vers la lumière pendant la passe d'ombre
  const depth = new THREE.ShaderMaterial({
    uniforms: { uEl: uniforms.uEl, uR: uniforms.uR, uCY: uniforms.uCY, tAlb: uniforms.tAlb, tNor: uniforms.tNor },
    vertexShader: `${COMMON(meta)}${VFUNC} void main(){ vec3 wp; impostorVertex(wp); gl_Position = projectionMatrix * viewMatrix * vec4(wp, 1.0); }`,
    fragmentShader: `${COMMON(meta)}
      ${FRAG_SAMPLE}
      #include <packing>
      void main(){ vec4 c = sampleAtlas(tAlb); if (c.a < mix(0.5, 0.93, vBurn)) discard; gl_FragColor = packDepthToRGBA(gl_FragCoord.z); }`,
  });
  return { mat, depth };
}

// Ensemble instancié d'imposteurs (une espèce)
export class ImpostorField {
  constructor(imp, capacity, opts = {}) {
    this.imp = imp;
    const base = new THREE.PlaneGeometry(1, 1);
    const g = new THREE.InstancedBufferGeometry();
    g.index = base.index; g.setAttribute('position', base.attributes.position); g.setAttribute('uv', base.attributes.uv);
    this.pos = new Float32Array(capacity * 3);
    this.par = new Float32Array(capacity * 4);
    g.setAttribute('aPos', new THREE.InstancedBufferAttribute(this.pos, 3));
    g.setAttribute('aP', new THREE.InstancedBufferAttribute(this.par, 4));
    g.instanceCount = 0;
    const { mat, depth } = impostorMaterial(imp, opts);
    this.mat = mat;
    this.mesh = new THREE.Mesh(g, mat);
    this.mesh.customDepthMaterial = depth;
    this.mesh.frustumCulled = false;
    this.mesh.castShadow = opts.shadows !== false;
    this.count = 0;
    this.capacity = capacity;
  }
  set(i, x, y, z, scale, yaw, burn = 0, visible = 1) {
    this.pos[i * 3] = x; this.pos[i * 3 + 1] = y; this.pos[i * 3 + 2] = z;
    this.par[i * 4] = scale; this.par[i * 4 + 1] = yaw; this.par[i * 4 + 2] = burn; this.par[i * 4 + 3] = visible;
  }
  commit(count) {
    this.count = count;
    const g = this.mesh.geometry;
    g.instanceCount = count;
    g.attributes.aPos.needsUpdate = true; g.attributes.aP.needsUpdate = true;
  }
  // lumière commune à toute la végétation
  light(sunDir, sunCol, sky, ground) {
    const u = this.mat.uniforms;
    u.uSunDir.value.copy(sunDir); u.uSunCol.value.copy(sunCol); u.uSky.value.copy(sky); u.uGround.value.copy(ground);
  }
}

// Arbre « physique » pour les chutes : deux panneaux croisés figés (vues de face et de profil)
export function crossedTree(imp, burnt) {
  const { meta } = imp;
  const s = 2 * meta.radius;
  const geos = [];
  for (const [az, rot] of [[0, 0], [2, Math.PI / 2]]) {
    const p = new THREE.PlaneGeometry(s, s);
    const uv = p.attributes.uv;
    for (let i = 0; i < uv.count; i++) uv.setXY(i, (az + uv.getX(i)) / meta.az, (0 + uv.getY(i)) / meta.el.length);
    p.rotateY(rot); p.translate(0, meta.center[1], 0);
    geos.push(p);
  }
  const m = new THREE.MeshStandardMaterial({ map: imp.alb, alphaTest: 0.5, side: THREE.DoubleSide, roughness: 0.9, color: burnt ? 0x151210 : 0xffffff });
  const grp = new THREE.Group();
  for (const g of geos) { const mesh = new THREE.Mesh(g, m); mesh.castShadow = true; grp.add(mesh); }
  return grp;
}
