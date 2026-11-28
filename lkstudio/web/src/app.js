// LK Studio — the mod maker of LOST KINGDOM. Everything runs on the phone: no account, no server.
import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { MeshoptDecoder } from 'three/examples/jsm/libs/meshopt_decoder.module.js';
import GAME from './gamedata.json';
import { extractMesh, simplify, orient, rigHumanoid, rigQuadruped, rigProp, writeLKM, canvasToJpeg } from './lkm.js';
import { Viewer } from './viewer.js';
import { makeZip, readZip, toBase64 } from './zip.js';
import { putFile, getFile, delFile, loadProjects, saveProjects, newProject } from './store.js';

// ------------------------------------------------------------------------------------------ data from the game
const RARITIES = [['commun', 'Commun', '#c8c4bc'], ['rare', 'Rare', '#5aa0ff'], ['epique', 'Épique', '#b070ff'], ['legendaire', 'Légendaire', '#ffa030'], ['mythique', 'Mythique', '#ff5070'], ['ancestral', 'Ancestral', '#ffe070']];
const KIND_NAMES = ['Arme', 'Bouclier', 'Tête', 'Torse', 'Mains', 'Pieds', 'Anneau', 'Amulette', 'Consommable', 'Matériau', 'Quête', 'Outil', 'Monture', 'Relique', 'Cosmétique', 'Objet clé'];
const ARCH = ['Humanoïde', 'Quadrupède', 'Araignée', 'Dragon'];
const ENEMIES = GAME.enemies.filter(e => !e.boss);
const BASE_ITEMS = GAME.items.filter(i => i.kind <= 8 && i.id !== 'star_shard');
const REGIONS = GAME.regions;
const BUILDINGS = [['house', 'Maison'], ['house_2', 'Maison (variante 2)'], ['house_3', 'Maison (variante 3)'], ['inn', 'Auberge'], ['stonehouse', 'Maison de pierre'], ['longhouse', 'Longue maison'],
  ['forge', 'Forge'], ['chapel', 'Chapelle'], ['tower', 'Tour'], ['well', 'Puits'], ['stall', 'Étal'], ['tent', 'Tente']];
const PEOPLE = [['hero', 'Le héros (Aldric)'], ['hero_ninja', 'Kaede, la ninja'], ['hero_flyer', 'Astra, l\'étoile'], ['villager_m', 'Villageois'], ['villager_m2', 'Villageois (2)'], ['villager_f', 'Villageoise'], ['villager_f3', 'Villageoise (2)']];
const SOUNDS = [['audio/music/title.ogg', 'Musique : écran titre'], ['audio/music/greenhaven.ogg', 'Musique : Greenhaven'], ['audio/music/frostheim.ogg', 'Musique : Frostheim'], ['audio/music/ashen.ogg', 'Musique : Ashen Lands'],
  ['audio/music/elderwood.ogg', 'Musique : Elderwood'], ['audio/music/shadows.ogg', 'Musique : Royaume des Ombres'], ['audio/music/highcrowns.ogg', 'Musique : High Crowns'], ['audio/music/village.ogg', 'Musique : villages'],
  ['audio/music/combat.ogg', 'Musique : combat'], ['audio/music/boss.ogg', 'Musique : boss'], ['audio/music/dungeon.ogg', 'Musique : donjons'], ['audio/music/night.ogg', 'Musique : nuit'],
  ['audio/sfx/swing.ogg', 'Son : coup d\'épée'], ['audio/sfx/hit_flesh.ogg', 'Son : impact'], ['audio/sfx/jump.ogg', 'Son : saut'], ['audio/sfx/levelup.ogg', 'Son : niveau'], ['audio/sfx/coin.ogg', 'Son : pièces'],
  ['audio/sfx/roar.ogg', 'Son : rugissement'], ['audio/sfx/wolf_howl.ogg', 'Son : hurlement'], ['audio/sfx/fireball.ogg', 'Son : boule de feu'], ['audio/sfx/discover.ogg', 'Son : découverte']];
const WORLD = [
  ['Combat', [['game.enemy_hp', 'Vie des ennemis', 0.2, 10, 1, 0.1, 'x'], ['game.enemy_atk', 'Force des ennemis', 0.2, 10, 1, 0.1, 'x'], ['game.enemy_scale', 'Taille des ennemis', 0.4, 3, 1, 0.05, 'x']]],
  ['Récompenses', [['game.xp', 'Expérience gagnée', 0.1, 10, 1, 0.1, 'x'], ['game.gold', 'Or gagné', 0.1, 10, 1, 0.1, 'x'], ['game.start_gold', 'Or au départ', 0, 100000, 60, 10, ' or']]],
  ['Héros', [['game.player_speed', 'Vitesse du héros', 0.5, 2.5, 1, 0.05, 'x'], ['game.jump', 'Hauteur de saut', 0.5, 3, 1, 0.05, 'x'], ['game.stamina_regen', 'Récupération d\'endurance', 0.2, 5, 1, 0.1, 'x'], ['game.mount_speed', 'Vitesse des montures', 0.5, 3, 1, 0.05, 'x']]],
  ['Monde', [['game.gravity', 'Gravité', 0.15, 3, 1, 0.05, 'x'], ['game.time_speed', 'Vitesse du temps (0 = figé)', 0, 20, 1, 0.5, 'x'], ['game.hour', 'Heure figée (-1 = normale)', -1, 23.99, -1, 0.25, ' h']]],
  ['Bombes et pelle', [['bomb.radius', 'Rayon des bombes', 1, 12, 3, 0.5, ' m'], ['bomb.damage', 'Dégâts des bombes', 0, 2000, 120, 10, ''], ['dig.radius', 'Rayon de la pelle', 0.6, 4, 1.2, 0.1, ' m'], ['dig.depth', 'Profondeur par coup', 0.1, 3, 0.5, 0.1, ' m']]],
];
const WEATHERS = ['normal', 'clair', 'nuageux', 'pluie', 'orage', 'neige', 'brouillard', 'cendres', 'sable'];

