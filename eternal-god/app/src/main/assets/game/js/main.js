// ETERNAL GOD — point d'entrée : chargement (écran SuperEssence Studio), création du monde,
// boucle de jeu, sauvegarde, résolution dynamique, cycle de vie Android.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { G, Q, settings, saveSettings, detectQuality, on, emit, clamp } from './core.js';
import { Terrain } from './terrain.js';
import { loadTextureSet, TERRAIN_LAYERS, BUILDING_LAYERS } from './textures.js';
import { PhotoSky } from './sky.js';
import { Environment } from './env.js';
import { Vegetation, loadVegetationAssets } from './vegetation.js';
import { Villages, loadProps } from './village.js';
import { Roads } from './roads.js';
import { People, loadPeople } from './people.js';
import { Fauna, loadFauna } from './fauna.js';
import { FX } from './fx.js';
import { Physics } from './physics.js';
import { God } from './god.js';
import { loadHero, HEROES } from './heromodel.js';
import { CameraRig } from './camera.js';
import { Powers } from './powers.js';
import { Ultimates } from './ultimates.js';
import { Monster } from './monster.js';
import { Audio } from './audio.js';
import { UI } from './ui.js';

const SAVE_KEY = 'eg.save.v1';
const $ = (s) => document.querySelector(s);

if (!settings.quality) { settings.quality = detectQuality(); saveSettings(); }

// ---------- progression réelle du chargement ----------
let progress = 0, total = 1;
function step(label, weight = 1) { progress += weight; $('#barFill').style.width = `${Math.min(100, progress / total * 100)}%`; if (label) $('#loadText').textContent = label; }
const frame = () => new Promise((r) => requestAnimationFrame(() => setTimeout(r, 0)));

async function boot() {
  total = 48;
  const q = Q();
  // moteur de rendu
  const canvas = $('#c');
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: q.label !== 'Bas', powerPreference: 'high-performance', stencil: false, preserveDrawingBuffer: false });
  G.basePR = Math.min(window.devicePixelRatio || 1, 2) * q.pixelRatio;
  G.pr = G.basePR;
  renderer.setPixelRatio(G.pr);
  renderer.setSize(innerWidth, innerHeight);
  renderer.toneMapping = THREE.AgXToneMapping;
  renderer.shadowMap.enabled = !!q.shadows;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;
  G.renderer = renderer;
  canvas.addEventListener('webglcontextlost', (e) => { e.preventDefault(); emit('save'); $('#fatalMsg').textContent = 'L\'affichage a été interrompu par le système. Le monde est sauvegardé.'; $('#fatal').classList.remove('hidden'); });
  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(58, innerWidth / innerHeight, 0.4, 6000);
  G.scene = scene; G.camera = camera;
  G.uniforms = { time: { value: 0 }, gust: { value: new THREE.Vector4() }, press: { value: new THREE.Vector4() } };
  step('Préparation du monde…');
  await frame();

  // ressources
  const texSize = q.tex || 1024;
  const terrTex = await loadTextureSet(TERRAIN_LAYERS, 'img/tex', texSize, () => step('Textures du sol…', 0.5));
  const bTex = await loadTextureSet(BUILDING_LAYERS, 'img/tex', texSize, () => step('Pierre, bois et tuiles…', 0.5));
  G.photoSky = new PhotoSky(); await G.photoSky.load(renderer, () => step('Ciels du monde…', 0.5));
  const vegAssets = await loadVegetationAssets(() => step('Forêts et prairies…', 0.5));
  const props = await loadProps(() => step('Objets des villages…', 0.3));
  const peopleAssets = await loadPeople(() => step('Habitants…', 0.6));
  const faunaAssets = await loadFauna(() => step('Faune…', 0.3));

  // monde
  step('Façonnage du relief…'); await frame();
  const T = new Terrain(scene); G.terrain = T; T.textures = terrTex;
  T.generate();
  step('Fondation des villages…', 2); await frame();
  const villages = new Villages(scene, props, bTex); G.villages = villages;
  villages.prepare(); villages.build();
  const roads = new Roads(scene, villages); G.roads = roads; roads.build();
  const crops = villages.dressFarms(vegAssets.grass);
  villages.finalize();
  step('Sculpture du terrain…', 2); await frame();
  T.build(scene);
  step('Croissance des forêts…', 2); await frame();
  const veg = new Vegetation(scene, vegAssets); G.veg = veg;
  veg.build((x, z) => !villages.isFree(x, z, 2));
  veg.extraFields = crops;
  step('Lumière et ciel…', 1); await frame();
  const env = new Environment(scene, renderer); G.env = env;
  env.build();
  G.fx = new FX(scene); G.shake = (a, p) => G.fx.shake(a, p);
  G.physics = new Physics(scene, villages.mat);
  step('Éveil du dieu…', 1); await frame();
  let heroModel = null;
  if (settings.hero && settings.hero !== 'celeste') {
    try { heroModel = await loadHero(settings.hero); } catch (e) { console.error('Héros indisponible, retour au Dieu Céleste', e); settings.hero = 'celeste'; saveSettings(); }
  }
  const god = new God(scene, heroModel); G.god = god;
  G.cam = new CameraRig(camera);
  G.powers = new Powers(scene);
  G.ultimates = new Ultimates(scene);
  G.monster = new Monster(scene);
  G.audio = new Audio();
  step('Les habitants s\'éveillent…', 1); await frame();
  const people = new People(scene, peopleAssets, villages); G.people = people;
  people.populate();
  people.addTravelers(roads, q.label === 'Bas' ? 4 : 8);
  const fauna = new Fauna(scene, faunaAssets); G.fauna = fauna; fauna.populate();
  G.ui = new UI();

  // reprise d'une partie sauvegardée
  const save = loadSave();
  if (save) restore(save);
  else {
    god.pos.set(20, T.groundAt(20, 120) + 70, 120);
    G.cam.yaw = 0; G.cam.pitch = 0.22;
    env.dayTime = 0.33;
  }

  // post-traitement (lueur divine) en qualité élevée seulement
  let composer = null;
  if (q.bloom) {
    composer = new EffectComposer(renderer);
    composer.addPass(new RenderPass(scene, camera));
    composer.addPass(new UnrealBloomPass(new THREE.Vector2(innerWidth / 2, innerHeight / 2), 0.32, 0.45, 1.5));
    composer.addPass(new OutputPass());
  }
  G.composer = composer;
  // pré-compilation des shaders pour éviter les saccades à la première apparition
  step('Compilation des matériaux…', 2); await frame();
  try { renderer.compile(scene, camera); } catch (e) { /* facultatif */ }
  env.update(0, true);
  G.cam.update(0.016);
  render();
  step('Prêt.', 10);
  await frame();
  showTitle();
}

