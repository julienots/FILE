// Génère www/js/worlddata.js : provinces (Natural Earth admin-1, regroupées pour certains pays),
// voisinages, relief moyen, villes, nations. Natural Earth = domaine public ; world-countries = ODbL.
// Usage : NODE_PATH=<node_modules> node tools/build_world.js <dossier_geo>
const fs = require("fs"), path = require("path");
const wc = require("world-countries");
const STATS = require("./stats.js");
const GEO = process.argv[2];
const topo = JSON.parse(fs.readFileSync(path.join(GEO, "prov.topojson")));
const obj = Object.values(topo.objects)[0];
const [sx, sy] = topo.transform.scale, [tx, ty] = topo.transform.translate;

// --- arcs → unités carte (x=(lon+180)*10, y=(84-lat)*10), entiers ×10 (précision 0,01°)
const toMap = (lon, lat) => [Math.round((lon + 180) * 100), Math.round((84 - lat) * 100)];
const arcs = topo.arcs.map(a => { let x = 0, y = 0; return a.map(([dx, dy]) => { x += dx; y += dy; return toMap(x * sx + tx, y * sy + ty); }); });
// encodage delta
const arcsEnc = arcs.map(a => { const o = []; let px = 0, py = 0; for (const [x, y] of a) { o.push(x - px, y - py); px = x; py = y; } return o; });

const ALIAS = { SDS: "SSD", KOS: "XKX", SAH: "ESH", PSX: "PSE", SDN: "SDN" };
const iso = c => ALIAS[c] || c;
const byIso = {}; for (const c of wc) byIso[c.cca3] = c;
const NAMEFIX = { CYN: "Chypre du Nord", SOL: "Somaliland", KAS: "Cachemire", XKX: "Kosovo", TWN: "Taïwan", ESH: "Sahara occidental" };

// territoires rattachés à leur État souverain
const SOV = { GIB:"GBR",JEY:"GBR",GGY:"GBR",IMN:"GBR",BMU:"GBR",CYM:"GBR",VGB:"GBR",AIA:"GBR",MSR:"GBR",TCA:"GBR",PCN:"GBR",SHN:"GBR",IOT:"GBR",SGS:"GBR",FLK:"GBR",ESB:"GBR",WSB:"GBR",
  MAF:"FRA",SPM:"FRA",PYF:"FRA",WLF:"FRA",BLM:"FRA",NCL:"FRA",ATF:"FRA",CLP:"FRA", SXM:"NLD",CUW:"NLD",ABW:"NLD",
  GUM:"USA",MNP:"USA",ASM:"USA",VIR:"USA",UMI:"USA",PRI:"USA",USG:"USA", HKG:"CHN",MAC:"CHN",PGA:"CHN", FRO:"DNK",GRL:"DNK", ALD:"FIN",
  NFK:"AUS",HMD:"AUS",CSI:"AUS",ATC:"AUS",IOA:"AUS", COK:"NZL",NIU:"NZL", KAS:"IND", KAB:"KAZ" };
const geoms = obj.geometries.filter(g => g.type);
geoms.forEach(g => { g.properties.nat = SOV[g.properties.nat] || g.properties.nat; });
const provs = [];
const arcUse = new Map(); // arc -> [prov...]
geoms.forEach((g, pi) => {
  const polys = g.type === "Polygon" ? [g.arcs] : g.arcs;
  provs.push({ nat: g.properties.nat, name: g.properties.pname, polys });
  for (const p of polys) for (const ring of p) for (const a of ring) { const k = a < 0 ? ~a : a; if (!arcUse.has(k)) arcUse.set(k, new Set()); arcUse.get(k).add(pi); }
});
// rings en coordonnées
function ringPts(ring) { const pts = []; for (const a of ring) { const arc = a < 0 ? arcs[~a].slice().reverse() : arcs[a]; for (let i = pts.length ? 1 : 0; i < arc.length; i++) pts.push(arc[i]); } return pts; }
function area(pts) { let s = 0; for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) s += (pts[j][0] + pts[i][0]) * (pts[j][1] - pts[i][1]); return s / 2; }
function pip(x, y, pts) { let c = false; for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) { const [xi, yi] = pts[i], [xj, yj] = pts[j]; if ((yi > y) !== (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) c = !c; } return c; }
function centroid(pts) { let a = 0, cx = 0, cy = 0; for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) { const f = pts[j][0] * pts[i][1] - pts[i][0] * pts[j][1]; a += f; cx += (pts[j][0] + pts[i][0]) * f; cy += (pts[j][1] + pts[i][1]) * f; } a *= 3; return a ? [cx / a, cy / a] : pts[0]; }