// ------------------------------------------------------------------------------------------ tiny DOM helpers
const $ = (s, r = document) => r.querySelector(s);
function h(tag, attrs = {}, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (k === 'class') e.className = v; else if (k === 'style') e.style.cssText = v; else if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
    else if (k === 'html') e.innerHTML = v; else if (v !== false && v != null) e.setAttribute(k, v === true ? '' : v);
  }
  for (const k of kids.flat()) if (k != null && k !== false) e.append(k.nodeType ? k : document.createTextNode(k));
  return e;
}
const slug = s => (s || '').toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '').slice(0, 32) || 'sans_nom';
function toast(msg, kind = '') { const t = h('div', { class: 'toast ' + kind }, msg); document.body.append(t); setTimeout(() => t.classList.add('show'), 10); setTimeout(() => { t.classList.remove('show'); setTimeout(() => t.remove(), 400); }, 2600); }
function field(label, input, hint) { return h('label', { class: 'field' }, h('span', { class: 'lbl' }, label), input, hint ? h('small', {}, hint) : null); }
function text(val, on, ph = '') { const i = h('input', { type: 'text', value: val || '', placeholder: ph }); i.addEventListener('input', () => on(i.value)); return i; }
function area(val, on, ph = '', rows = 4) { const i = h('textarea', { rows, placeholder: ph }); i.value = val || ''; i.addEventListener('input', () => on(i.value)); return i; }
function select(opts, val, on) { const s = h('select', {}, ...opts.map(([v, l]) => h('option', { value: v, selected: String(v) === String(val) }, l))); s.addEventListener('change', () => on(s.value)); return s; }
function slider(min, max, step, val, on, unit = '') {
  const out = h('b', {}, fmt(val) + unit), r = h('input', { type: 'range', min, max, step, value: val });
  r.addEventListener('input', () => { out.textContent = fmt(+r.value) + unit; on(+r.value); });
  return h('div', { class: 'slider' }, r, out);
}
const fmt = v => (Math.abs(v) >= 100 ? Math.round(v) : Math.round(v * 100) / 100).toString().replace('.', ',');
function toggle(val, on, label) { const c = h('input', { type: 'checkbox', checked: !!val }); c.addEventListener('change', () => on(c.checked)); return h('label', { class: 'toggle' }, c, h('i'), label); }
function color(val, on) { const c = h('input', { type: 'color', value: val || '#ffffff' }); c.addEventListener('input', () => on(c.value)); return c; }
function searchSelect(list, val, on, labelOf, placeholder = 'Rechercher…') {
  const wrap = h('div', { class: 'ssel' }), inp = h('input', { type: 'search', placeholder }), box = h('div', { class: 'ssel-list' });
  const cur = h('div', { class: 'ssel-cur' });
  const show = () => { const it = list.find(x => x.id === val); cur.innerHTML = ''; cur.append(it ? labelOf(it) : h('span', { class: 'dim' }, 'Aucun')); };
  const render = () => { box.innerHTML = ''; const q = inp.value.toLowerCase(); list.filter(x => !q || (x.name + ' ' + x.id + ' ' + (x.family || '')).toLowerCase().includes(q)).slice(0, 60).forEach(x => box.append(h('div', { class: 'ssel-opt' + (x.id === val ? ' sel' : ''), onclick: () => { val = x.id; on(x.id); show(); wrap.classList.remove('open'); } }, labelOf(x)))); };
  cur.addEventListener('click', () => { wrap.classList.toggle('open'); render(); inp.focus(); });
  inp.addEventListener('input', render);
  wrap.append(cur, h('div', { class: 'ssel-pop' }, inp, box)); show();
  return wrap;
}
const enemyLabel = e => h('span', {}, h('b', {}, e.name), h('small', {}, ` · ${e.family} · ${REGIONS[e.region] ? REGIONS[e.region].name : ''} · niv. ${e.level} · ${ARCH[e.arch]}`));
const itemLabel = i => h('span', {}, h('b', { style: `color:${RARITIES[i.rarity] ? RARITIES[i.rarity][2] : '#ccc'}` }, i.name), h('small', {}, ` · ${KIND_NAMES[i.kind] || ''}${i.atk ? ' · atq ' + i.atk : ''}${i.def ? ' · déf ' + i.def : ''}`));

// ------------------------------------------------------------------------------------------ state
let projects = loadProjects();
let P = null;            // the open project
let tab = 'home';
let viewer = null;
const save = () => { if (!P) return; const i = projects.findIndex(p => p.id === P.id); if (i >= 0) projects[i] = P; else projects.unshift(P); saveProjects(projects); };
let saveT = 0; const saveSoon = () => { clearTimeout(saveT); saveT = setTimeout(save, 400); };
window.onAndroidBack = () => { if ($('.modal')) { $('.modal').remove(); return true; } if (P && tab !== 'project') { go('project'); return true; } if (P) { P = null; go('home'); return true; } return false; };

function go(t) { tab = t; render(); window.scrollTo(0, 0); }

// ------------------------------------------------------------------------------------------ layout
function render() {
  const app = $('#app'); app.innerHTML = '';
  const top = h('header', {}, h('div', { class: 'logo', onclick: () => { P = null; go('home'); } }, h('span', { class: 'gem' }), h('div', {}, h('b', {}, 'LK Studio'), h('small', {}, 'Créateur de mods · LOST KINGDOM'))),
    P ? h('div', { class: 'proj', style: `--c:${P.color}` }, P.name) : null);
  app.append(top);
  const main = h('main', { class: 'tab-' + tab }); app.append(main);
  if (!P) { home(main); return; }
  const pages = { project: pageProject, creatures: pageCreatures, items: pageItems, world: pageWorld, models: pageModels, story: pageStory, export: pageExport, edits: pageEdits, sounds: pageSounds };
  (pages[tab] || pageProject)(main);
  const nav = h('nav', {}, ...[['project', '◆', 'Projet'], ['creatures', '🐺', 'Créatures'], ['items', '⚔', 'Objets'], ['models', '▲', '3D'], ['world', '☀', 'Monde'], ['export', '⬇', 'Exporter']]
    .map(([t, ic, l]) => h('button', { class: tab === t ? 'on' : '', onclick: () => go(t) }, h('i', {}, ic), h('span', {}, l))));
  app.append(nav);
}

function home(main) {
  main.append(h('section', { class: 'hero' },
    h('h1', {}, 'Crée ton mod'), h('p', {}, 'Nouvelles créatures, armes légendaires, règles du monde, modèles 3D animés, musiques… Puis un seul bouton : le mod part dans LOST KINGDOM.'),
    h('div', { class: 'tpl' }, ...[['vide', '✦', 'Mod vide', 'Partir de zéro'], ['creatures', '🐺', 'Bestiaire', 'Une créature d\'exemple'], ['armes', '⚔', 'Arsenal', 'Une arme d\'exemple'], ['monde', '☀', 'Règles du monde', 'Gravité, météo, temps…']]
      .map(([t, ic, n, d]) => h('button', { class: 'card tplc', onclick: () => { P = newProject(t); applyTemplate(P, t); save(); go('project'); } }, h('i', {}, ic), h('b', {}, n), h('small', {}, d)))),
    h('label', { class: 'btn ghost wide' }, 'Ouvrir un mod existant (.zip)', h('input', { type: 'file', accept: '.zip,application/zip', style: 'display:none', onchange: e => importZip(e.target.files[0]) }))));
  if (projects.length) {
    main.append(h('h2', {}, 'Mes mods'));
    main.append(h('div', { class: 'list' }, ...projects.map(p => h('div', { class: 'card proj-card', style: `--c:${p.color}`, onclick: () => { P = p; go('project'); } },
      h('div', { class: 'swatch' }), h('div', { class: 'grow' }, h('b', {}, p.name), h('small', {}, `${p.creatures.length} créature(s) · ${p.items.length} objet(s) · ${p.models.length} modèle(s) 3D`)),
      h('button', { class: 'icon danger', onclick: e => { e.stopPropagation(); if (confirm('Supprimer « ' + p.name + ' » ?')) { projects = projects.filter(x => x.id !== p.id); saveProjects(projects); render(); } } }, '✕')))));
  }
  main.append(h('p', { class: 'foot' }, 'LK Studio 1.0 · Superessence Studio · les mods restent sur ton téléphone.'));
}