function render() {
  if (G.composer) G.composer.render(); else G.renderer.render(G.scene, G.camera);
}
G.renderNow = render;
window.G = G; // accès de débogage (console, tests automatisés)

function showTitle() {
  $('#studio').classList.add('hidden');
  $('#title').classList.remove('hidden');
  const start = () => {
    $('#boot').removeEventListener('pointerup', start);
    G.audio.unlock();
    $('#boot').classList.add('fade');
    setTimeout(() => $('#boot').remove(), 1100);
    $('#hud').classList.remove('hidden');
    G.started = true; G.paused = false; window.__started = true;
    if (!G.restored) emit('journal', { icon: '✦', text: 'Le dieu éternel contemple son monde. Tout lui appartient.' });
    loop.last = performance.now();
    requestAnimationFrame(loop);
  };
  $('#boot').addEventListener('pointerup', start);
}

// ---------- boucle ----------
let fpsAvg = 60, slowT = 0, fastT = 0;
function loop(now) {
  requestAnimationFrame(loop);
  let dt = Math.min(0.05, (now - loop.last) / 1000 || 0.016);
  loop.last = now;
  fpsAvg = fpsAvg * 0.95 + (1 / Math.max(dt, 1e-3)) * 0.05;
  if (G.paused && !G.photoMode) { return; }
  dt *= G.slowmo || 1;
  try {
    if (!G.photoMode) {
      G.time += dt; G.uniforms.time.value = G.time;
      const input = G.ui.input;
      G.god.update(dt, input);
      G.powers.update(dt, input);
      G.ultimates.update(dt);
      if (!G.timeStop) { // arrêt du temps : le monde se fige, seul le dieu agit
        G.monster.update(dt);
        G.people.update(dt);
        G.fauna.update(dt);
        G.villages.update(dt);
        G.physics.update(dt);
      }
      G.fx.update(dt);
      if (Math.random() < dt * 3) G.fx.motes(G.god.pos, 6, 1);
    }
    if (!G.timeStop) G.veg.update(dt);
    G.terrain.update(dt);
    G.env.update(G.photoMode ? 0 : dt);
    G.cam.update(dt);
    G.audio.update(dt);
    G.ui.update(dt, fpsAvg);
    render();
  } catch (e) {
    // une erreur dans un système ne doit jamais figer le jeu : on la signale et on continue
    console.error(e);
    if (!loop.errShown) { loop.errShown = true; G.ui && G.ui.toast('⚠', 'Un incident a été corrigé automatiquement.', 3); }
  }
  adaptResolution(dt);
}

