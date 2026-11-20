import * as THREE from 'three';
import { fbm, ridged, noise, smoothstep, lerp, clamp } from './noise.js';

// ---- Dimensions du monde (unités = mètres, un habitant mesure ~1,8 m) ----
export const DOME_R = 120;      // rayon du dôme
export const WORLD_R = 116;     // rayon du disque de terrain
const SIZE = 240;               // emprise de la grille
const N = 321;                  // sommets par côté (~0,75 m)
const STEP = SIZE / (N - 1);

export const VILLAGE = new THREE.Vector2(-30, 16);
export const LAKE = { x: 40, z: 70, r: 15, level: 0 };

const RIVER_PTS = [
  [-8, -88], [-6, -72], [-3, -58], [-2, -50], [-0.5, -45], [2, -38], [5, -26], [8, -12],
  [6, 0], [8, 14], [15, 30], [26, 49], [36, 64],
];
const PATHS = [
  [[-30, 16], [-16, 12], [-2, 9], [7, 8], [20, 4], [36, -6], [52, -12]],
  [[-30, 16], [-33, 34], [-28, 52], [-18, 66]],
  [[-30, 16], [-45, 5], [-56, -4]],
  [[-30, 16], [-24, 0], [-14, -16], [-6, -30]],
];

function sampleCurve(pts, spacing) {
  const curve = new THREE.CatmullRomCurve3(pts.map(([x, z]) => new THREE.Vector3(x, 0, z)), false, 'centripetal');
  const n = Math.max(8, Math.round(curve.getLength() / spacing));
  return curve.getSpacedPoints(n).map(v => new THREE.Vector2(v.x, v.z));
}

// distance au segment le plus proche d'une polyligne échantillonnée (recherche grossière puis fine)
function nearestOnPolyline(samples, x, z, coarse = 8) {
  let best = Infinity, bi = 0;
  for (let i = 0; i < samples.length; i += coarse) {
    const s = samples[i]; const d = (s.x - x) ** 2 + (s.y - z) ** 2;
    if (d < best) { best = d; bi = i; }
  }
  if (best > 45 * 45) return { d: Math.sqrt(best), i: bi, t: 0 };
  best = Infinity; let bt = 0, bseg = bi;
  const i0 = Math.max(0, bi - coarse - 1), i1 = Math.min(samples.length - 2, bi + coarse + 1);
  for (let i = i0; i <= i1; i++) {
    const a = samples[i], b = samples[i + 1];
    const abx = b.x - a.x, abz = b.y - a.y;
    const t = clamp(((x - a.x) * abx + (z - a.y) * abz) / (abx * abx + abz * abz), 0, 1);
    const px = a.x + abx * t - x, pz = a.y + abz * t - z;
    const d = px * px + pz * pz;
    if (d < best) { best = d; bseg = i; bt = t; }
  }
  return { d: Math.sqrt(best), i: bseg, t: bt };
}

// Relief "naturel" avant creusement de la rivière
function baseHeight(x, z) {
  const r = Math.hypot(x, z);
  const a = Math.atan2(x, -z); // 0 = nord
  let h = 5 + fbm(x * 0.013, z * 0.013, 4) * 7 + fbm(x * 0.05, z * 0.05, 3) * 1.3;

  // Haut plateau au nord, bordé d'une falaise (cascade)
  const edge = -45 + noise(x * 0.05, 3.1) * 3.5;
  const plateau = smoothstep(edge + 1.5, edge - 3.5, z) * smoothstep(78, 52, Math.abs(x));
  h = lerp(h, 30 + fbm(x * 0.04, z * 0.04, 3) * 3, plateau);

  // Chaîne de montagnes (arc nord) et collines rocheuses à l'ouest
  const mm = smoothstep(0.05, 0.8, Math.cos(a)) * smoothstep(42, 90, r);
  h += mm * (4 + ridged(x * 0.022, z * 0.022) * 46);
  const mm2 = smoothstep(0.55, 0.97, Math.cos(a + 1.85)) * smoothstep(55, 96, r);
  h += mm2 * (2 + ridged(x * 0.03 + 7, z * 0.03) * 26);

  // Village : replat
  const dv = Math.hypot(x - VILLAGE.x, z - VILLAGE.y);
  h = lerp(h, 6 + fbm(x * 0.03, z * 0.03, 2) * 0.6, smoothstep(36, 22, dv));
  // Colline du moulin
  const dm = Math.hypot(x + 58, z + 6);
  h += smoothstep(16, 2, dm) * 4.5;

  // Le monde se termine sur l'anneau du piédestal
  h = lerp(h, 0.6, smoothstep(100, 115, r));
  return h;
}