function applyTemplate(p, t) {
  if (t === 'creatures') { p.name = 'Mon bestiaire'; p.creatures.push(newCreature('wolf', 'Loup d\'émeraude')); p.creatures[0].tint = '#40e090'; }
  if (t === 'armes') { p.name = 'Mon arsenal'; p.items.push(newItem('sword_knight', 'Lame des étoiles')); }
  if (t === 'monde') { p.name = 'Mon monde'; p.world = { 'game.gravity': 0.6, 'game.jump': 1.6, 'game.weather': 'neige' }; }
}
function newCreature(base, name) {
  const b = ENEMIES.find(e => e.id === base) || ENEMIES[0];
  return { id: slug(name), base: b.id, name, desc: '', region: REGIONS[b.region].id, tint: '#ffffff', scale: Math.max(0.5, b.scale), hp: b.hp, atk: b.atk, level: b.level, spawn: 1, night: false, loot: [], shards: 0.1 };
}
function newItem(base, name) {
  const b = BASE_ITEMS.find(i => i.id === base) || BASE_ITEMS[0];
  return { id: slug(name), base: b.id, name, desc: '', rarity: Math.max(1, b.rarity), atk: b.atk, def: b.def, hp: b.hp, crit: 0.08, elem: 0, color: '#c9a45c', dropFrom: '', dropChance: 0.1, start: false };
}

// ------------------------------------------------------------------------------------------ pages
function pageProject(main) {
  main.append(h('h2', {}, 'Le projet'));
  main.append(h('div', { class: 'card form' },
    field('Nom du mod', text(P.name, v => { P.name = v; saveSoon(); $('.proj') && ($('.proj').textContent = v); })),
    field('Auteur', text(P.author, v => { P.author = v; saveSoon(); })),
    h('div', { class: 'row' }, field('Version', text(P.version, v => { P.version = v; saveSoon(); })), field('Couleur', color(P.color, v => { P.color = v; saveSoon(); }))),
    field('Description', area(P.description, v => { P.description = v; saveSoon(); }, 'Ce que fait le mod, en une ou deux phrases.'))));
  const tiles = [['creatures', '🐺', 'Créatures', `${P.creatures.length} nouvelle(s)`], ['items', '⚔', 'Objets', `${P.items.length} nouveau(x)`], ['models', '▲', 'Modèles 3D', `${P.models.length} importé(s)`],
    ['world', '☀', 'Règles du monde', `${Object.keys(P.world).length} réglage(s)`], ['edits', '✎', 'Modifier le jeu', `${P.edits.length} changement(s)`], ['story', '📜', 'Chroniques et PNJ', `${P.lore.length + P.npcs.length} texte(s)`],
    ['sounds', '♪', 'Musiques et sons', `${P.sounds.length} remplacé(s)`], ['export', '⬇', 'Exporter', 'vers LOST KINGDOM']];
  main.append(h('div', { class: 'tiles' }, ...tiles.map(([t, ic, n, d]) => h('button', { class: 'card tile', onclick: () => go(t) }, h('i', {}, ic), h('b', {}, n), h('small', {}, d)))));
}

function pageCreatures(main) {
  main.append(h('div', { class: 'head' }, h('h2', {}, 'Créatures'), h('button', { class: 'btn', onclick: () => { P.creatures.push(newCreature('wolf', 'Nouvelle créature ' + (P.creatures.length + 1))); save(); render(); } }, '+ Créature')));
  main.append(h('p', { class: 'dim' }, 'Chaque créature part d\'une créature du jeu : elle garde son corps, ses attaques et son intelligence. Change son nom, sa couleur, sa taille, sa région, sa force… et donne-lui ton propre modèle 3D.'));
  if (!P.creatures.length) main.append(h('div', { class: 'empty' }, 'Aucune créature. Touche « + Créature ».'));
  P.creatures.forEach((c, idx) => {
    const base = ENEMIES.find(e => e.id === c.base) || ENEMIES[0];
    const model = P.models.find(m => m.target === 'enemy:' + c.id);
    const card = h('details', { class: 'card item', open: idx === P.creatures.length - 1 },
      h('summary', {}, h('span', { class: 'dot', style: `background:${c.tint}` }), h('b', {}, c.name || 'Sans nom'), h('small', {}, ` ${base.name} · ${(REGIONS.find(r => r.id === c.region) || {}).name || ''}`), model ? h('span', { class: 'tag' }, '3D') : null),
      h('div', { class: 'form' },
        field('Nom', text(c.name, v => { c.name = v; saveSoon(); })),
        field('Identifiant', text(c.id, v => { c.id = slug(v); saveSoon(); }), 'Lettres minuscules, chiffres et _ : c\'est aussi le nom du modèle 3D.'),
        field('Créature de base', searchSelect(ENEMIES, c.base, v => { const b = ENEMIES.find(e => e.id === v); c.base = v; c.hp = b.hp; c.atk = b.atk; c.level = b.level; c.region = REGIONS[b.region].id; save(); render(); }, enemyLabel)),
        field('Description', area(c.desc, v => { c.desc = v; saveSoon(); }, 'Ce qu\'on lit dans le bestiaire.', 2)),
        h('div', { class: 'row' }, field('Région', select(REGIONS.map(r => [r.id, r.name]), c.region, v => { c.region = v; saveSoon(); })), field('Couleur', color(c.tint, v => { c.tint = v; saveSoon(); card.querySelector('.dot').style.background = v; }))),
        field('Taille', slider(0.3, 6, 0.05, c.scale, v => { c.scale = v; saveSoon(); }, ' ×')),
        field('Vie', slider(1, 20000, 1, c.hp, v => { c.hp = v; saveSoon(); })),
        field('Attaque', slider(0, 500, 1, c.atk, v => { c.atk = v; saveSoon(); })),
        field('Niveau', slider(1, 60, 1, c.level, v => { c.level = v; saveSoon(); })),
        field('Fréquence dans sa région', slider(0, 10, 0.1, c.spawn, v => { c.spawn = v; saveSoon(); }, ' ×'), '1 = aussi courante que les créatures du jeu, 0 = n\'apparaît jamais seule.'),
        toggle(c.night, v => { c.night = v; saveSoon(); }, 'Surtout la nuit'),
        field('Butin', lootEditor(c)),
        h('div', { class: 'row actions' },
          h('button', { class: 'btn ghost', onclick: () => { tab = 'models'; modelTarget = 'enemy:' + c.id; render(); } }, model ? 'Changer le modèle 3D' : 'Donner un modèle 3D'),
          h('button', { class: 'btn ghost', onclick: () => { const d = JSON.parse(JSON.stringify(c)); d.id += '_2'; d.name += ' (copie)'; P.creatures.push(d); save(); render(); } }, 'Dupliquer'),
          h('button', { class: 'btn danger', onclick: () => { P.creatures.splice(idx, 1); save(); render(); } }, 'Supprimer'))));
    main.append(card);
  });
}

