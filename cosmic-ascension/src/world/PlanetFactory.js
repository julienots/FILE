import * as THREE from 'three';
import { makeNoise } from './noise.js';
import { toonMaterial } from '../render/shaders/Toon.js';
import { createAtmosphere } from '../render/shaders/Atmosphere.js';
import { Config } from '../core/Config.js';

// Construit une planète stylisée à partir d'une définition data (src/data/planets.js).
// def: {id, radius, seed, palette:{deep,shore,low,mid,high,peak}, seaLevel, relief, atmosphere, clouds, ring, moons}
export function buildPlanet(def, detail = Config.qualityPresets[Config.quality.level].planetDetail) {
  const g = new THREE.Group(); g.name = 'planet:' + def.id;
  const n = makeNoise(def.seed), R = def.radius, relief = def.relief ?? 0.08, sea = def.seaLevel ?? 0.0;
  const geo = new THREE.IcosahedronGeometry(R, detail), pos = geo.attributes.position, col = new Float32Array(pos.count * 3);
  const pal = Object.fromEntries(Object.entries(def.palette).map(([k, v]) => [k, new THREE.Color(v)]));
  const c = new THREE.Color(), v = new THREE.Vector3();
  const heights = new Float32Array(pos.count);
  for (let i = 0; i < pos.count; i++) {
    v.fromBufferAttribute(pos, i).normalize();
    let h = n.fbm(v.x * 1.6, v.y * 1.6, v.z * 1.6, 5);        // [-1,1] approx
    h = h > sea ? h : sea + (h - sea) * 0.15;                  // océans plats
    heights[i] = h; v.multiplyScalar(R * (1 + h * relief)); pos.setXYZ(i, v.x, v.y, v.z);
    const t = h - sea;
    if (t < -0.01) c.copy(pal.deep).lerp(pal.shore, Math.max(0, 1 + t * 4));
    else if (t < 0.04) c.copy(pal.shore);
    else if (t < 0.22) c.copy(pal.low).lerp(pal.mid, (t - .04) / .18);
    else c.copy(pal.mid).lerp(pal.high, Math.min(1, (t - .22) / .2));
    if (t > 0.42) c.lerp(pal.peak, Math.min(1, (t - .42) * 4));
    const k = 0.94 + Math.random() * 0.12; col[i * 3] = c.r * k; col[i * 3 + 1] = c.g * k; col[i * 3 + 2] = c.b * k;
  }
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3)); geo.computeVertexNormals();
  const body = new THREE.Mesh(geo, toonMaterial({ vertexColors: true, rim: def.atmosphere?.color ?? '#8fd0ff', rimStrength: .5, flatShading: detail < 4 }));
  body.name = 'body'; g.add(body);
  if (def.atmosphere) { const a = createAtmosphere(R * (1 + relief), def.atmosphere.color, def.atmosphere.power ?? 3, def.atmosphere.intensity ?? 1.1); a.name = 'atmo'; g.add(a); }
  if (def.clouds) {
    const cg = new THREE.IcosahedronGeometry(R * 1.07, 2), cp = cg.attributes.position;
    for (let i = 0; i < cp.count; i++) { v.fromBufferAttribute(cp, i).normalize(); const k = 1 + Math.max(0, n.fbm(v.x * 2.5 + 9, v.y * 2.5, v.z * 2.5, 3)) * .22; v.multiplyScalar(R * 1.07 * k); cp.setXYZ(i, v.x, v.y, v.z); }
    cg.computeVertexNormals();
    const clouds = new THREE.Mesh(cg, new THREE.MeshToonMaterial({ color: def.clouds, transparent: true, opacity: .85, flatShading: true }));
    clouds.name = 'clouds'; g.add(clouds);
  }
  if (def.ring) {
    const rg = new THREE.RingGeometry(R * 1.5, R * 2.3, 64), ring = new THREE.Mesh(rg, new THREE.MeshBasicMaterial({ color: def.ring, side: THREE.DoubleSide, transparent: true, opacity: .55 }));
    ring.rotation.x = Math.PI / 2.4; ring.name = 'ring'; g.add(ring);
  }
  g.userData = { def, radius: R, noise: n, heightAt: (dir) => { const hh = n.fbm(dir.x * 1.6, dir.y * 1.6, dir.z * 1.6, 5); return R * (1 + (hh > sea ? hh : sea + (hh - sea) * .15) * relief); } };
  g.userData.update = (dt) => { const cl = g.getObjectByName('clouds'); if (cl) cl.rotation.y += dt * 0.02; };
  return g;
}