// altitude (npy int16 3600x1410, 0,1°)
const npy = fs.readFileSync(path.join(__dirname, ".cache", "elev.npy"));
const hl = npy.readUInt16LE(8), off = 10 + hl;
const EW = 3600, EH = 1410;
const elevAt = (x, y) => { const i = Math.min(EH - 1, Math.max(0, Math.floor(y / 10))), j = Math.min(EW - 1, Math.max(0, Math.floor(x / 10))); return npy.readInt16LE(off + 2 * (i * EW + j)); };

for (const p of provs) {
  p.rings = p.polys.map(poly => poly.map(ringPts));
  let best = null, ba = -1, tot = 0, x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
  for (const poly of p.rings) { const a = Math.abs(area(poly[0])); tot += a; if (a > ba) { ba = a; best = poly[0]; } for (const [x, y] of poly[0]) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); } }
  p.area = tot; p.bbox = [x0, y0, x1, y1];
  let [cx, cy] = centroid(best);
  if (!pip(cx, cy, best)) { // centre hors du polygone : chercher un point intérieur
    let bx = best[0][0], by = best[0][1], bd = -1;
    const st = Math.max(5, (x1 - x0) / 25);
    for (let x = x0; x <= x1; x += st) for (let y = y0; y <= y1; y += st) if (pip(x, y, best)) { const d = -Math.hypot(x - cx, y - cy); if (bd === -1 || d > bd) { bd = d; bx = x; by = y; } }
    cx = bx; cy = by;
  }
  p.cx = cx; p.cy = cy;
  // relief : échantillonnage
  const step = Math.max(10, Math.sqrt((x1 - x0) * (y1 - y0) / 300));
  const vals = [];
  for (let x = x0 + step / 2; x < x1; x += step) for (let y = y0 + step / 2; y < y1; y += step) if (p.rings.some(r => pip(x, y, r[0]))) vals.push(Math.max(0, elevAt(x, y)));
  if (!vals.length) vals.push(Math.max(0, elevAt(cx, cy)));
  const m = vals.reduce((a, b) => a + b, 0) / vals.length;
  const sd = Math.sqrt(vals.reduce((a, b) => a + (b - m) * (b - m), 0) / vals.length);
  p.elev = Math.round(m); p.rough = Math.round(sd);
  p.terrain = (m > 1500 || sd > 450) ? 2 : (m > 500 || sd > 180) ? 1 : 0; // 0 plaine 1 collines 2 montagnes
}
// voisins et littoral
provs.forEach((p, i) => { p.nb = new Set(); p.coast = false; });
for (const [k, set] of arcUse) {
  const arr = [...set];
  if (arr.length === 1) { const len = arcs[k].length; if (len > 1) provs[arr[0]].coast = true; }
  for (const a of arr) for (const b of arr) if (a !== b) provs[a].nb.add(b);
}

// --- villes
const pp = JSON.parse(fs.readFileSync(path.join(GEO, "ne_10m_populated_places_simple.geojson"))).features
  .map(f => f.properties).filter(c => c.scalerank <= 8 || c.adm0cap);