function lootEditor(c) {
  const wrap = h('div', { class: 'loot' });
  const draw = () => {
    wrap.innerHTML = '';
    wrap.append(h('div', { class: 'row' }, h('span', {}, 'Éclats d\'étoile'), slider(0, 1, 0.01, c.shards || 0, v => { c.shards = v; saveSoon(); }, '')));
    c.loot.forEach((l, i) => wrap.append(h('div', { class: 'loot-row' },
      searchSelect(allItemChoices(), l.item, v => { l.item = v; saveSoon(); }, itemLabel), slider(0.01, 1, 0.01, l.chance, v => { l.chance = v; saveSoon(); }, ''),
      h('button', { class: 'icon danger', onclick: () => { c.loot.splice(i, 1); saveSoon(); draw(); } }, '✕'))));
    wrap.append(h('button', { class: 'btn small ghost', onclick: () => { c.loot.push({ item: 'potion_minor', chance: 0.2 }); saveSoon(); draw(); } }, '+ Objet lâché'));
  };
  draw(); return wrap;
}
function allItemChoices() { return [...P.items.map(i => ({ id: i.id, name: i.name + ' (mod)', kind: (BASE_ITEMS.find(b => b.id === i.base) || {}).kind || 0, rarity: i.rarity, atk: i.atk, def: i.def })), ...GAME.items.filter(i => i.kind !== 15)]; }

function pageItems(main) {
  main.append(h('div', { class: 'head' }, h('h2', {}, 'Objets'), h('button', { class: 'btn', onclick: () => { P.items.push(newItem('sword_iron', 'Nouvel objet ' + (P.items.length + 1))); save(); render(); } }, '+ Objet')));
  main.append(h('p', { class: 'dim' }, 'Armes, armures, anneaux, potions… Chaque objet part d\'un objet du jeu (son apparence et son type) : choisis son nom, sa rareté, sa puissance, qui le lâche, et s\'il est offert au départ.'));
  if (!P.items.length) main.append(h('div', { class: 'empty' }, 'Aucun objet. Touche « + Objet ».'));
  P.items.forEach((it, idx) => {
    const base = BASE_ITEMS.find(b => b.id === it.base) || BASE_ITEMS[0];
    const card = h('details', { class: 'card item', open: idx === P.items.length - 1 },
      h('summary', {}, h('span', { class: 'dot', style: `background:${RARITIES[it.rarity][2]}` }), h('b', { style: `color:${RARITIES[it.rarity][2]}` }, it.name || 'Sans nom'), h('small', {}, ` ${KIND_NAMES[base.kind]} · ${RARITIES[it.rarity][1]}`)),
      h('div', { class: 'form' },
        field('Nom', text(it.name, v => { it.name = v; saveSoon(); })),
        field('Identifiant', text(it.id, v => { it.id = slug(v); saveSoon(); })),
        field('Objet de base', searchSelect(BASE_ITEMS, it.base, v => { const b = BASE_ITEMS.find(x => x.id === v); it.base = v; it.atk = b.atk; it.def = b.def; it.hp = b.hp; save(); render(); }, itemLabel)),
        field('Description', area(it.desc, v => { it.desc = v; saveSoon(); }, '', 2)),
        field('Rareté', h('div', { class: 'chips' }, ...RARITIES.map(([k, l, col], ri) => h('button', { class: 'chip' + (it.rarity === ri ? ' on' : ''), style: `--c:${col}`, onclick: () => { it.rarity = ri; save(); render(); } }, l)))),
        base.kind === 0 ? field('Attaque', slider(0, 300, 1, it.atk, v => { it.atk = v; saveSoon(); })) : field('Défense', slider(0, 300, 1, it.def, v => { it.def = v; saveSoon(); })),
        field('Vie en plus', slider(0, 2000, 5, it.hp, v => { it.hp = v; saveSoon(); })),
        base.kind === 0 ? field('Coups critiques', slider(0, 1, 0.01, it.crit, v => { it.crit = v; saveSoon(); }, '')) : null,
        base.kind === 0 ? field('Dégâts élémentaires', slider(0, 200, 1, it.elem, v => { it.elem = v; saveSoon(); })) : null,
        field('Couleur', color(it.color, v => { it.color = v; saveSoon(); })),
        field('Lâché par', searchSelect([{ id: '', name: 'Personne', family: '', region: 0, level: 0, arch: 0 }, ...P.creatures.map(c => ({ id: c.id, name: c.name + ' (mod)', family: 'Mod', region: 0, level: c.level, arch: 0 })), ...ENEMIES], it.dropFrom, v => { it.dropFrom = v; saveSoon(); }, e => e.id ? enemyLabel(e) : h('span', {}, 'Personne'))),
        it.dropFrom ? field('Chance', slider(0.01, 1, 0.01, it.dropChance, v => { it.dropChance = v; saveSoon(); }, '')) : null,
        toggle(it.start, v => { it.start = v; saveSoon(); }, 'Offert au début d\'une nouvelle partie'),
        h('div', { class: 'row actions' }, h('button', { class: 'btn danger', onclick: () => { P.items.splice(idx, 1); save(); render(); } }, 'Supprimer'))));
    main.append(card);
  });
}

