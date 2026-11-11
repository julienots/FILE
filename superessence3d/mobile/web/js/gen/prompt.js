// Analyse des descriptions FR/EN → spécification (portage fidèle de la version PC).
// Ce n'est pas un modèle de langage : c'est un analyseur à règles, dont le résultat est affiché et modifiable.
import { T } from "./tables.js";

const ACC = { à: "a", â: "a", ä: "a", á: "a", ã: "a", é: "e", è: "e", ê: "e", ë: "e", í: "i", ì: "i", î: "i", ï: "i", ó: "o", ò: "o", ô: "o", ö: "o", õ: "o", ú: "u", ù: "u", û: "u", ü: "u", ç: "c", ñ: "n", œ: "o" };
export const normalize = (s) => s.toLowerCase().replace(/[àâäáãéèêëíìîïóòôöõúùûüçñœ]/g, (c) => ACC[c]).trim();

export function tokens(text) {
  let t = normalize(text).replace(/(\d)\s*,\s*(\d)/g, "$1.$2").replace(/['’]/g, " ");
  return t.match(/[a-z0-9]+(?:[-.][a-z0-9]+)*/g) || [];
}

function num(tok) {
  if (/^\d+$/.test(tok)) return parseInt(tok, 10);
  if (tok === "six") return 6;
  return T.NUMBERS[tok] ?? null;
}

function base(tok) {
  for (const suf of ["ees", "es", "s", "e", "x"]) if (tok.endsWith(suf) && tok.length - suf.length >= 3) return tok.slice(0, -suf.length);
  return tok;
}
const match = (tok, words) => words.includes(tok) || words.includes(base(tok)) || words.includes(tok.replace(/-/g, ""));

export function colorOf(tok) {
  const N = T.NAMED;
  if (N[tok]) return N[tok];
  if (N[base(tok)]) return N[base(tok)];
  const t = tok.endsWith("s") ? tok.slice(0, -1) : tok;
  for (const c of [t, t.endsWith("e") ? t.slice(0, -1) : t, t.endsWith("tte") ? t.slice(0, -2) : t, t.endsWith("lle") ? t.slice(0, -2) : t, t.endsWith("che") ? t.slice(0, -3) + "c" : t]) if (N[c]) return N[c];
  if (["dore", "doree", "dores", "dorees"].includes(tok)) return N.dore;
  if (["argente", "argentee", "argentes", "argentees"].includes(tok)) return N.argente;
  return null;
}

function shade(words, i, c) {
  const nxt = words[i + 1] || "", prv = words[i - 1] || "";
  let [r, g, b] = c;
  if (["fonce", "foncee", "fonces", "foncees", "sombre", "sombres"].includes(nxt) || ["dark", "deep"].includes(prv)) return [r * 0.45, g * 0.45, b * 0.45];
  if (["clair", "claire", "clairs", "claires", "pale", "pales"].includes(nxt) || ["light", "pale", "bright"].includes(prv)) return [r + (1 - r) * 0.45, g + (1 - g) * 0.45, b + (1 - b) * 0.45];
  return [r, g, b];
}

function findCategory(words) {
  for (let i = 0; i < words.length; i++) {
    const w = words[i];
    for (const [cat, subs] of Object.entries(T.CATEGORIES)) for (const [sub, keys] of Object.entries(subs)) {
      if (match(w, keys) || keys.includes(w.replace(/-/g, "_"))) return [cat, sub, w];
    }
  }
  return ["creature", "beast", null];
}

const BOOL_FEATS = ["spikes", "scales", "claws", "fangs", "fur", "feathers", "shell", "mouth"];

export function parsePrompt(text) {
  const words = tokens(text);
  const [category, subtype, keyword] = findCategory(words);
  const spec = { prompt: text, category, subtype, keyword, category_label: T.CATEGORY_LABELS[category], features: {}, colors: {}, scales: {}, notes: [] };
  const f = spec.features;
  const m = normalize(text).replace(/,/g, ".").match(/(\d+(?:\.\d+)?)\s*(m|metres?|meters?|cm|centimetres?|mm)\b/);
  if (m) {
    const v = parseFloat(m[1]);
    const k = m[2].startsWith("c") ? 0.01 : m[2] === "mm" ? 0.001 : 1;
    spec.height = Math.min(Math.max(v * k, 0.01), 500);
    spec.notes.push(`taille : ${spec.height} m`);
  }
  if (words.some((w) => match(w, T.BIG_WORDS))) spec.size_hint = "big";
  else if (words.some((w) => match(w, T.SMALL_WORDS))) spec.size_hint = "small";
  if (words.some((w) => match(w, T.HEAVY_WORDS))) spec.build = "heavy";
  else if (words.some((w) => match(w, T.THIN_WORDS))) spec.build = "thin";
  words.forEach((w, i) => {
    for (const [feat, keys] of Object.entries(T.FEATURES)) {
      if (!match(w, keys)) continue;
      let n = null;
      for (const back of [1, 2]) if (i - back >= 0) { n = num(words[i - back]); if (n !== null) break; }
      const plural = w.endsWith("s") || w.endsWith("x") || w === "bras" || w === "yeux";
      if (n === null) {
        n = { wings: 2, horns: 2, eyes: 2, legs: 4, arms: 2, tentacles: 6, heads: 1, tails: 1, fins: 3, antennae: 2 }[feat] ?? 1;
        if ((feat === "heads" || feat === "tails") && plural) n = 2;
      }
      f[feat] = BOOL_FEATS.includes(feat) ? true : Math.min(n, 64);
    }
  });
  words.forEach((w, i) => {
    if (["sans", "without", "no", "aucun", "aucune"].includes(w)) for (const j of [i + 1, i + 2]) if (words[j]) for (const [feat, keys] of Object.entries(T.FEATURES)) if (match(words[j], keys)) f[feat] = BOOL_FEATS.includes(feat) ? false : 0;
  });
  const targets = { eyes: T.FEATURES.eyes, veins: ["veine", "veines", "vein", "veins"], horns: T.FEATURES.horns, wings: T.FEATURES.wings };
  const body = [];
  words.forEach((w, i) => {
    let c = colorOf(w);
    if (!c || w === "or" || w === "a" || w === "an") {
      if (w === "or" && i > 0 && ["en", "d", "de", "dore"].includes(words[i - 1])) c = T.NAMED.or; else return;
    }
    c = shade(words, i, c);
    let done = false;
    let prevNoun = null;
    for (let j = i - 1; j >= Math.max(0, i - 3); j--) if (Object.values(targets).some((v) => match(words[j], v))) { prevNoun = words[j]; break; }
    for (const [name, keys] of Object.entries(targets)) {
      if ((prevNoun && match(prevNoun, keys)) || (words[i + 1] && match(words[i + 1], keys))) { spec.colors[name] = c; done = true; break; }
    }
    if (!done) body.push(c);
  });
  if (body[0]) spec.colors.primary = body[0];
  if (body[1]) spec.colors.secondary = body[1];
  words.forEach((w, i) => {
    if (match(w, T.GLOW_WORDS)) {
      if (words.slice(Math.max(0, i - 4), i + 2).some((x) => match(x, T.FEATURES.eyes))) spec.glow_eyes = true; else spec.glow = true;
    }
  });
  for (const [style, keys] of Object.entries(T.STYLE_WORDS)) if (words.some((w) => match(w, keys))) { spec.style = style; break; }
  if (spec.style === "realistic") spec.detail = "high";
  for (const [preset, keys] of T.MATERIAL_WORDS) if (words.some((w) => match(w, keys))) { spec.material = preset; break; }
  if (spec.material === "scales") f.scales = true;
  if (words.some((w) => match(w, T.WET_WORDS))) spec.wet = true;
  if (category === "creature" && subtype === "dragon") {
    f.wings ??= 2; f.horns ??= 2; f.tails ??= 1; f.legs ??= 4; f.scales ??= true;
  }
  return spec;
}

export function parseModification(text, spec) {
  const s = JSON.parse(JSON.stringify(spec));
  const f = (s.features ||= {}), sc = (s.scales ||= {}), col = (s.colors ||= {});
  const changes = [];
  const all = tokens(text);
  let big = 1.4;
  if ((all.includes("deux") && (all.includes("fois") || all.includes("times"))) || all.includes("twice")) big = 2;
  if (["beaucoup", "much", "tres"].some((w) => all.includes(w))) big = Math.max(big, 1.7);
  const clauses = normalize(text).split(/[,;]| et | and | puis | then /).map(tokens).filter((c) => c.length);
  for (const cl of clauses) {
    let verb = cl.find((w) => match(w, [...T.BIGGER, ...T.SMALLER, ...T.ADD, ...T.REMOVE, ...T.REPLACE])) || null;
    const more = cl.some((w) => w === "plus" || w === "more");
    if (more && cl.some((w) => match(w, ["grand", "grande", "long", "longue", "gros", "grosse", "big", "large"]))) verb = "agrandis";
    if (more && cl.some((w) => match(w, ["petit", "petite", "court", "courte", "small", "short"]))) verb = "reduis";
    const tg = Object.entries(T.SCALE_TARGETS).filter(([, keys]) => cl.some((w) => match(w, keys))).map(([t]) => t);
    const n = cl.map((w) => (["a", "an", "un", "une"].includes(w) ? null : num(w))).find((x) => x !== null) ?? null;
    if (!verb && cl.some((w) => colorOf(w))) verb = "remplace";
    if (!verb) continue;
    if (match(verb, T.BIGGER) || verb === "agrandis") {
      for (const t of tg.length ? tg : ["body"]) { sc[t] = +((sc[t] || 1) * big).toFixed(3); changes.push(`${t} ×${big}`); }
      if (!tg.length) s.height = +((s.height || 2) * big).toFixed(3);
    } else if (match(verb, T.SMALLER) || verb === "reduis") {
      for (const t of tg.length ? tg : ["body"]) { sc[t] = +((sc[t] || 1) / 1.4).toFixed(3); changes.push(`${t} ÷1.4`); }
      if (!tg.length) s.height = +((s.height || 2) / 1.4).toFixed(3);
    } else if (match(verb, T.ADD)) {
      for (const t of tg) {
        if (t === "spikes" || t === "claws") { f[t] = true; changes.push(`+ ${t}`); }
        else if (T.FEATURES[t]) { const k = n ?? ({ wings: 2, horns: 2, eyes: 2, legs: 2, arms: 2, tentacles: 4 }[t] || 1); f[t] = (+f[t] || 0) + k; changes.push(`+${k} ${t}`); }
      }
      for (const ft of ["scales", "fur", "feathers", "shell", "fangs"]) if (cl.some((w) => match(w, T.FEATURES[ft]))) { f[ft] = true; changes.push(`+ ${ft}`); }
    } else if (match(verb, T.REMOVE)) {
      for (const t of tg) { f[t] = t === "spikes" || t === "claws" ? false : 0; changes.push(`− ${t}`); }
      for (const ft of ["scales", "fur", "feathers", "shell", "fangs"]) if (cl.some((w) => match(w, T.FEATURES[ft]))) { f[ft] = false; changes.push(`− ${ft}`); }
    }
    const cols = cl.map((w, i) => [i, colorOf(w)]).filter(([i, c]) => c && !["or", "a"].includes(cl[i]));
    const glow = cl.some((w) => match(w, T.GLOW_WORDS));
    if (cols.length || glow) {
      const target = tg.includes("eyes") ? "eyes" : tg.includes("horns") ? "horns" : tg.includes("wings") ? "wings" : cl.some((w) => w.startsWith("vein")) ? "veins" : "primary";
      if (cols.length) { const [i, c] = cols[cols.length - 1]; col[target] = shade(cl, i, c); changes.push(`couleur ${target} : ${cl[i]}`); }
      if (glow) { if (target === "eyes") s.glow_eyes = true; else s.glow = true; changes.push(`${target} lumineux`); }
    }
    if (n !== null && match(verb, T.REPLACE)) for (const t of tg) if (T.FEATURES[t] && t !== "spikes" && t !== "claws") { f[t] = n; changes.push(`${t} = ${n}`); }
  }
  const hm = normalize(text).replace(/,/g, ".").match(/(\d+(?:\.\d+)?)\s*(m|metres?|meters?)\b/);
  if (hm) { s.height = parseFloat(hm[1]); changes.push(`taille ${s.height} m`); }
  return [s, changes];
}

export function parseMaterialText(text) {
  const words = tokens(text);
  let preset = null;
  for (const [p, keys] of T.MATERIAL_WORDS) if (words.some((w) => match(w, keys))) { preset = p; break; }
  if ((preset === "skin" || !preset) && words.some((w) => match(w, ["veine", "veines", "organique", "organiques", "organic", "visqueux", "veins"]))) preset = "organic";
  preset ||= "plain";
  const p = {};
  const colors = [];
  words.forEach((w, i) => {
    let c = colorOf(w);
    if (!c || ["or", "a", "an"].includes(w)) return;
    c = shade(words, i, c);
    if (words.slice(Math.max(0, i - 3), i).some((x) => x.startsWith("vein"))) p.veins = c; else colors.push(c);
  });
  if (colors[0]) p.color = colors[0];
  if (colors[1]) p.color2 = colors[1];
  if (words.some((w) => match(w, T.WET_WORDS))) p.wet = 0.7;
  if (words.some((w) => match(w, T.ROUGH_WORDS))) p.roughness = 0.85;
  else if (words.some((w) => match(w, T.SMOOTH_WORDS))) p.roughness = 0.15;
  if (words.some((w) => match(w, [...T.BUMP_WORDS, "organiques", "organique"]))) p.bump = 1.8;
  if (words.some((w) => match(w, T.GLOW_WORDS))) { p.emission = colors[0] || [0.2, 0.8, 1]; p.emission_strength = 2; }
  return { preset, params: p };
}
