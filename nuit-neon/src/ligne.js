// Ligne de trajectoire : la meilleure ligne du circuit, dessinée sur la piste devant le joueur.
// Verte : on peut accélérer. Jaune : lever le pied. Rouge : freiner.
import * as THREE from 'three';

let mat = null;
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

// T : tracé échantillonné (world.js), phys : grandeurs physiques du véhicule du joueur
export function buildLine(T, phys) {
  const N = T.N, off = new Float32Array(N), X = new Float32Array(N), Z = new Float32Array(N), lim = Math.max(0.4, T.limit - 1.3);
  const place = (i) => { X[i] = T.px[i] - T.tz[i] * off[i]; Z[i] = T.pz[i] + T.tx[i] * off[i]; };
  for (let i = 0; i < N; i++) place(i);
  // Ligne de moindre courbure : chaque point glisse en travers de la piste vers la position qui « redresse » le plus la ligne
  // entre ses quatre voisins. Elle aborde ainsi les virages par l'extérieur, passe à la corde et ressort large, comme en course.
  for (const w of [20, 10, 5, 2, 1]) {
    if (w * 4 >= N) continue;
    for (let it = 0; it < 70; it++) for (let i = 0; i < N; i++) {
      const a = (i - w + N) % N, b = (i + w) % N, a2 = (i - 2 * w + N) % N, b2 = (i + 2 * w) % N;
      const mx = (4 * (X[a] + X[b]) - X[a2] - X[b2]) / 6, mz = (4 * (Z[a] + Z[b]) - Z[a2] - Z[b2]) / 6;
      const o = (mx - T.px[i]) * (-T.tz[i]) + (mz - T.pz[i]) * T.tx[i];
      off[i] = clamp(off[i] + (o - off[i]) * 0.6, -lim, lim); place(i);
    }
  }
  // courbure de la ligne, puis vitesse tenable en chaque point, en tenant compte de la distance de freinage
  const v = new Float32Array(N), m = 3, aLat = phys.grip * 1.02, bEff = phys.brake * 0.8;
  for (let i = 0; i < N; i++) {
    const a = (i - m + N) % N, b = (i + m) % N, ux = X[i] - X[a], uz = Z[i] - Z[a], wx = X[b] - X[i], wz = Z[b] - Z[i], cx = X[b] - X[a], cz = Z[b] - Z[a];
    const k = Math.abs(2 * (ux * wz - uz * wx) / Math.max(1e-6, Math.hypot(ux, uz) * Math.hypot(wx, wz) * Math.hypot(cx, cz)));
    v[i] = Math.min(phys.vmax, k > 1e-5 ? Math.min(Math.sqrt(aLat / k), phys.steer / k) : phys.vmax);
  }
  for (let pass = 0; pass < 2; pass++) for (let i = N - 1; i >= 0; i--) { const j = (i + 1) % N, d = Math.hypot(X[j] - X[i], Z[j] - Z[i]); v[i] = Math.min(v[i], Math.sqrt(v[j] * v[j] + 2 * bEff * d)); }

  // ruban posé sur la route
  const pos = new Float32Array(N * 6), uv = new Float32Array(N * 4), col = new Float32Array(N * 8), idx = new Uint32Array(N * 6);
  let s = 0;
  for (let i = 0; i < N; i++) {
    const a = (i - 1 + N) % N, b = (i + 1) % N; let tx = X[b] - X[a], tz = Z[b] - Z[a]; const l = Math.hypot(tx, tz) || 1; tx /= l; tz /= l;
    const hw = 0.7;
    pos.set([X[i] + tz * hw, 0.045, Z[i] - tx * hw, X[i] - tz * hw, 0.045, Z[i] + tx * hw], i * 6);
    uv.set([0, s / 2.6, 1, s / 2.6], i * 4);
    s += Math.hypot(X[b] - X[i], Z[b] - Z[i]);
    const j = (i + 1) % N; idx.set([i * 2, i * 2 + 1, j * 2, i * 2 + 1, j * 2 + 1, j * 2], i * 6);
  }
  uv[0] = uv[2] = 0;
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3)); g.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  const cAttr = new THREE.BufferAttribute(col, 4).setUsage(THREE.DynamicDrawUsage); g.setAttribute('color', cAttr); g.setIndex(new THREE.BufferAttribute(idx, 1));
  if (!mat) mat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, side: THREE.DoubleSide, polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2,
    vertexShader: 'attribute vec4 color; varying vec4 vC; varying vec2 vU; void main(){ vC = color; vU = uv; gl_Position = projectionMatrix*modelViewMatrix*vec4(position,1.); }',
    fragmentShader: `varying vec4 vC; varying vec2 vU;
      void main(){ float x = abs(vU.x - 0.5) * 2.0; float ch = fract(vU.y - x * 0.38);                 // chevrons pointés vers l'avant
        float m = smoothstep(0.0, 0.07, ch) * smoothstep(0.66, 0.56, ch) * smoothstep(1.0, 0.8, x);
        float a = vC.a * (0.25 + 0.75 * m); if (a < 0.01) discard;
        gl_FragColor = vec4(vC.rgb * (0.7 + 0.6 * m), a);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }` });
  const mesh = new THREE.Mesh(g, mat); mesh.frustumCulled = false; mesh.layers.set(1); mesh.renderOrder = 6;
  let last = -1;
  const AHEAD = Math.min(N - 2, Math.round(190 / T.ds));
  // recolore la portion visible devant le véhicule, selon sa vitesse comparée à la vitesse conseillée
  const update = (c) => {
    if (last >= 0) for (let q = 0; q <= AHEAD; q++) { const i = (last + q) % N; col[i * 8 + 3] = col[i * 8 + 7] = 0; }
    const sp = Math.abs(c.v); last = ((c.idx % N) + N) % N;
    for (let q = 2; q <= AHEAD; q++) {
      const i = (last + q) % N, r = sp / Math.max(1, v[i]);
      let R, G, B;
      if (r < 0.96) { R = 0.1; G = 1.0; B = 0.4; }
      else if (r < 1.1) { const t = (r - 0.96) / 0.14; R = 0.1 + 0.9 * t; G = 1.0 - 0.15 * t; B = 0.4 - 0.3 * t; }
      else { const t = Math.min(1, (r - 1.1) / 0.18); R = 1.0; G = 0.85 - 0.75 * t; B = 0.1; }
      const a = Math.min(1, (q - 2) / 4) * Math.min(1, (AHEAD - q) / (AHEAD * 0.35)) * 0.8;
      for (const k of [i * 8, i * 8 + 4]) { col[k] = R * 1.5; col[k + 1] = G * 1.5; col[k + 2] = B * 1.5; col[k + 3] = a; }
    }
    cAttr.needsUpdate = true;
  };
  return { mesh, update, v, off, X, Z };
}