function pageWorld(main) {
  main.append(h('h2', {}, 'Règles du monde'));
  main.append(h('p', { class: 'dim' }, 'Ces réglages changent toute la partie. Laisse un curseur à sa valeur normale pour ne pas y toucher.'));
  for (const [title, keys] of WORLD) {
    main.append(h('div', { class: 'card form' }, h('h3', {}, title), ...keys.map(([k, l, mn, mx, def, st, unit]) => {
      const on = P.world[k] !== undefined;
      const sl = slider(mn, mx, st, on ? P.world[k] : def, v => { P.world[k] = v; saveSoon(); row.classList.add('set'); }, unit);
      const row = h('div', { class: 'wrow' + (on ? ' set' : '') }, h('span', {}, l), sl, h('button', { class: 'icon', title: 'Valeur normale', onclick: () => { delete P.world[k]; save(); render(); } }, '↺'));
      return row;
    })));
  }
  main.append(h('div', { class: 'card form' }, h('h3', {}, 'Météo'), h('div', { class: 'chips' }, ...WEATHERS.map(w => h('button', { class: 'chip' + ((P.world['game.weather'] || 'normal') === w ? ' on' : ''), onclick: () => { if (w === 'normal') delete P.world['game.weather']; else P.world['game.weather'] = w; save(); render(); } }, w)))));
}

function pageEdits(main) {
  main.append(h('div', { class: 'head' }, h('h2', {}, 'Modifier le jeu'), h('button', { class: 'btn', onclick: () => { P.edits.push({ type: 'enemy', id: 'wolf', field: 'hp', value: '80' }); save(); render(); } }, '+ Changement')));
  main.append(h('p', { class: 'dim' }, 'Change une créature ou un objet qui existe déjà dans le jeu : le nom du loup, la force d\'une épée…'));
  const fields = { enemy: [['name', 'Nom'], ['desc', 'Description'], ['hp', 'Vie'], ['atk', 'Attaque'], ['def', 'Défense'], ['speed', 'Vitesse'], ['scale', 'Taille'], ['aggro', 'Distance de repérage'], ['tint', 'Couleur (#rrggbb)'], ['region', 'Région'], ['spawn', 'Fréquence'], ['level', 'Niveau']],
    item: [['name', 'Nom'], ['desc', 'Description'], ['atk', 'Attaque'], ['def', 'Défense'], ['hp', 'Vie'], ['mp', 'Mana'], ['crit', 'Critique (0 à 1)'], ['speed', 'Vitesse'], ['value', 'Prix'], ['rarity', 'Rareté (commun…ancestral)'], ['color', 'Couleur (#rrggbb)']] };
  P.edits.forEach((e, i) => {
    const list = e.type === 'enemy' ? GAME.enemies : GAME.items;
    main.append(h('div', { class: 'card form' },
      h('div', { class: 'row' }, field('Quoi', select([['enemy', 'Une créature'], ['item', 'Un objet']], e.type, v => { e.type = v; e.id = v === 'enemy' ? 'wolf' : 'sword_iron'; e.field = 'name'; save(); render(); })),
        field('Champ', select(fields[e.type], e.field, v => { e.field = v; saveSoon(); }))),
      field(e.type === 'enemy' ? 'Créature' : 'Objet', searchSelect(list, e.id, v => { e.id = v; saveSoon(); }, e.type === 'enemy' ? enemyLabel : itemLabel)),
      field('Nouvelle valeur', text(e.value, v => { e.value = v; saveSoon(); })),
      h('button', { class: 'btn small danger', onclick: () => { P.edits.splice(i, 1); save(); render(); } }, 'Retirer')));
  });
  main.append(h('div', { class: 'card form' }, h('h3', {}, 'Lignes libres (avancé)'), h('p', { class: 'dim' }, 'Ajoutées telles quelles au fichier data.txt du mod (voir docs/MODDING.md du jeu).'),
    area(P.raw || '', v => { P.raw = v; saveSoon(); }, 'enemy.wolf.hp = 90', 6)));
}

function pageStory(main) {
  main.append(h('div', { class: 'head' }, h('h2', {}, 'Chroniques'), h('button', { class: 'btn', onclick: () => { P.lore.push({ id: 'page_' + (P.lore.length + 1), title: 'Nouvelle page', text: '' }); save(); render(); } }, '+ Page')));
  P.lore.forEach((l, i) => main.append(h('div', { class: 'card form' }, field('Titre', text(l.title, v => { l.title = v; l.id = slug(v); saveSoon(); })), field('Texte', area(l.text, v => { l.text = v; saveSoon(); }, 'Ce que le joueur lira dans ses Chroniques.', 5)),
    h('button', { class: 'btn small danger', onclick: () => { P.lore.splice(i, 1); save(); render(); } }, 'Retirer'))));
  main.append(h('div', { class: 'head' }, h('h2', {}, 'Paroles des PNJ'), h('button', { class: 'btn', onclick: () => { P.npcs.push({ id: GAME.npcs[0].id, greet: '' }); save(); render(); } }, '+ Réplique')));
  P.npcs.forEach((n, i) => main.append(h('div', { class: 'card form' }, field('Personnage', searchSelect(GAME.npcs, n.id, v => { n.id = v; saveSoon(); }, x => h('span', {}, h('b', {}, x.name), h('small', {}, ' · ' + x.title)))),
    field('Ce qu\'il ou elle dit', area(n.greet, v => { n.greet = v; saveSoon(); }, '', 2)), h('button', { class: 'btn small danger', onclick: () => { P.npcs.splice(i, 1); save(); render(); } }, 'Retirer'))));
}

function pageSounds(main) {
  main.append(h('h2', {}, 'Musiques et sons'));
  main.append(h('p', { class: 'dim' }, 'Remplace une musique ou un son du jeu par ton propre fichier .ogg (OGG Vorbis : c\'est le format que lit le jeu).'));
  for (const [path, label] of SOUNDS) {
    const s = P.sounds.find(x => x.target === path);
    main.append(h('div', { class: 'card srow' }, h('div', { class: 'grow' }, h('b', {}, label), h('small', {}, s ? '✓ ' + s.name : 'son d\'origine')),
      s ? h('button', { class: 'icon danger', onclick: async () => { await delFile(s.key); P.sounds = P.sounds.filter(x => x !== s); save(); render(); } }, '✕') : null,
      h('label', { class: 'btn small ghost' }, s ? 'Changer' : 'Choisir', h('input', { type: 'file', accept: '.ogg,audio/ogg', style: 'display:none', onchange: async e => {
        const f = e.target.files[0]; if (!f) return;
        if (!/\.ogg$/i.test(f.name)) { toast('Il faut un fichier .ogg', 'bad'); return; }
        const key = P.id + ':snd:' + path; await putFile(key, new Uint8Array(await f.arrayBuffer()));
        P.sounds = P.sounds.filter(x => x.target !== path); P.sounds.push({ target: path, key, name: f.name }); save(); render(); toast('Son ajouté');
      } }))));
  }
}

