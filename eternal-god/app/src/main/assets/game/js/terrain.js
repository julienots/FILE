// Terrain déformable : grille d'altitude persistante, tronçons de maillage reconstruits
// à la demande, coloration par régions, brûlures et chaleur (lave) persistantes.
import * as THREE from 'three';
import { WORLD, G, Q, noise, clamp, lerp, smooth } from './core.js';
import { makeTerrainMaterial } from './terrainmat.js';

export const REGIONS = {
  village: { x: 0, z: 0, r: 150, name: 'Vallon d\'Aube' },
  lake: { x: 330, z: 300, r: 190, name: 'Lac des Murmures' },
  volcano: { x: 430, z: -430, r: 300, name: 'Mont Braise' },
  forest: { name: 'Forêt des Anciens' },
  mountains: { name: 'Pics de Givre' },
  sky: { name: 'Archipel Céleste' },
};

// Tracé de la rivière, de la source dans les montagnes jusqu'au lac.
export const RIVER = [
  [40, -560], [62, -400], [96, -250], [128, -120], [152, -20], [168, 90], [222, 185], [300, 262],
];

const N = WORLD.n, RES = WORLD.res, HALF = WORLD.half;

function distToRiver(x, z) {
  let best = 1e9, bt = 0, total = 0;
  const lens = [];
  for (let i = 0; i < RIVER.length - 1; i++) { const l = Math.hypot(RIVER[i + 1][0] - RIVER[i][0], RIVER[i + 1][1] - RIVER[i][1]); lens.push(l); total += l; }
  let acc = 0;
  for (let i = 0; i < RIVER.length - 1; i++) {
    const [ax, az] = RIVER[i], [bx, bz] = RIVER[i + 1];
    const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
    let t = ((x - ax) * dx + (z - az) * dz) / L2; t = clamp(t, 0, 1);
    const px = ax + dx * t, pz = az + dz * t;
    const d = Math.hypot(x - px, z - pz);
    if (d < best) { best = d; bt = (acc + lens[i] * t) / total; }
    acc += lens[i];
  }
  return [best, bt];
}
export function riverBed(t) { return 1.5 + 78 * Math.pow(1 - t, 2.6); }

export function generatorHeight(x, z) {
  const n = noise;
  // distorsion de domaine : formes organiques, vallées sinueuses
  const wx = x + n.fbm(x * 0.0016 + 4.1, z * 0.0016 - 2.3, 3) * 140;
  const wz = z + n.fbm(x * 0.0016 - 7.9, z * 0.0016 + 5.4, 3) * 140;
  let h = 17 + n.fbm(wx * 0.0019 + 3.1, wz * 0.0019 - 7.7, 5) * 16 + n.fbm(x * 0.011, z * 0.011, 3) * 1.6;
  // massif : crêtes adoucies (crêtes au carré puis lissées) + grandes masses
  const r = n.ridged(wx * 0.0016 + 11, wz * 0.0016 + 5, 6);
  const mass = 0.5 + 0.5 * n.fbm(wx * 0.0011 - 3, wz * 0.0011 + 8, 3);
  const peaks = Math.pow(r, 1.8) * 0.6 + mass * 0.55;
  // Pics de Givre au nord
  const mN = smooth(-180, -480, z);
  h += mN * mN * (25 + peaks * 230);
  // collines de la forêt à l'ouest
  const mW = smooth(-110, -420, x);
  h += mW * (n.fbm(wx * 0.004 + 9, wz * 0.004, 4) * 18 + 10);
  // remparts montagneux en bordure du monde
  const e = Math.max(Math.abs(x), Math.abs(z)) / HALF;
  const eb = smooth(0.78, 0.99, e);
  h += eb * eb * (110 + peaks * 230);
  // volcan
  const V = REGIONS.volcano;
  const dv = Math.hypot(x - V.x, z - V.z);
  if (dv < 320) {
    const k = 1 - dv / 320;
    let hv = 255 * Math.pow(k, 1.55) + 12 + n.fbm(x * 0.02, z * 0.02, 2) * 5 * k;
    if (dv < 48) hv -= 42 * Math.pow(1 - dv / 48, 0.6);
    h = Math.max(h, lerp(h, hv, smooth(0, 0.25, k)));
  }
  // plateau du village
  const d0 = Math.hypot(x - REGIONS.village.x, z - REGIONS.village.z);
  const f = 1 - smooth(105, 200, d0);
  h = lerp(h, 19.5 + n.fbm(x * 0.01, z * 0.01, 2) * 1.2, f);
  // lac
  const L = REGIONS.lake;
  const dl = Math.hypot(x - L.x, z - L.z) + n.noise(x * 0.01, z * 0.01) * 28;
  const l = 1 - smooth(70, L.r, dl);
  h = lerp(h, -9 + dl * 0.03, l);
  // rivière
  const [dr, t] = distToRiver(x, z);
  if (dr < 40) {
    const bed = riverBed(t);
    const w = 1 - smooth(6, 30, dr);
    const target = Math.min(h, bed + (dr / 30) * 6);
    h = lerp(h, target, w);
  }
  return h;
}

