import * as THREE from 'three';
import { mergeVertices } from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import { makeNoise } from './noise.js';
import { toonMaterial } from '../render/shaders/Toon.js';
import { waterMaterial } from '../render/shaders/Water.js';
import { createAtmosphere } from '../render/shaders/Atmosphere.js';
import { Config } from '../core/Config.js';

const _v = new THREE.Vector3(), _c = new THREE.Color();
// Fonction de terrain partagée (mesh, physique, placement): renvoie la hauteur brute h ∈ ~[-.8,.8].
function terrainFn(def) {
  const n = makeNoise(def.seed), f = def.freq ?? 1.6, sea = def.seaLevel ?? 0;
  return (d) => {
    let h = n.fbm(d.x * f, d.y * f, d.z * f, 5);
    h += (1 - Math.abs(n(d.x * f * 5 + 31, d.y * f * 5, d.z * f * 5))) * 0.05 - 0.03;   // crêtes fines
    return h < sea ? sea + (h - sea) * 0.6 : sea + Math.pow(h - sea, 1.15);              // plages douces, fonds marins peu profonds
  };
}
// Sphère icosaédrique indexée (normales lisses) de rayon 1.
const unitCache = new Map();
function unitSphere(detail) {
  if (!unitCache.has(detail)) { const g = mergeVertices(new THREE.IcosahedronGeometry(1, detail).deleteAttribute('uv').deleteAttribute('normal'), 1e-5); unitCache.set(detail, g); }
  return unitCache.get(detail).clone();
}
// Construit une planète stylisée depuis sa définition data.  opts.surface=true → version détaillée vue du sol (eau animée, sans coque).
// userData: { def, radius, seaRadius, sample(dir)->h brut, heightAt(dir)->rayon du sol, update(dt) }
export function buildPlanet(def, opts = {}) {
  const q = Config.qualityPresets[Config.quality.level], detail = opts.detail ?? (opts.surface ? q.surfaceDetail : q.planetDetail);
  const g = new THREE.Group(); g.name = 'planet:' + def.id;
  const R = def.radius, relief = def.relief ?? 0.08, sea = def.seaLevel ?? 0, H = terrainFn(def);
  const geo = unitSphere(detail), pos = geo.attributes.position, col = new Float32Array(pos.count * 3);
  const pal = Object.fromEntries(Object.entries(def.palette).map(([k, v]) => [k, new THREE.Color(v)]));
  for (let i = 0; i < pos.count; i++) {
    _v.fromBufferAttribute(pos, i).normalize(); const h = H(_v), t = h - sea;
    _v.multiplyScalar(R * (1 + h * relief)); pos.setXYZ(i, _v.x, _v.y, _v.z);
    if (t < 0) _c.copy(pal.shore).lerp(pal.deep, Math.min(1, -t * 5));
    else if (t < 0.035) _c.copy(pal.shore);
    else if (t < 0.2) _c.copy(pal.low).lerp(pal.mid, (t - .035) / .165);
    else _c.copy(pal.mid).lerp(pal.high, Math.min(1, (t - .2) / .2));
    if (t > 0.4) _c.lerp(pal.peak, Math.min(1, (t - .4) * 5));
    col[i * 3] = _c.r; col[i * 3 + 1] = _c.g; col[i * 3 + 2] = _c.b;
  }
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3)); geo.computeVertexNormals(); geo.computeBoundingSphere();
  const body = new THREE.Mesh(geo, toonMaterial({ vertexColors: true, rim: def.atmosphere?.color ?? '#8fd0ff', rimStrength: opts.surface ? .06 : .5, detail: opts.surface ? 1 : 0 }));
  body.name = 'body'; g.add(body);
  // Océan / lave: sphère au niveau de la mer avec attribut de profondeur (écume au rivage).
  const seaR = R * (1 + sea * relief);
  if (def.water !== false) {
    const wg = unitSphere(Math.max(8, Math.round(detail * .6))), wp = wg.attributes.position, dep = new Float32Array(wp.count);
    let any = false;
    for (let i = 0; i < wp.count; i++) { _v.fromBufferAttribute(wp, i).normalize(); const d = (sea - H(_v)) * 16; dep[i] = d; if (d > 0) any = true; _v.multiplyScalar(seaR); wp.setXYZ(i, _v.x, _v.y, _v.z); }
    if (any) {
      wg.setAttribute('depth', new THREE.BufferAttribute(dep, 1));
      const lava = def.biome === 'volcanic';
      const water = new THREE.Mesh(wg, waterMaterial(def.waterColors ?? (lava ? { shallow: '#ffd24a', deep: '#ff4a1c', foam: '#fff3b0' } : { shallow: '#6fe6ff', deep: def.palette.deep, foam: '#ffffff' })));
      water.name = 'water'; water.renderOrder = 1; g.add(water);
    }
  }
  if (!opts.surface) {
    if (def.atmosphere) { const a = createAtmosphere(R * (1 + relief), def.atmosphere.color, def.atmosphere.power ?? 3, def.atmosphere.intensity ?? 1.1); a.name = 'atmo'; g.add(a); }
    if (def.clouds) {
      const cg = unitSphere(10), cp = cg.attributes.position, n = makeNoise(def.seed + 7), ca = new Float32Array(cp.count);
      for (let i = 0; i < cp.count; i++) { _v.fromBufferAttribute(cp, i).normalize(); const k = n.fbm(_v.x * 2.4, _v.y * 3.2, _v.z * 2.4, 3); ca[i] = THREE.MathUtils.smoothstep(k, .02, .22); _v.multiplyScalar(R * (1.09 + Math.max(0, k) * .1)); cp.setXYZ(i, _v.x, _v.y, _v.z); }
      cg.setAttribute('alpha', new THREE.BufferAttribute(ca, 1)); cg.computeVertexNormals();
      const cm = new THREE.MeshToonMaterial({ color: def.clouds, emissive: def.clouds, emissiveIntensity: .45, transparent: true, depthWrite: false });
      cm.onBeforeCompile = (s) => { s.vertexShader = s.vertexShader.replace('#include <common>', '#include <common>\nattribute float alpha; varying float vA;').replace('#include <begin_vertex>', '#include <begin_vertex>\nvA=alpha;');
        s.fragmentShader = s.fragmentShader.replace('#include <common>', '#include <common>\nvarying float vA;').replace('#include <opaque_fragment>', '#include <opaque_fragment>\ngl_FragColor.a*=step(.5,vA)*.9; if(gl_FragColor.a<.05) discard;'); };
      const clouds = new THREE.Mesh(cg, cm); clouds.name = 'clouds'; clouds.renderOrder = 2; g.add(clouds);
    }
    if (def.ring) {
      const ring = new THREE.Mesh(new THREE.RingGeometry(R * 1.5, R * 2.3, 64), new THREE.MeshBasicMaterial({ color: def.ring, side: THREE.DoubleSide, transparent: true, opacity: .55 }));
      ring.rotation.x = Math.PI / 2.4; ring.name = 'ring'; g.add(ring);
    }
  }
  const clouds = g.getObjectByName('clouds');
  g.userData = { def, radius: R, seaRadius: seaR, sea, relief, sample: H, heightAt: (dir) => R * (1 + H(dir) * relief), update: (dt) => { if (clouds) clouds.rotation.y += dt * 0.02; } };
  return g;
}
export function disposePlanet(g) { g.traverse(o => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose?.(); }); }