// ------------------------------------------------------------------------------------------ 3D models
let modelTarget = 'hero';
let work = null;   // {scene, mesh, rig, turns, tris}
function targetKind(t) {
  if (t.startsWith('prop:')) return 'prop';
  if (t.startsWith('enemy:')) { const id = t.slice(6); const c = P.creatures.find(x => x.id === id); const e = ENEMIES.find(x => x.id === (c ? c.base : id)); return e && e.arch === 1 ? 'quadruped' : 'humanoid'; }
  return 'humanoid';
}
function targetOptions() {
  const o = [];
  P.creatures.forEach(c => { const e = ENEMIES.find(x => x.id === c.base); if (e && e.arch <= 1) o.push(['enemy:' + c.id, '🐺 ' + c.name + ' (ta créature)']); });
  PEOPLE.forEach(([id, n]) => o.push([id, '🧍 ' + n]));
  ENEMIES.filter(e => e.arch <= 1).forEach(e => o.push(['enemy:' + e.id, '⚔ ' + e.name]));
  BUILDINGS.forEach(([id, n]) => o.push(['prop:' + id, '🏠 ' + n]));
  return o;
}
function assetPathOf(t) { if (t.startsWith('prop:')) return 'models/props/' + t.slice(5); if (t.startsWith('enemy:')) return 'models/' + t.slice(6); return 'models/' + t; }

function pageModels(main) {
  main.append(h('h2', {}, 'Modèles 3D'));
  main.append(h('p', { class: 'dim' }, 'Importe un modèle .glb ou .gltf (Tripo, Meshy, Sketchfab…). Le studio le pose sur le squelette du jeu tout seul : il marche, court et attaque comme les autres. Humains : bras écartés (pose en T). Animaux : debout, la tête devant.'));
  const opts = targetOptions();
  if (!opts.find(o => o[0] === modelTarget)) modelTarget = opts[0][0];
  const kind = targetKind(modelTarget);
  const kindLabel = { humanoid: 'Humanoïde (21 os)', quadruped: 'Quadrupède (21 os)', prop: 'Bâtiment (immobile)' }[kind];
  main.append(h('div', { class: 'card form' },
    field('Ce que le modèle remplace', select(opts, modelTarget, v => { modelTarget = v; work = null; render(); })),
    h('div', { class: 'row' }, h('span', { class: 'tag big' }, kindLabel), h('small', { class: 'dim' }, assetPathOf(modelTarget) + '.lkm'))));
  const stage = h('div', { class: 'stage' }); main.append(stage);
  if (!viewer) viewer = new Viewer(h('div', { class: 'vp' })); stage.append(viewer.el); viewer.resize();
  const status = h('div', { class: 'status' }, work ? (work.rig ? `✓ ${work.rig.P.length / 3 | 0} sommets · ${work.rig.I.length / 3 | 0} triangles · ${work.mesh.textures} texture(s)` : 'Modèle chargé : oriente-le, puis lance le squelette automatique.') : 'Aucun modèle chargé.');
  main.append(status);
  if (work && work.rig) {
    if (kind !== 'prop') main.append(h('div', { class: 'card form' }, h('h3', {}, 'Le faire bouger'),
      h('div', { class: 'chips' }, ...[['idle', 'Repos'], ['walk', 'Marche'], ['run', 'Course'], ['attack', 'Attaque'], ['jump', 'Saut']].map(([a, l]) => h('button', { class: 'chip' + (viewer.anim === a ? ' on' : ''), onclick: e => { viewer.anim = a; [...e.target.parentNode.children].forEach(c => c.classList.toggle('on', c === e.target)); } }, l))),
      h('div', { class: 'row' }, toggle(viewer.showSkeleton, v => viewer.setSkeletonVisible(v), 'Voir le squelette'), toggle(work.weights, v => { work.weights = v; viewer.showRig(work.rig, work.mesh.canvas, v); }, 'Voir les zones des os'))));
    main.append(h('button', { class: 'btn wide gold', onclick: saveModel }, '💾 Enregistrer dans le mod'));
  }
  const controls = h('div', { class: 'card form' });
  controls.append(h('label', { class: 'btn wide' }, '📂 Importer un fichier 3D (.glb, .gltf)', h('input', { type: 'file', accept: '.glb,.gltf,model/gltf-binary', style: 'display:none', onchange: e => loadModelFile(e.target.files[0]) })));
  if (work) {
    controls.append(h('div', { class: 'row' },
      h('button', { class: 'btn ghost', onclick: () => { work.turns = (work.turns + 1) % 4; work.rig = null; prepare(); } }, '⟳ Tourner de 90°'),
      h('span', { class: 'dim' }, kind === 'prop' ? 'L\'entrée doit regarder vers toi.' : 'Le visage doit regarder vers toi.')));
    controls.append(field('Détail (triangles)', slider(2000, 40000, 1000, work.tris, v => { work.tris = v; work.rig = null; }, ''), 'Moins de triangles = plus fluide sur téléphone. 12 000 à 20 000 conviennent.'));
    controls.append(h('button', { class: 'btn wide gold', onclick: runRig }, kind === 'prop' ? '✦ Préparer le bâtiment' : '✦ Squelette automatique'));
  }
  main.append(controls);
  // models already in the mod
  if (P.models.length) {
    main.append(h('h3', {}, 'Dans ce mod'));
    main.append(h('div', { class: 'list' }, ...P.models.map(m => h('div', { class: 'card srow' }, m.thumb ? h('img', { class: 'thumb', src: m.thumb }) : null, h('div', { class: 'grow' }, h('b', {}, m.label), h('small', {}, `${m.kind} · ${m.tris} triangles`)),
      h('button', { class: 'icon danger', onclick: async () => { await delFile(m.key + '.lkm'); await delFile(m.key + '.jpg'); P.models = P.models.filter(x => x !== m); save(); render(); } }, '✕')))));
  }
  if (work) { if (work.rig) viewer.showRig(work.rig, work.mesh.canvas, work.weights); else if (work.preview) viewer.showRig(work.preview, work.mesh.canvas, false); }
}

async function loadModelFile(f) {
  if (!f) return;
  const busy = overlay('Lecture du modèle…');
  try {
    const buf = await f.arrayBuffer();
    const loader = new GLTFLoader(); loader.setMeshoptDecoder(MeshoptDecoder);
    const gltf = await new Promise((res, rej) => loader.parse(buf, '', res, rej));
    const mesh = await extractMesh(gltf.scene, 1024);
    work = { name: f.name, raw: mesh, mesh, turns: 0, tris: Math.min(16000, Math.max(2000, mesh.I.length / 3 | 0)), rig: null, weights: false };
    busy.remove();
    await prepare(); render();
    toast(`${f.name} : ${mesh.I.length / 3 | 0} triangles`);
  } catch (e) { busy.remove(); toast('Impossible de lire ce fichier : ' + (e.message || e), 'bad'); console.error(e); }
}