const cities = [];
for (const c of pp) {
  const [x, y] = toMap(c.longitude, c.latitude);
  let pi = -1;
  for (let i = 0; i < provs.length; i++) { const b = provs[i].bbox; if (x < b[0] || x > b[2] || y < b[1] || y > b[3]) continue; if (provs[i].rings.some(r => pip(x, y, r[0]))) { pi = i; break; } }
  if (pi < 0) continue;
  cities.push({ n: c.name, x, y, pop: c.pop_max || 0, cap: c.adm0cap ? 1 : 0, r: c.scalerank, p: pi });
}
cities.sort((a, b) => b.pop - a.pop);

// --- nations
const natCodes = [...new Set(provs.map(p => p.nat))];
const nations = natCodes.map(code => {
  const c = byIso[iso(code)];
  const st = STATS[iso(code)] || STATS[code] || [0.3, 3];
  return { code: iso(code), a2: c ? c.cca2 : "", n: NAMEFIX[iso(code)] || (c ? c.translations.fra.common : code), pop: st[0], gdp: st[1], reg: c ? c.region : "", sub: c ? c.subregion : "" };
});
const natIdx = {}; natCodes.forEach((c, i) => natIdx[c] = i);
// répartition population/PIB par province
for (let n = 0; n < nations.length; n++) {
  const ps = provs.map((p, i) => i).filter(i => natIdx[provs[i].nat] === n);
  const A = ps.reduce((s, i) => s + provs[i].area, 0) || 1;
  const cp = {}; for (const c of cities) if (natIdx[provs[c.p].nat] === n) cp[c.p] = (cp[c.p] || 0) + Math.sqrt(Math.max(c.pop, 5000));
  const C = Object.values(cp).reduce((a, b) => a + b, 0) || 1;
  for (const i of ps) {
    const sa = provs[i].area / A, sc = (cp[i] || 0) / C;
    const wp = 0.15 * sa + 0.85 * sc, wg = 0.08 * sa + 0.92 * sc;
    provs[i].pop = +(nations[n].pop * (ps.length === 1 ? 1 : wp)).toFixed(3);
    provs[i].gdp = +(nations[n].gdp * (ps.length === 1 ? 1 : wg)).toFixed(3);
  }
  // normaliser si C=1 (pas de villes)
  const capCity = cities.find(c => c.cap && natIdx[provs[c.p].nat] === n) || cities.find(c => natIdx[provs[c.p].nat] === n);
  nations[n].capP = capCity ? capCity.p : ps.sort((a, b) => provs[b].area - provs[a].area)[0];
  nations[n].capN = capCity ? capCity.n : "";
}
// nom de province : nom de la plus grande ville si nom manquant
const ri = {}; nations.forEach((n,i)=>ri[i]=1);
const out = {
  W: 36000, H: 14100,
  nations,
  arcs: arcsEnc,
  provs: provs.map(p => ({ n: p.name, o: natIdx[p.nat], g: p.polys, cx: Math.round(p.cx), cy: Math.round(p.cy), pop: p.pop, gdp: p.gdp, el: p.elev, t: p.terrain, c: p.coast ? 1 : 0, nb: [...p.nb], a: Math.round(p.area / 1000) })),
  cities: cities.map(c => [c.n, c.x, c.y, Math.round(c.pop / 1000), c.cap, c.r, c.p]),
};
const js = "// Généré par tools/build_world.js — Natural Earth (domaine public), world-countries (ODbL)\nwindow.WORLD=" + JSON.stringify(out) + ";\n";
fs.writeFileSync(path.join(__dirname, "../www/js/worlddata.js"), js);
console.log("provinces", provs.length, "nations", nations.length, "villes", cities.length, "taille", (js.length / 1e6).toFixed(2) + " Mo");
console.log("sans stats:", nations.filter(n => !STATS[n.code]).map(n => n.code + ":" + n.n).join(", "));