// résolution dynamique : garde le jeu fluide sur les téléphones modestes
function adaptResolution(dt) {
  if (fpsAvg < 26) { slowT += dt; fastT = 0; } else if (fpsAvg > 50) { fastT += dt; slowT = 0; } else { slowT = fastT = 0; }
  if (slowT > 2.5 && G.pr > G.basePR * 0.55) { G.pr = Math.max(G.basePR * 0.55, G.pr * 0.85); applyPR(); slowT = 0; }
  if (fastT > 6 && G.pr < G.basePR) { G.pr = Math.min(G.basePR, G.pr * 1.1); applyPR(); fastT = 0; }
}
function applyPR() { G.renderer.setPixelRatio(G.pr); G.renderer.setSize(innerWidth, innerHeight); if (G.composer) G.composer.setPixelRatio(G.pr); }

addEventListener('resize', () => {
  if (!G.renderer) return;
  G.camera.aspect = innerWidth / innerHeight; G.camera.updateProjectionMatrix();
  G.renderer.setSize(innerWidth, innerHeight); if (G.composer) G.composer.setSize(innerWidth, innerHeight);
});

// ---------- sauvegarde ----------
function snapshot() {
  const V = G.villages.villages;
  return {
    v: 1, time: G.time, dayTime: G.env.dayTime, day: G.env.day,
    god: [G.god.pos.x, G.god.pos.y, G.god.pos.z, G.god.heading, G.god.state === 'fly' ? 1 : 0],
    villages: V.map((v) => [v.worship, v.fear]),
    buildings: G.villages.serialize(), trees: G.veg.serialize(),
    terrain: G.terrain.serialize(), journal: G.ui.journal.slice(-60), deaths: G.people.deaths,
    monster: G.monster.alive ? 30 : G.monster.respawnT,
  };
}
function save() {
  if (!G.started) return;
  try { localStorage.setItem(SAVE_KEY, JSON.stringify(snapshot())); }
  catch (e) { // stockage plein : on garde l'essentiel sans les déformations du terrain
    try { const s = snapshot(); s.terrain = null; localStorage.setItem(SAVE_KEY, JSON.stringify(s)); } catch (e2) { /* tant pis */ }
  }
}
function loadSave() { try { const s = JSON.parse(localStorage.getItem(SAVE_KEY) || 'null'); return s && s.v === 1 ? s : null; } catch (e) { return null; } }
function restore(s) {
  try {
    G.time = s.time || 0; G.env.dayTime = s.dayTime; G.env.day = s.day || 1;
    G.god.pos.set(s.god[0], s.god[1], s.god[2]); G.god.heading = s.god[3]; G.god.state = s.god[4] ? 'fly' : 'air';
    s.villages.forEach((r, i) => { const v = G.villages.villages[i]; if (v) { v.worship = r[0]; v.fear = r[1]; } });
    G.terrain.restore(s.terrain);
    G.veg.restore(s.trees);
    G.villages.restoreState(s.buildings);
    G.ui.journal = s.journal || [];
    G.people.deaths = s.deaths || 0;
    G.monster.respawnT = s.monster ?? 60;
    G.restored = true;
    G.ui.journal.push({ icon: '↺', text: 'Le monde se souvient de vous.', day: G.env.day, hour: '' });
  } catch (e) { console.error('Sauvegarde illisible, nouveau monde.', e); }
}
on('save', save);
on('resetWorld', () => { try { localStorage.removeItem(SAVE_KEY); } catch (e) { /* ignore */ } G.started = false; location.reload(); });
setInterval(() => { if (G.started && !G.paused) save(); }, 45000);

// ---------- cycle de vie Android ----------
window.EG = {
  onPause() { save(); G.wasPaused = G.paused; G.paused = true; G.audio && G.audio.suspend(); },
  onResume() { G.paused = !!G.wasPaused || (G.ui && G.ui.menuOpen); G.audio && G.audio.resume(); loop.last = performance.now(); },
  onBack() {
    if (!G.started) return false;
    if (G.photoMode) { G.ui.photo(false); return true; }
    if (G.ui.menuOpen) { G.ui.closeMenu(); return true; }
    const w = document.querySelector('#wheel'); if (w && !w.classList.contains('hidden')) { G.ui.toggleWheel(false); return true; }
    G.ui.openMenu('map'); return true;
  },
};
document.addEventListener('visibilitychange', () => { if (document.hidden) window.EG.onPause(); else window.EG.onResume(); });

boot().catch((e) => { console.error(e); window.__fatal('Chargement impossible : ' + (e && e.message ? e.message : e)); });