export class Terrain {
  constructor(scene) {
    this.scene = scene;
    this.h = new Float32Array(N * N);
    this.base = new Float32Array(N * N);
    this.burn = new Uint8Array(N * N);       // 0..255 calcination
    this.heat = new Float32Array(N * N);     // 0..1 lave / incandescence
    this.paint = new Uint8Array(N * N);      // 1 chemin, 2 champ, 3 pavés
    this.dirty = new Set();
    this.hotChunks = new Set();
    this.texDirty = false;
    this.texTimer = 0;
    this.heatTimer = 0;
    this.version = 0;
  }

  generate(progress) {
    const h = this.h;
    for (let j = 0; j < N; j++) {
      const z = -HALF + j * RES;
      for (let i = 0; i < N; i++) {
        h[j * N + i] = generatorHeight(-HALF + i * RES, z);
      }
      if (progress && (j & 63) === 0) progress(j / N);
    }
    this.base.set(h);
  }

  idx(x, z) {
    const i = clamp(Math.round((x + HALF) / RES), 0, N - 1), j = clamp(Math.round((z + HALF) / RES), 0, N - 1);
    return j * N + i;
  }

  heightAt(x, z) {
    const fx = clamp((x + HALF) / RES, 0, N - 1.001), fz = clamp((z + HALF) / RES, 0, N - 1.001);
    const i = Math.floor(fx), j = Math.floor(fz), u = fx - i, v = fz - j;
    const h = this.h, k = j * N + i;
    // découpage triangulaire identique au maillage
    if (u + v < 1) return h[k] + (h[k + 1] - h[k]) * u + (h[k + N] - h[k]) * v;
    return h[k + N + 1] + (h[k + N] - h[k + N + 1]) * (1 - u) + (h[k + 1] - h[k + N + 1]) * (1 - v);
  }

  groundAt(x, z) { return Math.max(this.heightAt(x, z), WORLD.water); }

  normalAt(x, z, out = new THREE.Vector3()) {
    const e = 1.5;
    out.set(this.heightAt(x - e, z) - this.heightAt(x + e, z), 2 * e, this.heightAt(x, z - e) - this.heightAt(x, z + e));
    return out.normalize();
  }

  inWorld(x, z, m = 0) { return Math.abs(x) < HALF - m && Math.abs(z) < HALF - m; }

  // Lancer de rayon sur la grille (marche puis dichotomie)
  raycast(o, d, maxDist = 2000) {
    let t = 0, step = 2, prevT = 0;
    const p = new THREE.Vector3();
    while (t < maxDist) {
      p.copy(o).addScaledVector(d, t);
      if (!this.inWorld(p.x, p.z) && t > 10) return null;
      if (p.y <= this.heightAt(p.x, p.z)) {
        let a = prevT, b = t;
        for (let k = 0; k < 10; k++) {
          const m = (a + b) / 2;
          p.copy(o).addScaledVector(d, m);
          if (p.y <= this.heightAt(p.x, p.z)) b = m; else a = m;
        }
        p.copy(o).addScaledVector(d, b);
        return { point: p, distance: b };
      }
      prevT = t;
      step = Math.min(12, 1 + t * 0.02);
      t += step;
    }
    return null;
  }