async function prepare() {
  if (!work) return;
  const m = orient(work.raw, work.turns);
  work.mesh = { ...m, canvas: work.raw.canvas, textures: work.raw.textures };
  // a static preview (no skinning) while the rig is not done
  const nv = m.P.length / 3, hmax = m.size[1] || 1, k = (targetKind(modelTarget) === 'quadruped' ? 1.0 : 1.8) / hmax;
  work.preview = { ...m, P: m.P.map(v => v * k), joints: [[0, 0, 0]], b0: new Uint8Array(nv), b1: new Uint8Array(nv), w0: new Float32Array(nv).fill(1), height: hmax * k, kind: 'prop' };
  if (viewer) viewer.showRig(work.preview, work.raw.canvas, false);
}

async function runRig() {
  const kind = targetKind(modelTarget);
  const busy = overlay(kind === 'prop' ? 'Préparation…' : 'Squelette automatique… (quelques secondes)');
  await new Promise(r => setTimeout(r, 50));
  try {
    let m = await simplify(work.mesh, work.tris);
    m = { ...m, canvas: work.raw.canvas, textures: work.raw.textures };
    work.mesh = m;
    work.rig = kind === 'humanoid' ? rigHumanoid(m, 1.8) : kind === 'quadruped' ? rigQuadruped(m) : rigProp(m);
    busy.remove(); render(); const st = document.querySelector('.stage'); if (st) st.scrollIntoView({ block: 'start' });
    if (kind === 'humanoid' && !work.rig.tpose) toast('Les bras ne semblent pas écartés : le résultat peut être moins bon.', 'warn'); else toast('Prêt ! Essaie les animations.');
  } catch (e) { busy.remove(); toast('Échec : ' + (e.message || e), 'bad'); console.error(e); }
}

async function saveModel() {
  const busy = overlay('Enregistrement…');
  try {
    const lkm = writeLKM(work.rig, true);
    const jpg = canvasToJpeg(work.mesh.canvas, 1024);
    const key = P.id + ':' + assetPathOf(modelTarget);
    await putFile(key + '.lkm', lkm); await putFile(key + '.jpg', jpg);
    const label = (targetOptions().find(o => o[0] === modelTarget) || [0, modelTarget])[1];
    P.models = P.models.filter(x => x.target !== modelTarget);
    P.models.push({ target: modelTarget, key, path: assetPathOf(modelTarget), kind: targetKind(modelTarget), tris: work.rig.I.length / 3 | 0, label, thumb: viewer.snapshot() });
    save(); busy.remove(); toast('Modèle ajouté au mod ✓'); render();
  } catch (e) { busy.remove(); toast('Échec : ' + (e.message || e), 'bad'); }
}

function overlay(msg) { const o = h('div', { class: 'overlay' }, h('div', { class: 'spin' }), h('p', {}, msg)); document.body.append(o); return o; }

// ------------------------------------------------------------------------------------------ export
function buildData() {
  const L = [`# ${P.name} — fait avec LK Studio`, ''], warn = [];
  const ids = new Set();
  for (const it of P.items) {
    if (ids.has(it.id)) warn.push(`Deux objets ont l'identifiant « ${it.id} ».`); ids.add(it.id);
    if (GAME.items.some(g => g.id === it.id)) warn.push(`L'objet « ${it.name} » a l'identifiant d'un objet du jeu (${it.id}) : change-le.`);
    const base = BASE_ITEMS.find(b => b.id === it.base) || BASE_ITEMS[0];
    L.push(`item.add = ${it.id} | ${it.base} | ${it.name}${it.desc ? ' | ' + it.desc.replace(/\n/g, ' ') : ''}`, `item.${it.id}.rarity = ${RARITIES[it.rarity][0]}`);
    if (base.kind === 0) L.push(`item.${it.id}.atk = ${it.atk}`, `item.${it.id}.crit = ${it.crit}`, `item.${it.id}.elem_dmg = ${it.elem}`); else L.push(`item.${it.id}.def = ${it.def}`);
    if (it.hp) L.push(`item.${it.id}.hp = ${it.hp}`);
    L.push(`item.${it.id}.color = ${it.color}`, `item.${it.id}.value = ${Math.max(10, Math.round((it.atk + it.def) * 30 * (1 + it.rarity)))}`, '');
  }
  for (const c of P.creatures) {
    if (ids.has(c.id)) warn.push(`L'identifiant « ${c.id} » est utilisé deux fois.`); ids.add(c.id);
    if (GAME.enemies.some(g => g.id === c.id)) warn.push(`La créature « ${c.name} » a l'identifiant d'une créature du jeu (${c.id}) : change-le.`);
    L.push(`enemy.add = ${c.id} | ${c.base} | ${c.name}${c.desc ? ' | ' + c.desc.replace(/\n/g, ' ') : ''}`, `enemy.${c.id}.region = ${c.region}`, `enemy.${c.id}.tint = ${c.tint}`,
      `enemy.${c.id}.scale = ${c.scale}`, `enemy.${c.id}.hp = ${c.hp}`, `enemy.${c.id}.atk = ${c.atk}`, `enemy.${c.id}.level = ${c.level}`, `enemy.${c.id}.spawn = ${c.spawn}`,
      `enemy.${c.id}.xp = ${Math.round(8 + c.level * c.level * 3)}`, `enemy.${c.id}.gold = ${2 + c.level * 2}`);
    if (c.night) L.push(`enemy.${c.id}.night = 1`);
    if (c.shards > 0) L.push(`enemy.${c.id}.loot = star_shard | ${c.shards}`);
    for (const l of c.loot) L.push(`enemy.${c.id}.loot = ${l.item} | ${l.chance}`);
    L.push('');
  }
  for (const it of P.items) { if (it.dropFrom) L.push(`enemy.${it.dropFrom}.loot = ${it.id} | ${it.dropChance}`); if (it.start) L.push(`game.start_item = ${it.id} | 1`); }
  for (const [k, v] of Object.entries(P.world)) L.push(`${k} = ${v}`);
  for (const e of P.edits) if (e.value !== '') L.push(`${e.type}.${e.id}.${e.field} = ${e.value}`);
  for (const n of P.npcs) if (n.greet) L.push(`npc.${n.id}.greet = ${n.greet.replace(/\n/g, ' ')}`);
  for (const l of P.lore) if (l.title) L.push(`lore.add = ${slug(l.id || l.title)} | ${l.title} | ${(l.text || '').replace(/\n/g, ' ')}`);
  if (P.raw) L.push('', '# lignes libres', P.raw);
  if (!P.name.trim()) warn.push('Le mod n\'a pas de nom.');
  return { text: L.join('\n') + '\n', warn };
}