export function buildTerrain() {
  const heights = new Float32Array(N * N);
  const pathMask = new Float32Array(N * N);
  const wetMask = new Float32Array(N * N);

  // --- rivière : lit monotone décroissant obtenu en suivant le relief ---
  const river = sampleCurve(RIVER_PTS, 0.8);
  const bed = new Float32Array(river.length);
  let prev = Infinity;
  for (let i = 0; i < river.length; i++) {
    const h0 = baseHeight(river[i].x, river[i].y) - 1.7;
    prev = Math.min(prev - 0.015, h0);
    bed[i] = prev;
  }
  LAKE.level = bed[bed.length - 1] + 0.9;
  const riverWidth = i => 2.4 + (i / river.length) * 2.8;

  const paths = PATHS.map(p => sampleCurve(p, 1.0));

  for (let iz = 0; iz < N; iz++) {
    for (let ix = 0; ix < N; ix++) {
      const x = -SIZE / 2 + ix * STEP, z = -SIZE / 2 + iz * STEP;
      const k = iz * N + ix;
      let h = baseHeight(x, z);
      let wet = 0;

      const nr = nearestOnPolyline(river, x, z);
      if (nr.d < 14) {
        const i = nr.i, b = lerp(bed[i], bed[Math.min(i + 1, bed.length - 1)], nr.t);
        const w = riverWidth(i);
        let carved;
        if (nr.d < w) carved = b - 0.7 * (1 - (nr.d / w) ** 2);
        else carved = lerp(b, h, smoothstep(w, w + 6, nr.d));
        h = Math.min(h, carved);
        wet = 1 - smoothstep(w, w + 3.5, nr.d);
      }
      const dl = Math.hypot(x - LAKE.x, z - LAKE.z);
      if (dl < LAKE.r + 8) {
        const lr = LAKE.r + noise(x * 0.08, z * 0.08) * 2.5;
        const carved = dl < lr ? LAKE.level - 1 - 2.2 * (1 - (dl / lr) ** 2) : lerp(LAKE.level - 1, h, smoothstep(lr, lr + 7, dl));
        h = Math.min(h, carved);
        wet = Math.max(wet, 1 - smoothstep(lr, lr + 3.5, dl));
      }

      let pm = 0;
      for (const p of paths) {
        const np = nearestOnPolyline(p, x, z, 4);
        pm = Math.max(pm, 1 - smoothstep(1.0, 2.1, np.d + noise(x * 0.4, z * 0.4) * 0.5));
      }
      const dv = Math.hypot(x - VILLAGE.x, z - VILLAGE.y);
      pm = Math.max(pm, 1 - smoothstep(6.5, 8.5, dv + noise(x * 0.3, z * 0.3) * 0.8));
      if (pm > 0) h -= pm * 0.08;

      heights[k] = h; pathMask[k] = pm; wetMask[k] = wet;
    }
  }

  // --- géométrie ---
  const geo = new THREE.PlaneGeometry(SIZE, SIZE, N - 1, N - 1);
  geo.rotateX(-Math.PI / 2);
  const pos = geo.attributes.position;
  for (let i = 0; i < pos.count; i++) {
    // PlaneGeometry tournée : ligne par ligne de z=-S/2 à z=+S/2
    const x = pos.getX(i), z = pos.getZ(i);
    const ix = Math.round((x + SIZE / 2) / STEP), iz = Math.round((z + SIZE / 2) / STEP);
    pos.setY(i, heights[iz * N + ix]);
  }
  // on ne garde que le disque
  const idx = geo.index.array, keep = [];
  for (let t = 0; t < idx.length; t += 3) {
    let inside = false;
    for (let j = 0; j < 3; j++) { const v = idx[t + j]; if (Math.hypot(pos.getX(v), pos.getZ(v)) < WORLD_R + 2.5) inside = true; }
    if (inside) keep.push(idx[t], idx[t + 1], idx[t + 2]);
  }
  geo.setIndex(keep);
  geo.computeVertexNormals();

  // --- couleurs (palette peinte, chaude et saturée) ---
  const C = hex => new THREE.Color(hex);
  const grassA = C('#5f9a32'), grassB = C('#9cbf45'), grassDry = C('#b9b356'), forest = C('#3f7a2c');
  const dirt = C('#a08257'), sand = C('#cdbb8a'), rock = C('#8d877c'), rockDark = C('#6c6a68'), cliff = C('#9a8f80'), snow = C('#f4f7fb');
  const col = new Float32Array(pos.count * 3), tmp = new THREE.Color();
  const matW = new Float32Array(pos.count * 4);
  const nrm = geo.attributes.normal;
  const forestMask = (x, z) => smoothstep(0.05, 0.4, fbm(x * 0.02 + 11, z * 0.02 - 3, 3)) * (x > 20 || z > 40 ? 1 : 0.35);
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i), y = pos.getY(i), z = pos.getZ(i);
    const ix = Math.round((x + SIZE / 2) / STEP), iz = Math.round((z + SIZE / 2) / STEP);
    const k = iz * N + ix;
    const slope = 1 - nrm.getY(i);
    const n1 = fbm(x * 0.03, z * 0.03, 3), n2 = noise(x * 0.15, z * 0.15);
    tmp.copy(grassA).lerp(grassB, clamp(0.5 + n1 * 1.2, 0, 1));
    tmp.lerp(grassDry, smoothstep(0.25, 0.6, n2 * 0.5 + n1) * 0.45);
    tmp.lerp(forest, forestMask(x, z) * 0.6);
    tmp.lerp(sand, wetMask[k] * 0.85);
    tmp.lerp(dirt, pathMask[k] * 0.92);
    const rockAmt = smoothstep(0.22, 0.42, slope + n2 * 0.05);
    tmp.lerp(y > 25 ? cliff : rock, rockAmt);
    tmp.lerp(rockDark, smoothstep(0.5, 0.75, slope) * 0.5);
    const sn = smoothstep(46, 54, y + n1 * 8) * (1 - smoothstep(0.35, 0.6, slope));
    tmp.lerp(snow, sn);
    col[i * 3] = tmp.r; col[i * 3 + 1] = tmp.g; col[i * 3 + 2] = tmp.b;
    matW[i * 4] = Math.max(rockAmt, smoothstep(0.5, 0.75, slope)); matW[i * 4 + 1] = pathMask[k]; matW[i * 4 + 2] = wetMask[k]; matW[i * 4 + 3] = sn;
  }
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  geo.setAttribute('aMat', new THREE.BufferAttribute(matW, 4));

  const mat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.96, metalness: 0 });
  const mesh = new THREE.Mesh(geo, mat);
  mesh.receiveShadow = true;
  mesh.name = 'terrain';

  // --- requêtes ---
  function heightAt(x, z) {
    const fx = (x + SIZE / 2) / STEP, fz = (z + SIZE / 2) / STEP;
    const ix = clamp(Math.floor(fx), 0, N - 2), iz = clamp(Math.floor(fz), 0, N - 2);
    const tx = fx - ix, tz = fz - iz;
    const a = heights[iz * N + ix], b = heights[iz * N + ix + 1], c = heights[(iz + 1) * N + ix], d = heights[(iz + 1) * N + ix + 1];
    return lerp(lerp(a, b, tx), lerp(c, d, tx), tz);
  }
  function maskAt(arr, x, z) {
    const ix = clamp(Math.round((x + SIZE / 2) / STEP), 0, N - 1), iz = clamp(Math.round((z + SIZE / 2) / STEP), 0, N - 1);
    return arr[iz * N + ix];
  }
  function slopeAt(x, z) {
    const e = 0.8;
    const dx = heightAt(x + e, z) - heightAt(x - e, z), dz = heightAt(x, z + e) - heightAt(x, z - e);
    return Math.hypot(dx, dz) / (2 * e);
  }
  const vegetationOK = (x, z, maxSlope = 0.5) =>
    Math.hypot(x, z) < WORLD_R - 4 && maskAt(pathMask, x, z) < 0.25 && maskAt(wetMask, x, z) < 0.2 && slopeAt(x, z) < maxSlope && heightAt(x, z) < 47;

  return {
    mesh, heightAt, slopeAt, vegetationOK, forestMask, heights, pathMask, wetMask, N, SIZE, STEP,
    pathAt: (x, z) => maskAt(pathMask, x, z),
    wetAt: (x, z) => maskAt(wetMask, x, z),
    river, bed, riverWidth,
  };
}