  // ---------- Maillage ----------
  build(scene) {
    const seg = WORLD.chunkSeg, vps = seg + 1;
    const idx = [];
    for (let j = 0; j < seg; j++) for (let i = 0; i < seg; i++) {
      const a = j * vps + i, b = a + 1, c = a + vps, d = c + 1;
      idx.push(a, c, b, b, c, d);
    }
    this.index = new THREE.BufferAttribute(new Uint16Array(idx), 1);
    this.material = makeTerrainMaterial(this.textures);
    this.meshes = [];
    const C = WORLD.chunks;
    for (let cz = 0; cz < C; cz++) for (let cx = 0; cx < C; cx++) {
      const g = new THREE.BufferGeometry();
      g.setIndex(this.index);
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vps * vps * 3), 3));
      g.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(vps * vps * 3), 3));
      g.setAttribute('splat', new THREE.BufferAttribute(new Float32Array(vps * vps * 4), 4));
      g.setAttribute('fx', new THREE.BufferAttribute(new Float32Array(vps * vps * 4), 4));
      g.setAttribute('heat', new THREE.BufferAttribute(new Float32Array(vps * vps), 1));
      const m = new THREE.Mesh(g, this.material);
      m.receiveShadow = true;
      m.userData.chunk = [cx, cz];
      m.matrixAutoUpdate = false;
      this.meshes.push(m);
      scene.add(m);
      this.rebuildChunk(cx, cz);
    }
    this.buildDataTexture();
  }

  rebuildChunk(cx, cz) {
    const seg = WORLD.chunkSeg, vps = seg + 1, C = WORLD.chunks;
    const m = this.meshes[cz * C + cx];
    const g = m.geometry;
    const pos = g.attributes.position.array, nor = g.attributes.normal.array, spl = g.attributes.splat.array, fx = g.attributes.fx.array, heat = g.attributes.heat.array;
    const h = this.h;
    const i0 = cx * seg, j0 = cz * seg;
    let minY = 1e9, maxY = -1e9, hot = false;
    for (let j = 0; j < vps; j++) {
      const gj = j0 + j;
      for (let i = 0; i < vps; i++) {
        const gi = i0 + i, k = gj * N + gi, v = j * vps + i;
        const y = h[k];
        pos[v * 3] = -HALF + gi * RES; pos[v * 3 + 1] = y; pos[v * 3 + 2] = -HALF + gj * RES;
        if (y < minY) minY = y; if (y > maxY) maxY = y;
        const hl = h[gj * N + Math.max(0, gi - 1)], hr = h[gj * N + Math.min(N - 1, gi + 1)];
        const hd = h[Math.max(0, gj - 1) * N + gi], hu = h[Math.min(N - 1, gj + 1) * N + gi];
        let nx = hl - hr, ny = 2 * RES, nz = hd - hu;
        const inv = 1 / Math.hypot(nx, ny, nz); nx *= inv; ny *= inv; nz *= inv;
        nor[v * 3] = nx; nor[v * 3 + 1] = ny; nor[v * 3 + 2] = nz;
        this.splatAt(k, gi, gj, y, spl, fx, v * 4);
        heat[v] = this.heat[k];
        if (this.heat[k] > 0.01) hot = true;
      }
    }
    g.attributes.position.needsUpdate = true;
    g.attributes.normal.needsUpdate = true;
    g.attributes.splat.needsUpdate = true;
    g.attributes.fx.needsUpdate = true;
    g.attributes.heat.needsUpdate = true;
    const cxw = -HALF + (i0 + seg / 2) * RES, czw = -HALF + (j0 + seg / 2) * RES;
    g.boundingSphere = new THREE.Sphere(new THREE.Vector3(cxw, (minY + maxY) / 2, czw), Math.hypot(WORLD.chunkSize * 0.71, (maxY - minY) / 2 + 2));
    g.boundingBox = null;
    if (hot) this.hotChunks.add(cz * C + cx); else this.hotChunks.delete(cz * C + cx);
  }

  // poids des couches de texture : forêt, chemin, champ, pavés | brûlure, terre remuée, volcan
  splatAt(k, gi, gj, y, spl, fx, o) {
    const x = -HALF + gi * RES, z = -HALF + gj * RES;
    const forest = smooth(-120, -250, x) * (1 - smooth(-300, -470, z)) * (0.75 + 0.25 * noise.noise(x * 0.02, z * 0.02));
    // poids lissés sur les voisins : bords doux pour chemins, champs et pavés
    let w1 = 0, w2 = 0, w3 = 0, cnt = 0;
    for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) {
      const ii = clamp(gi + di, 0, N - 1), jj = clamp(gj + dj, 0, N - 1), p = this.paint[jj * N + ii], wt = di || dj ? 1 : 2;
      if (p === 1) w1 += wt; else if (p === 2) w2 += wt; else if (p === 3) w3 += wt;
      cnt += wt;
    }
    spl[o] = clamp(forest, 0, 1);
    spl[o + 1] = (w1 / cnt) * 0.85;
    spl[o + 2] = w2 / cnt;
    spl[o + 3] = w3 / cnt;
    const V = REGIONS.volcano;
    const dug = this.base[k] - y;
    fx[o] = this.burn[k] / 255;
    fx[o + 1] = dug > 0 ? dug : -dug * 0.6;
    fx[o + 2] = 1 - smooth(140, 290, Math.hypot(x - V.x, z - V.z));
    fx[o + 3] = 0;
  }

  markDirtyArea(x, z, rad) {
    const C = WORLD.chunks, cs = WORLD.chunkSize;
    const x0 = Math.floor((x - rad - RES + HALF) / cs), x1 = Math.floor((x + rad + RES + HALF) / cs);
    const z0 = Math.floor((z - rad - RES + HALF) / cs), z1 = Math.floor((z + rad + RES + HALF) / cs);
    for (let cz = Math.max(0, z0); cz <= Math.min(C - 1, z1); cz++)
      for (let cx = Math.max(0, x0); cx <= Math.min(C - 1, x1); cx++) this.dirty.add(cz * C + cx);
    this.texDirty = true;
    this.version++;
  }

  // Cratère : creuse, forme un bourrelet, brûle et chauffe le sol.
  crater(x, z, radius, depth, opts = {}) {
    const rim = opts.rim ?? 0.22, burnAmt = opts.burn ?? 0, heatAmt = opts.heat ?? 0;
    const ext = radius * 1.45;
    const i0 = Math.max(0, Math.floor((x - ext + HALF) / RES)), i1 = Math.min(N - 1, Math.ceil((x + ext + HALF) / RES));
    const j0 = Math.max(0, Math.floor((z - ext + HALF) / RES)), j1 = Math.min(N - 1, Math.ceil((z + ext + HALF) / RES));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const wx = -HALF + i * RES, wz = -HALF + j * RES;
      const d = Math.hypot(wx - x, wz - z) / radius;
      if (d > 1.45) continue;
      const k = j * N + i;
      let delta = 0;
      if (d < 1) delta = -depth * (1 - d * d) * (0.85 + 0.15 * noise.noise(wx * 0.3, wz * 0.3));
      if (rim > 0 && d > 0.75) delta += depth * rim * Math.max(0, 1 - Math.abs(d - 1.05) / 0.35);
      // bord du monde : on ne creuse pas sous le socle
      this.h[k] = Math.max(-45, this.h[k] + delta);
      if (burnAmt > 0 && d < 1.25) this.burn[k] = Math.min(255, this.burn[k] + burnAmt * 255 * clamp(1.25 - d, 0, 1));
      if (heatAmt > 0 && d < 0.9) this.heat[k] = Math.min(1, this.heat[k] + heatAmt * (1 - d / 0.9));
    }
    this.markDirtyArea(x, z, ext);
  }

  // Tranchée (laser) : creuse un sillon continu
  scorch(x, z, radius, depth, burn = 0.7, heat = 0.6) {
    this.crater(x, z, radius, depth, { rim: 0.05, burn, heat });
  }

  burnArea(x, z, radius, amount = 0.6) {
    const i0 = Math.max(0, Math.floor((x - radius + HALF) / RES)), i1 = Math.min(N - 1, Math.ceil((x + radius + HALF) / RES));
    const j0 = Math.max(0, Math.floor((z - radius + HALF) / RES)), j1 = Math.min(N - 1, Math.ceil((z + radius + HALF) / RES));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const d = Math.hypot(-HALF + i * RES - x, -HALF + j * RES - z) / radius;
      if (d < 1) { const k = j * N + i; this.burn[k] = Math.min(255, this.burn[k] + amount * 255 * (1 - d)); }
    }
    this.markDirtyArea(x, z, radius);
  }

  paintDisc(x, z, r, v) {
    const i0 = Math.max(0, Math.floor((x - r + HALF) / RES)), i1 = Math.min(N - 1, Math.ceil((x + r + HALF) / RES));
    const j0 = Math.max(0, Math.floor((z - r + HALF) / RES)), j1 = Math.min(N - 1, Math.ceil((z + r + HALF) / RES));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++)
      if (Math.hypot(-HALF + i * RES - x, -HALF + j * RES - z) <= r) this.paint[j * N + i] = v;
  }
  paintRect(x, z, w, d, rot, v) {
    const r = Math.hypot(w, d) / 2 + RES;
    const c = Math.cos(-rot), s = Math.sin(-rot);
    const i0 = Math.max(0, Math.floor((x - r + HALF) / RES)), i1 = Math.min(N - 1, Math.ceil((x + r + HALF) / RES));
    const j0 = Math.max(0, Math.floor((z - r + HALF) / RES)), j1 = Math.min(N - 1, Math.ceil((z + r + HALF) / RES));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const dx = -HALF + i * RES - x, dz = -HALF + j * RES - z;
      const lx = dx * c - dz * s, lz = dx * s + dz * c;
      if (Math.abs(lx) <= w / 2 && Math.abs(lz) <= d / 2) this.paint[j * N + i] = v;
    }
  }
  paintPath(ax, az, bx, bz, w) {
    const L = Math.hypot(bx - ax, bz - az), steps = Math.ceil(L / 2);
    for (let s = 0; s <= steps; s++) {
      const t = s / steps;
      const wob = noise.noise(ax + t * 10, az) * 3;
      this.paintDisc(lerp(ax, bx, t) + wob * (bz - az) / L, lerp(az, bz, t) - wob * (bx - ax) / L, w / 2, 1);
    }
  }
  // Aplanit une zone (fondations des bâtiments)
  flatten(x, z, r, y) {
    const i0 = Math.max(0, Math.floor((x - r * 1.6 + HALF) / RES)), i1 = Math.min(N - 1, Math.ceil((x + r * 1.6 + HALF) / RES));
    const j0 = Math.max(0, Math.floor((z - r * 1.6 + HALF) / RES)), j1 = Math.min(N - 1, Math.ceil((z + r * 1.6 + HALF) / RES));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const d = Math.hypot(-HALF + i * RES - x, -HALF + j * RES - z);
      const k = j * N + i;
      const w = 1 - smooth(r, r * 1.6, d);
      this.h[k] = lerp(this.h[k], y, w); this.base[k] = this.h[k];
    }
  }

  // Texture d'altitude (R) et densité d'herbe (G) pour les shaders de végétation
  buildDataTexture() {
    this.texData = new Uint16Array(N * N * 4);
    this.dataTex = new THREE.DataTexture(this.texData, N, N, THREE.RGBAFormat, THREE.HalfFloatType);
    this.dataTex.minFilter = this.dataTex.magFilter = THREE.LinearFilter;
    this.dataTex.wrapS = this.dataTex.wrapT = THREE.ClampToEdgeWrapping;
    this.fillDataTexture();
  }
  fillDataTexture() {
    const toH = THREE.DataUtils.toHalfFloat, d = this.texData;
    for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
      const k = j * N + i, y = this.h[k];
      const x = -HALF + i * RES, z = -HALF + j * RES;
      let dens = 1;
      dens *= smooth(WORLD.water + 0.8, WORLD.water + 2.5, y);
      dens *= 1 - smooth(110, 140, y);
      dens *= 1 - this.burn[k] / 255;
      dens *= 1 - smooth(0.2, 0.8, Math.abs(this.base[k] - y));
      if (this.paint[k]) dens = 0;
      const V = REGIONS.volcano;
      dens *= smooth(170, 240, Math.hypot(x - V.x, z - V.z));
      d[k * 4] = toH(y); d[k * 4 + 1] = toH(dens); d[k * 4 + 2] = 0; d[k * 4 + 3] = toH(1);
    }
    this.dataTex.needsUpdate = true;
  }

  update(dt) {
    // reconstruction progressive des tronçons modifiés
    let budget = 6;
    for (const id of this.dirty) {
      this.dirty.delete(id);
      this.rebuildChunk(id % WORLD.chunks, Math.floor(id / WORLD.chunks));
      if (--budget <= 0) break;
    }
    this.texTimer -= dt;
    if (this.texDirty && this.texTimer <= 0) { this.fillDataTexture(); this.texDirty = false; this.texTimer = 0.6; }
    // la lave refroidit lentement
    this.heatTimer -= dt;
    if (this.heatTimer <= 0 && this.hotChunks.size) {
      this.heatTimer = 0.5;
      const C = WORLD.chunks, seg = WORLD.chunkSeg, vps = seg + 1;
      for (const id of [...this.hotChunks]) {
        const cx = id % C, cz = Math.floor(id / C), g = this.meshes[id].geometry, a = g.attributes.heat.array;
        let any = false;
        for (let j = 0; j < vps; j++) for (let i = 0; i < vps; i++) {
          const k = (cz * seg + j) * N + cx * seg + i;
          if (this.heat[k] > 0) { this.heat[k] = this.heat[k] < 0.01 ? 0 : this.heat[k] * 0.985; a[j * vps + i] = this.heat[k]; any = true; }
        }
        g.attributes.heat.needsUpdate = true;
        if (!any) this.hotChunks.delete(id);
      }
    }
  }

  // Sauvegarde compacte des déformations (différences quantifiées au décimètre)
  serialize() {
    const diff = new Int16Array(N * N);
    let changed = 0;
    for (let k = 0; k < N * N; k++) { const d = Math.round((this.h[k] - this.base[k]) * 10); diff[k] = clamp(d, -32000, 32000); if (d) changed++; }
    if (!changed) return null;
    return { h: b64(new Uint8Array(diff.buffer)), b: b64(this.burn) };
  }
  restore(s) {
    if (!s) return;
    const diff = new Int16Array(unb64(s.h).buffer);
    for (let k = 0; k < N * N; k++) this.h[k] = this.base[k] + diff[k] / 10;
    this.burn.set(unb64(s.b));
    for (let id = 0; id < WORLD.chunks * WORLD.chunks; id++) this.dirty.add(id);
    this.texDirty = true;
  }
}

function b64(u8) { let s = ''; const CH = 0x8000; for (let i = 0; i < u8.length; i += CH) s += String.fromCharCode.apply(null, u8.subarray(i, i + CH)); return btoa(s); }
function unb64(str) { const s = atob(str); const u = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); return u; }