function pageExport(main) {
  const { text: data, warn } = buildData();
  main.append(h('h2', {}, 'Exporter vers LOST KINGDOM'));
  main.append(h('div', { class: 'card summary' },
    h('div', {}, h('b', {}, P.creatures.length), h('small', {}, 'créatures')), h('div', {}, h('b', {}, P.items.length), h('small', {}, 'objets')),
    h('div', {}, h('b', {}, P.models.length), h('small', {}, 'modèles 3D')), h('div', {}, h('b', {}, P.sounds.length), h('small', {}, 'sons'))));
  if (warn.length) main.append(h('div', { class: 'card warnbox' }, h('b', {}, 'À vérifier'), ...warn.map(w => h('p', {}, '⚠ ' + w))));
  main.append(h('button', { class: 'btn wide gold big', onclick: exportZip }, '⬇ Créer le fichier du mod (.zip)'));
  main.append(h('div', { class: 'card steps' }, h('b', {}, 'Ensuite, dans le jeu :'), h('ol', {},
    h('li', {}, 'Journal → Paramètres → Mods → « Importer un mod (.zip) »'), h('li', {}, 'Choisis le fichier dans Téléchargements / LOST KINGDOM Mods'), h('li', {}, 'Active le mod, puis redémarre le jeu'))));
  if (window.LKS) main.append(h('button', { class: 'btn wide ghost', onclick: () => { if (!window.LKS.openGame()) toast('LOST KINGDOM n\'est pas installé sur ce téléphone.', 'bad'); } }, '▶ Ouvrir LOST KINGDOM'));
  main.append(h('details', { class: 'card' }, h('summary', {}, 'Voir le fichier data.txt'), h('pre', {}, data)));
}

async function exportZip() {
  const busy = overlay('Création du mod…');
  try {
    const { text: data } = buildData();
    const files = [{ name: 'mod.txt', data: `name = ${P.name}\nauthor = ${P.author}\nversion = ${P.version}\ndescription = ${(P.description || '').replace(/\n/g, ' ')}\n` }, { name: 'data.txt', data }];
    for (const m of P.models) { const a = await getFile(m.key + '.lkm'), b = await getFile(m.key + '.jpg'); if (a && b) { files.push({ name: 'assets/' + m.path + '.lkm', data: a }, { name: 'assets/' + m.path + '.jpg', data: b }); } }
    for (const s of P.sounds) { const a = await getFile(s.key); if (a) files.push({ name: 'assets/' + s.target, data: a }); }
    const zip = makeZip(files);
    const name = slug(P.name).replace(/_/g, '-') + '.zip';
    busy.remove();
    if (window.LKS && window.LKS.saveFile) {
      const r = window.LKS.saveFile(name, toBase64(zip), 'application/zip');
      if (r.startsWith('error')) { toast(r, 'bad'); return; }
      modal(h('div', {}, h('h3', {}, 'Mod créé ✓'), h('p', {}, 'Enregistré dans ', h('b', {}, r)), h('p', { class: 'dim' }, `${(zip.length / 1024 / 1024).toFixed(2)} Mo`),
        h('div', { class: 'row' }, h('button', { class: 'btn gold', onclick: () => window.LKS.shareFile(name, 'application/zip') }, 'Partager'), h('button', { class: 'btn ghost', onclick: () => window.LKS.openGame() }, 'Ouvrir le jeu'))));
    } else {
      const a = h('a', { href: URL.createObjectURL(new Blob([zip], { type: 'application/zip' })), download: name }); document.body.append(a); a.click(); a.remove();
      toast('Mod téléchargé : ' + name);
    }
  } catch (e) { busy.remove(); toast('Échec : ' + (e.message || e), 'bad'); console.error(e); }
}
function modal(content) { const m = h('div', { class: 'modal', onclick: e => { if (e.target === m) m.remove(); } }, h('div', { class: 'card' }, content, h('button', { class: 'btn small ghost', onclick: () => m.remove() }, 'Fermer'))); document.body.append(m); }

// open an existing mod (.zip): the known lines become editable, the rest goes to "free lines"
async function importZip(f) {
  if (!f) return;
  try {
    const files = await readZip(new Uint8Array(await f.arrayBuffer()));
    const dec = new TextDecoder(), get = n => { const x = files.find(z => z.name === n || z.name.endsWith('/' + n)); return x ? dec.decode(x.data) : ''; };
    const p = newProject('vide');
    for (const l of get('mod.txt').split('\n')) { const [k, ...v] = l.split('='); const val = v.join('=').trim(); if (k.trim() === 'name') p.name = val; if (k.trim() === 'author') p.author = val; if (k.trim() === 'version') p.version = val; if (k.trim() === 'description') p.description = val; }
    const raw = [];
    for (const l0 of get('data.txt').split('\n')) {
      const l = l0.trim(); if (!l || l.startsWith('#')) continue;
      const m = l.match(/^(game\.[a-z_]+|bomb\.[a-z]+|dig\.[a-z]+)\s*=\s*(.+)$/);
      if (m && !m[1].startsWith('game.start_item') && !m[1].startsWith('game.start_weapon')) { p.world[m[1]] = isNaN(+m[2]) ? m[2] : +m[2]; continue; }
      raw.push(l);
    }
    p.raw = raw.join('\n');
    for (const z of files) {
      const mm = z.name.match(/assets\/(models\/(?:props\/)?[a-z0-9_]+)\.lkm$/);
      if (mm) { const jpg = files.find(y => y.name === z.name.replace('.lkm', '.jpg')); const target = mm[1].startsWith('models/props/') ? 'prop:' + mm[1].slice(13) : (PEOPLE.some(x => x[0] === mm[1].slice(7)) ? mm[1].slice(7) : 'enemy:' + mm[1].slice(7));
        const key = p.id + ':' + mm[1]; await putFile(key + '.lkm', z.data); if (jpg) await putFile(key + '.jpg', jpg.data);
        p.models.push({ target, key, path: mm[1], kind: target.startsWith('prop:') ? 'prop' : '?', tris: 0, label: mm[1] }); }
      const sm = z.name.match(/assets\/(audio\/.+\.ogg)$/);
      if (sm) { const key = p.id + ':snd:' + sm[1]; await putFile(key, z.data); p.sounds.push({ target: sm[1], key, name: sm[1].split('/').pop() }); }
    }
    P = p; save(); go('project'); toast('Mod ouvert : ' + p.name);
  } catch (e) { toast('Impossible d\'ouvrir ce zip : ' + (e.message || e), 'bad'); }
}

render();
