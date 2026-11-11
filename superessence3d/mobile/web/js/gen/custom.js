// Modélisation libre pilotée par Claude : corps organiques sur mesure (os + volumes), pièces de toutes formes,
// déformations. Coordonnées en mètres, Y vers le haut, avant +Z.
import * as THREE from "three";
import { Organic, assembleOrganic, addEyes } from "./generators.js";
import { V, box, curvePoints, cylinder, doubleSided, ellipsoid, fanMembrane, lathe, loft, ellipseSection, sweep, taper } from "../geom.js";
import { applyPattern, boxUV, canvasTexture, eyeGeometry, eyeMaterial, fbm, makeMaterial } from "../materials.js";
import { taubin } from "../organic.js";

const v3 = (a, d = [0, 0, 0]) => (Array.isArray(a) && a.length === 3 ? V(+a[0], +a[1], +a[2]) : V(...d));
const num = (x, d) => (x === undefined || x === null || Number.isNaN(+x) ? d : +x);
export const colorArg = (c, parse) => {
  if (!c) return null;
  if (Array.isArray(c)) return c.slice(0, 3).map(Number);
  if (/^#?[0-9a-f]{6}$/i.test(c)) { const n = parseInt(c.replace("#", ""), 16); return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255]; }
  return parse ? parse(c) : null;
};

/** Matériau à partir d'une description courte : {material, color, color2, emissive, roughness, metallic}. */
export function partMaterial(p, parse) {
  const preset = p.material || (p.emissive ? "emission" : "plain");
  const color = colorArg(p.color, parse);
  const params = { color: color || undefined, color2: colorArg(p.color2, parse) || undefined, roughness: p.roughness, metallic: p.metallic, doubleSide: !!p.double_sided };
  if (p.emissive) { params.emission = colorArg(p.emissive === true ? p.color : p.emissive, parse) || color || [1, 1, 1]; params.emission_strength = num(p.emission_strength, 2); }
  const texKind = { brick: "brick", wood: "planks", stone: "stone", metal: null }[preset];
  const m = makeMaterial(preset, params);
  if (p.texture && ["brick", "planks", "stone", "tiles", "metal"].includes(p.texture)) {
    const c = color || [0.6, 0.6, 0.6];
    m.map = canvasTexture(p.texture, c, c.map((x) => x * 0.7), [1, 1]);
    m.color.set(0xffffff);
  } else if (texKind && p.texture !== false) void texKind;
  return m;
}

/** Fabrique la géométrie d'une pièce. */
export function partGeometry(p) {
  const s = p.shape;
  const size = Array.isArray(p.size) ? p.size.map(Number) : [num(p.size, 1), num(p.size, 1), num(p.size, 1)];
  const r = num(p.radius, 0.5), h = num(p.height, 1);
  const seg = Math.max(6, Math.min(96, num(p.segments, 32)));
  let g;
  switch (s) {
    case "box": g = box(size[0], size[1], size[2], num(p.bevel, 0)); break;
    case "sphere": g = new THREE.SphereGeometry(r, seg, Math.max(6, seg / 2)); break;
    case "ellipsoid": { const rr = Array.isArray(p.radii) ? p.radii.map(Number) : [r, r, r]; g = ellipsoid(rr[0], rr[1], rr[2], seg, Math.max(6, seg / 2)); break; }
    case "cylinder": g = cylinder(r, h, seg, p.radius_top !== undefined ? num(p.radius_top, r) : null); break;
    case "cone": g = cylinder(r, h, seg, 0.0001); break;
    case "torus": g = new THREE.TorusGeometry(r, num(p.tube, r * 0.25), Math.max(6, seg / 2), seg); break;
    case "capsule": g = new THREE.CapsuleGeometry(r, h, 8, seg); break;
    case "tube": {
      // tube effilé le long d'une courbe : points [[x,y,z]...] (lissés), radii [r0, r1...] ; tip:true = pointe
      const pts = (p.points || []).map((q) => v3(q));
      if (pts.length < 2) throw new Error("tube : au moins 2 points");
      const curve = new THREE.CatmullRomCurve3(pts);
      const n = Math.max(pts.length, num(p.samples, 24));
      const path = curve.getPoints(n - 1);
      const radii = Array.isArray(p.radii) && p.radii.length ? path.map((_, i) => { const t = i / (n - 1) * (p.radii.length - 1); const a = Math.floor(t), b = Math.min(p.radii.length - 1, a + 1); return +p.radii[a] + (+p.radii[b] - +p.radii[a]) * (t - a); }) : taper(n, r, p.tip ? 0 : r);
      g = sweep(path, radii, { sides: Math.min(24, Math.max(4, num(p.sides, 12))), tip: !!p.tip, flatten: num(p.flatten, 1) });
      break;
    }
    case "lathe": {
      // profil [[rayon, hauteur], ...] tourné autour de Y (vases, colonnes, bouteilles, casques, tours)
      if (!Array.isArray(p.profile) || p.profile.length < 2) throw new Error("lathe : profil [[rayon, hauteur], ...] requis");
      g = lathe(p.profile.map((q) => [+q[0], +q[1]]), seg);
      break;
    }
    case "extrude": {
      // contour 2D [[x,y], ...] extrudé sur « depth » (lames, panneaux, ailes de vaisseau, logos)
      if (!Array.isArray(p.outline) || p.outline.length < 3) throw new Error("extrude : contour [[x,y], ...] requis");
      const shape = new THREE.Shape(p.outline.map((q) => new THREE.Vector2(+q[0], +q[1])));
      for (const hole of p.holes || []) shape.holes.push(new THREE.Path(hole.map((q) => new THREE.Vector2(+q[0], +q[1]))));
      const bev = num(p.bevel, 0);
      g = new THREE.ExtrudeGeometry(shape, { depth: num(p.depth, 0.1), bevelEnabled: bev > 0, bevelSize: bev, bevelThickness: bev, bevelSegments: 2, curveSegments: 12 });
      g.translate(0, 0, -num(p.depth, 0.1) / 2);
      break;
    }
    case "loft": {
      // sections elliptiques le long de Z : [[z, rayon_x, rayon_y, centre_y?], ...] (carrosseries, fuselages, coques)
      const secs = (p.sections || []).map((q) => ellipseSection(0, num(q[3], 0), +q[0], +q[1], +q[2], 20, num(p.squareness, 0)));
      if (secs.length < 2) throw new Error("loft : au moins 2 sections [z, rx, ry, cy]");
      g = loft(secs);
      break;
    }
    case "membrane": {
      // surface en éventail depuis « origin » vers des rangées de points (ailes, voiles, nageoires)
      if (!p.origin || !Array.isArray(p.rows) || p.rows.length < 2) throw new Error("membrane : origin et rows [[[x,y,z]...], ...] requis");
      const rows = p.rows.map((row) => row.map((q) => v3(q)));
      const L = Math.min(...rows.map((x) => x.length));
      g = fanMembrane(v3(p.origin), rows.map((x) => x.slice(0, L)), num(p.sag, 0.05));
      break;
    }
    case "plane": g = new THREE.PlaneGeometry(size[0], size[2] ?? size[0], 16, 16).rotateX(-Math.PI / 2); break;
    case "icosphere": g = new THREE.IcosahedronGeometry(r, Math.min(5, num(p.detail, 3))); break;
    default: throw new Error(`Forme inconnue : ${s}. Formes : box, sphere, ellipsoid, cylinder, cone, torus, capsule, tube, lathe, extrude, loft, membrane, plane, icosphere`);
  }
  if (p.noise) {
    const pos = g.attributes.position, amp = +p.noise, sc = num(p.noise_scale, 3);
    g.computeVertexNormals();
    const n = g.attributes.normal;
    for (let i = 0; i < pos.count; i++) { const f = (fbm(pos.getX(i) * sc + 7, pos.getY(i) * sc, pos.getZ(i) * sc, 4) - 0.5) * amp; pos.setXYZ(i, pos.getX(i) + n.getX(i) * f, pos.getY(i) + n.getY(i) * f, pos.getZ(i) + n.getZ(i) * f); }
  }
  if (!g.attributes.normal || p.noise) g.computeVertexNormals();
  return g;
}

export function makePart(p, parse) {
  const g = partGeometry(p);
  const m = new THREE.Mesh(g, partMaterial(p, parse));
  if (p.texture) boxUV(g, num(p.texture_scale, 1));
  m.name = p.name || p.shape;
  m.userData.part = m.name;
  m.userData.def = p;
  m.position.copy(v3(p.position));
  const rot = Array.isArray(p.rotation) ? p.rotation.map((d) => THREE.MathUtils.degToRad(+d)) : [0, 0, 0];
  m.rotation.set(rot[0], rot[1], rot[2]);
  if (p.scale !== undefined) Array.isArray(p.scale) ? m.scale.set(+p.scale[0], +p.scale[1], +p.scale[2]) : m.scale.setScalar(+p.scale);
  m.castShadow = m.receiveShadow = true;
  if (p.mirror_x) {
    const g2 = new THREE.Group();
    g2.name = m.name;
    m.name = `${g2.name} D`;
    const c = m.clone(); c.name = `${g2.name} G`;
    c.position.x = -c.position.x; c.rotation.y = -c.rotation.y; c.rotation.z = -c.rotation.z;
    g2.add(m, c);
    g2.userData.part = g2.name;
    return g2;
  }
  return m;
}

/** Corps organique libre : os (capsules), volumes (ellipsoïdes, creux), yeux et pièces attachées aux os. */
export function buildOrganicModel(def, parse, quality = "standard") {
  const spec = { category: "creature", subtype: "custom", prompt: def.description || "modèle organique de Claude", colors: {}, features: {}, scales: {} };
  const o = new Organic(spec, def.name || "Créature");
  o.noRescale = !def.height;
  if (def.height) spec.height = +def.height;
  o.blend = def.smoothness !== undefined ? +def.smoothness : null;
  o.resolution = def.resolution ? Math.max(40, Math.min(200, +def.resolution)) : null;
  o.patternScale = num(def.pattern_scale, 1);
  if (def.relief !== undefined) o.relief = +def.relief;
  const bones = def.bones || [];
  if (!bones.length && !(def.blobs || []).length) throw new Error("Indiquez des os (bones) et/ou des volumes (blobs).");
  const names = new Set();
  o.bone("racine", V(0, 0, 0), V(0, 0.05, 0), null, "root");
  for (const b of bones) {
    if (!b.name || !b.head || !b.tail) throw new Error("Chaque os : name, head [x,y,z], tail [x,y,z], radius [r_début, r_fin].");
    const r = Array.isArray(b.radius) ? b.radius : [num(b.radius, 0.1), num(b.radius, 0.1)];
    const parent = b.parent && names.has(b.parent) ? b.parent : "racine";
    o.bone(b.name, v3(b.head), v3(b.tail), parent, b.role || "", b.radius === 0 ? null : +r[0], b.radius === 0 ? null : +(r[1] ?? r[0]));
    names.add(b.name);
  }
  for (const bl of def.blobs || []) {
    const bone = bl.bone && names.has(bl.bone) ? bl.bone : "racine";
    const rr = Array.isArray(bl.radii) ? bl.radii.map(Number) : [num(bl.radius, 0.2), num(bl.radius, 0.2), num(bl.radius, 0.2)];
    if (bl.from && bl.to) o.caps.push({ a: bl.from, b: bl.to, ra: num(bl.radius_from, rr[0]), rb: num(bl.radius_to, rr[0]), bone, field: true, sub: !!bl.subtract, blend: bl.smoothness });
    else o.caps.push({ type: "ellipsoid", c: bl.center, r: rr, bone, field: true, sub: !!bl.subtract, blend: bl.smoothness });
  }
  o.bodyPreset = def.material || "organic";
  o.bodyParams = { color: colorArg(def.color, parse) || [0.5, 0.3, 0.3], color2: colorArg(def.color2, parse) || undefined, veins: colorArg(def.veins, parse) || undefined, wet: def.wet ? 0.6 : 0 };
  for (const e of def.eyes || []) {
    const bone = e.bone && names.has(e.bone) ? e.bone : "racine";
    const c = colorArg(e.color, parse) || [0.95, 0.75, 0.1];
    const mat = eyeMaterial(c, !!e.glow);
    const rad = num(e.radius, 0.05);
    const look = v3(e.look || [0, 0, 1]).normalize();
    const mirror = e.mirror_x !== false && Math.abs(+e.position[0]) > 1e-4;
    for (const [pos, lk] of mirror ? [[e.position, look], [[-e.position[0], e.position[1], e.position[2]], V(-look.x, look.y, look.z)]] : [[e.position, look]]) {
      o.rigid.push({ geo: eyeGeometry(rad, lk, c, !!e.glow).translate(+pos[0], +pos[1], +pos[2]), mat, bone, name: "Œil" });
    }
  }
  for (const p of def.parts || []) {
    const bone = p.bone && names.has(p.bone) ? p.bone : null;
    const parts = p.mirror_x ? [p, { ...p, position: p.position ? [-p.position[0], p.position[1], p.position[2]] : undefined, rotation: p.rotation ? [p.rotation[0], -p.rotation[1], -p.rotation[2]] : undefined, points: p.points?.map((q) => [-q[0], q[1], q[2]]), origin: p.origin ? [-p.origin[0], p.origin[1], p.origin[2]] : undefined, rows: p.rows?.map((row) => row.map((q) => [-q[0], q[1], q[2]])) }] : [p];
    for (const q of parts) {
      const geo = partGeometry({ ...q, mirror_x: false });
      const m = new THREE.Matrix4().compose(v3(q.position), new THREE.Quaternion().setFromEuler(new THREE.Euler(...(q.rotation || [0, 0, 0]).map((d) => THREE.MathUtils.degToRad(+d)))), q.scale !== undefined ? (Array.isArray(q.scale) ? v3(q.scale) : V(+q.scale, +q.scale, +q.scale)) : V(1, 1, 1));
      geo.applyMatrix4(m);
      const mat = partMaterial(q, parse);
      if (q.follow_bones) o.skinned.push({ geo, mat, bones: Array.isArray(q.follow_bones) ? q.follow_bones : null, name: q.name || q.shape });
      else o.rigid.push({ geo, mat, bone: bone || "racine", name: q.name || q.shape });
    }
  }
  const root = assembleOrganic(o, quality);
  root.name = def.name || "Créature";
  root.userData.generated = false;
  root.userData.custom = true;
  root.userData.def = def;
  return root;
}

/** Déformations sur un maillage (en coordonnées locales). */
export function deformMesh(mesh, op, amount = 1, axis = "y") {
  const g = mesh.geometry, p = g.attributes.position;
  g.computeBoundingBox();
  const bb = g.boundingBox, ax = { x: 0, y: 1, z: 2 }[axis] ?? 1;
  const lo = bb.min.getComponent(ax), hi = bb.max.getComponent(ax), span = Math.max(hi - lo, 1e-6);
  const v = new THREE.Vector3();
  if (op === "smooth") {
    if (!g.index) throw new Error("lissage impossible sur ce maillage");
    const arr = p.array; taubin(arr, Array.from(g.index.array), Math.max(1, Math.round(amount * 3))); p.needsUpdate = true;
  } else if (op === "noise" || op === "inflate") {
    g.computeVertexNormals();
    const n = g.attributes.normal;
    for (let i = 0; i < p.count; i++) {
      const f = op === "inflate" ? amount * 0.05 : (fbm(p.getX(i) * 4 + 3, p.getY(i) * 4, p.getZ(i) * 4, 4) - 0.5) * amount * 0.1;
      p.setXYZ(i, p.getX(i) + n.getX(i) * f, p.getY(i) + n.getY(i) * f, p.getZ(i) + n.getZ(i) * f);
    }
  } else {
    for (let i = 0; i < p.count; i++) {
      v.fromBufferAttribute(p, i);
      const t = (v.getComponent(ax) - lo) / span;
      if (op === "taper") { const k = 1 + (amount - 1) * t; for (const c of [0, 1, 2]) if (c !== ax) v.setComponent(c, v.getComponent(c) * k); }
      else if (op === "twist") { const a = THREE.MathUtils.degToRad(amount) * t; const [c1, c2] = [0, 1, 2].filter((c) => c !== ax); const x = v.getComponent(c1), y = v.getComponent(c2); v.setComponent(c1, x * Math.cos(a) - y * Math.sin(a)); v.setComponent(c2, x * Math.sin(a) + y * Math.cos(a)); }
      else if (op === "bend") { const k = THREE.MathUtils.degToRad(amount) * t * t * span; const c = ax === 1 ? 2 : 1; v.setComponent(c, v.getComponent(c) + k); }
      else if (op === "stretch") v.setComponent(ax, lo + (v.getComponent(ax) - lo) * amount);
      else throw new Error("Opérations : smooth, noise, inflate, taper, twist, bend, stretch");
      p.setXYZ(i, v.x, v.y, v.z);
    }
  }
  p.needsUpdate = true;
  g.computeVertexNormals(); g.computeBoundingBox(); g.computeBoundingSphere();
}

export { applyPattern, doubleSided, curvePoints };
