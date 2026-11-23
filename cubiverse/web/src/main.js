// CUBIVERSE — application principale : écrans, interactions, boucle de jeu, sauvegarde.
import './styles.css';
import * as THREE from 'three';
import { Renderer3D } from './render/renderer.js';
import { Simulation } from './sim/simulation.js';
import { starterWorld, emptyWorld, structureCells, kindLabel } from './sim/presets.js';
import { saveWorld, loadWorld, listWorlds, deleteWorld, lastWorldId, loadSettings, saveSettings, serialize, deserialize } from './sim/persistence.js';
import { THEMES, THEME_KEYS, OBJECT_TYPES, OBJECT_KEYS, ITEMS } from './sim/themes.js';
import { CUBE, SIDES, SIDE_LABELS, NEED_LABELS, TRAIT_LABELS, EMOTION_LABELS, MAX_PER_CUBE, TRAITS, clamp } from './sim/constants.js';
import { relLabel, bestFriendId, mood, dominantEmotion, personalitySummary, getRel, groupOf } from './sim/social.js';
import { abortAct } from './sim/ai.js';
import { PALETTE } from './sim/names.js';
import { pick, newId } from './sim/rng.js';
import { EVENT_LABELS } from './sim/events.js';
import { $, $$, el, esc, toast, bindActs, fmtTime, confirmBox, promptBox } from './ui/dom.js';
import { playIntro, APP_LOGO_SVG } from './ui/intro.js';

const DEFAULTS = { style: 'pixel', maxScreens: 48, quality: 'medium', shadows: true, bubbles: true, battery: false, simRadius: 3.2, maxActive: 48, detail: 8, maxDetail: 260, maxChars: 900, autoRotateMenu: true };
const EMO_ICON = { joy: '😄', sadness: '😢', anger: '😠', fear: '😨', surprise: '😮', neutral: '🙂' };
const SKILL_LABEL = { cuisine: 'Cuisine', sport: 'Sport', art: 'Art', science: 'Science', jardinage: 'Jardinage', 'pêche': 'Pêche', construction: 'Construction', musique: 'Musique', social: 'Social', lecture: 'Lecture' };
const ACT_LABEL = { sleep: 'Dormir', sit: "S'asseoir", eatTable: 'Manger à table', eat: 'Grignoter', drink: 'Boire', cook: 'Cuisiner', watch: 'Regarder la télé', read: 'Lire', draw: 'Dessiner', garden: 'Jardiner', fish: 'Pêcher', run: 'Courir', lift: 'Musculation', work: 'Travailler', experiment: 'Expériences', operate: 'Machines', playArcade: 'Arcade', teach: 'Écrire au tableau', study: 'Étudier', swim: 'Nager', danceMusic: 'Danser (musique)', playPiano: 'Piano', build: 'Construire', observe: 'Observer', warm: 'Feu de camp', swing: 'Balançoire', snack: 'En-cas', wash: 'Vaisselle', clean: 'Nettoyer', reign: 'Jouer au roi', sunbathe: 'Bronzer', buildSand: 'Château de sable', pilot: 'Piloter', mine: 'Extraire', climbRock: 'Admirer la vue', drive: 'Conduire', slide: 'Toboggan', water: 'Arroser', wander: 'Se promener', jog: 'Courir partout', dance: 'Danser', jump: 'Sauter', roll: 'Roulades', explore: 'Explorer', visit: 'Rendre visite', talk: 'Discuter', joke: 'Blagues', hug: 'Câlins', chase: 'Jouer à chat', hide: 'Cache-cache', ball: 'Ballon', wave: 'Faire coucou', handshake: 'Serrer la main', danceDuo: 'Danser à deux', argue: 'Se disputer', fight: 'Bagarres', gift: 'Offrir', serve: 'Servir', order: 'Commander', sitFloor: 'Assis par terre', sulk: 'Bouder', cry: 'Pleurer', laugh: 'Rire', juggle: 'Jongler', readOwn: 'Lire son livre', eatItem: 'Manger', napFloor: 'Sieste', pick: 'Cueillir', takeBall: 'Prendre un ballon', restTree: "Repos sous l'arbre", lean: "S'adosser", salute: 'Saluer', newspaper: 'Journal' };

class App {
  constructor() {
    this.settings = loadSettings(DEFAULTS);
    this.canvas = $('#view');
    this.r3d = new Renderer3D(this.canvas, this.settings);
    this.world = null; this.sim = null;
    this.state = { selectedCube: null, selectedChar: null, editMode: false, forceRebuild: true, placing: null, movingObj: null, selectedObj: null };
    this.mode = 'menu';
    this.overlays = []; // pile pour le bouton retour
    this.lastT = performance.now(); this.acc = 0; this.fps = 60; this.frames = 0; this.fpsT = 0;
    this.hudTimer = 0; this.saveTimer = 0; this.sheetTimer = 0;
    this.running = true;
    window.addEventListener('resize', () => this.r3d.resize());
    this.bindPointer();
    this.bindLifecycle();
  }

  async boot() {
    const last = lastWorldId();
    let w = last ? await loadWorld(last).catch(() => null) : null;
    if (!w) w = starterWorld('Mon premier univers');
    this.setWorld(w);
    requestAnimationFrame((t) => this.loop(t));
    await playIntro();
    this.showMenu();
  }

  setWorld(w) {
    this.world = w;
    if (!this.sim) this.sim = new Simulation(w); else this.sim.setWorld(w);
    this.applySimSettings();
    this.state.selectedCube = null; this.state.selectedChar = null; this.state.forceRebuild = true;
    this.r3d.followId = null; this.r3d.cutY = 999;
    // centre la caméra sur le premier cube
    let cx = 0, cz = 0, n = 0; for (const c of w.cubes.values()) { cx += c.x; cz += c.z; n++; if (n > 50) break; }
    const t = new THREE.Vector3((cx / Math.max(1, n)) * CUBE, 1, (cz / Math.max(1, n)) * CUBE);
    let ext = 0; for (const c of w.cubes.values()) { ext = Math.max(ext, Math.abs(c.x * CUBE - t.x), Math.abs(c.y * CUBE)); if (ext > 40) break; }
    t.y = Math.min(ext, 8) * 0.4 + 1.5;
    const d = Math.min(60, 14 + ext * 2.6);
    this.r3d.controls.target.copy(t); this.r3d.camera.position.set(t.x + d * 0.12, t.y + d * 0.28, t.z + d);
    this.sim.focus = [t.x, t.y, t.z]; this.sim.refreshLOD();
  }
  applySimSettings() { this.sim.radius = this.settings.simRadius; this.sim.maxActive = this.settings.maxActive; }

  // ---------------- boucle ----------------
  loop(t) {
    requestAnimationFrame((tt) => this.loop(tt));
    if (!this.running) return;
    const dt = Math.min(0.1, (t - this.lastT) / 1000);
    const minFrame = this.settings.battery ? 1 / 31 : 0;
    this.acc += dt; this.lastT = t;
    if (this.acc < minFrame) return;
    const step = this.acc; this.acc = 0;
    const tg = this.r3d.controls.target; this.sim.focus = [tg.x, tg.y, tg.z];
    if (this.mode === 'menu' && this.settings.autoRotateMenu) { this.r3d.controls.autoRotate = true; this.r3d.controls.autoRotateSpeed = 0.6; } else this.r3d.controls.autoRotate = false;
    this.sim.step(step);
    const info = this.r3d.render(this.world, step, this.state);
    this.frames++; this.fpsT += step; if (this.fpsT > 1) { this.fps = Math.round(this.frames / this.fpsT); this.frames = 0; this.fpsT = 0; }
    this.hudTimer += step; if (this.hudTimer > 0.5) { this.hudTimer = 0; this.updateHud(info); }
    this.sheetTimer += step; if (this.sheetTimer > 1) { this.sheetTimer = 0; this.refreshSheet(); }
    this.saveTimer += step; if (this.saveTimer > 45) { this.saveTimer = 0; this.save(true); }
  }

  async save(withThumb = false) {
    if (!this.world) return;
    let thumb = null;
    if (withThumb) { try { thumb = this.r3d.snapshot(); } catch (e) { thumb = null; } }
    try { await saveWorld(this.world, thumb); } catch (e) { console.warn(e); }
  }

  bindLifecycle() {
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) { this.save(false); this.running = false; } else { this.running = true; this.lastT = performance.now(); }
    });
    window.addEventListener('pagehide', () => this.save(false));
    window.__cvSave = () => this.save(false);
    window.__cvPause = () => { this.save(false); this.running = false; };
    window.__cvResume = () => { this.running = true; this.lastT = performance.now(); };
    window.cvBack = () => this.back();
    window.addEventListener('keydown', (e) => { if (e.key === 'Escape') this.back(); });
  }

  // Bouton retour Android : ferme l'élément au premier plan. Renvoie true si géré.
  back() {
    const top = this.overlays.pop();
    if (top) { top(); return true; }
    if (this.state.placing || this.state.movingObj) { this.state.placing = null; this.state.movingObj = null; this.updateHint(); return true; }
    if (this.state.selectedChar || this.state.selectedCube) { this.closeSheet(); return true; }
    if (this.mode === 'play') { this.save(true); this.showMenu(); return true; }
    return false;
  }

  // ---------------- menu principal ----------------
  showMenu() {
    this.mode = 'menu';
    this.closeSheet(true); $('#hud')?.remove(); $('#menu')?.remove();
    const m = el(`<div id="menu">
      <div class="logo">${APP_LOGO_SVG}<h1>CUBI<span>VERSE</span></h1><p>Des mondes vivants dans des cubes</p></div>
      <div class="menu-btns">
        <button class="mbtn primary" data-act="play"><span class="ic">▶</span><div style="text-align:left">Continuer<small>${esc(this.world.name)} · ${this.world.cubes.size} cubes · ${this.world.chars.size} habitants</small></div></button>
        <button class="mbtn" data-act="new"><span class="ic">✨</span>Nouvel univers</button>
        <button class="mbtn" data-act="gallery"><span class="ic">🖼️</span>Galerie des univers</button>
        <div class="grid2"><button class="mbtn" data-act="catalog"><span class="ic">🧊</span>Catalogue</button><button class="mbtn" data-act="settings"><span class="ic">⚙️</span>Paramètres</button></div>
        <div class="credit">UNE CRÉATION <b>Super<span>Essence</span>.</b> STUDIO</div>
      </div></div>`);
    document.body.appendChild(m);
    bindActs(m, {
      play: () => this.enterPlay(),
      new: () => this.newWorldDialog(),
      gallery: () => this.showGallery(),
      catalog: () => this.showCatalog(),
      settings: () => this.showSettings(),
    });
  }

  enterPlay() {
    $('#menu')?.remove(); this.mode = 'play';
    this.buildHud();
    if (!localStorage.getItem('cv_tuto')) {
      try { localStorage.setItem('cv_tuto', '1'); } catch (e) { /* ignore */ }
      toast('Glissez pour tourner · pincez pour zoomer · touchez un cube ou un habitant', 4200);
    }
  }

  async newWorldDialog() {
    const d = this.openScreen('Nouvel univers', `
      <div class="sec">Nom</div><input id="nw-name" maxlength="40" value="Univers ${Math.floor(Math.random() * 900 + 100)}"/>
      <div class="sec">Départ</div>
      <div class="grid2">
        <button class="card theme-card sel" data-act="pick" data-k="starter"><span class="em">🏘️</span><b>Petit village</b><small>4 cubes connectés (maison, parc, restaurant, arcade à l'étage) et 26 habitants.</small></button>
        <button class="card theme-card" data-act="pick" data-k="empty"><span class="em">🧊</span><b>Un seul cube</b><small>Une maison et 4 habitants. Construisez tout le reste.</small></button>
      </div>
      <button class="btn y" style="width:100%;margin-top:18px;padding:15px" data-act="create">Créer l'univers</button>`);
    let kind = 'starter';
    bindActs(d, {
      pick: (b) => { kind = b.dataset.k; $$('.theme-card', d).forEach((x) => x.classList.toggle('sel', x === b)); },
      create: async () => {
        await this.save(true);
        const name = $('#nw-name', d).value.trim() || 'Nouvel univers';
        const w = kind === 'starter' ? starterWorld(name) : emptyWorld(name);
        this.setWorld(w); await this.save(false);
        this.closeScreen(d); this.showMenu(); this.enterPlay();
        toast(`Univers « ${name} » créé`);
      },
    });
  }

  // ---------------- HUD ----------------
  buildHud() {
    $('#hud')?.remove();
    const h = el(`<div id="hud">
      <div class="topbar">
        <button class="chip icon" data-act="menu" aria-label="Menu">☰</button>
        <div class="chip title-chip"><div style="min-width:0"><div class="t" id="h-name"></div><div class="sub" id="h-time"></div></div></div>
        <div class="chip speed" id="h-speed">
          <button data-act="speed" data-v="0" aria-label="Pause">❚❚</button><button data-act="speed" data-v="1">1×</button><button data-act="speed" data-v="3">3×</button><button data-act="speed" data-v="8">8×</button>
        </div>
        <button class="chip icon" id="h-mode" data-act="mode" aria-label="Mode édition">✏️</button>
      </div>
      <div class="statsline" id="h-stats"></div>
      <div class="edit-hint hidden" id="h-hint"></div>
      <div class="cutbox"><button class="chip" id="h-cut" data-act="cut">🏢 Tous</button></div>
      <div class="bottombar">
        <button class="bb" data-act="chars">👥<span>Habitants</span></button>
        <button class="bb" data-act="history">📜<span>Histoire</span></button>
        <button class="bb" data-act="addcube">🧊<span>Cubes</span></button>
        <button class="bb" data-act="center">🎯<span>Centrer</span></button>
        <button class="bb" data-act="save">💾<span>Sauver</span></button>
      </div></div>`);
    document.body.appendChild(h);
    bindActs(h, {
      menu: () => { this.save(true); this.showMenu(); },
      speed: (b) => { this.sim.speed = +b.dataset.v; this.updateHud(); },
      mode: () => this.toggleEdit(),
      chars: () => this.showCharacters(),
      history: () => this.showHistory(),
      addcube: () => this.showCatalog(),
      center: () => this.centerView(),
      save: async () => { await this.save(true); toast('Univers sauvegardé'); },
      cut: () => this.cycleCut(),
    });
    this.updateHud(); this.updateHint();
  }

  updateHud(info) {
    if (this.mode !== 'play' || !$('#hud')) return;
    const w = this.world;
    $('#h-name').textContent = w.name;
    $('#h-time').textContent = `${fmtTime(w, w.time)} ${w.isNight ? '🌙' : '☀️'} · ${w.events.length ? '🎉 ' + w.events.map((e) => EVENT_LABELS[e.type]).join(', ') : ''}`;
    $$('#h-speed button').forEach((b) => b.classList.toggle('on', +b.dataset.v === this.sim.speed));
    $('#h-mode').classList.toggle('on', this.state.editMode);
    $('#h-stats').textContent = `${w.cubes.size} cubes · ${w.chars.size} habitants · ${this.sim.stats.active} cubes actifs (${this.sim.stats.activeChars} simulés) · ${this.fps} i/s`;
    $('#h-cut').textContent = this.r3d.cutY >= 999 ? '🏢 Tous' : `🏢 ≤ ${this.r3d.cutY}`;
  }

  updateHint() {
    const h = $('#h-hint'); if (!h) return;
    let txt = '';
    if (this.state.placing) txt = `Touchez le sol du cube pour poser : ${OBJECT_TYPES[this.state.placing].label}`;
    else if (this.state.movingObj) txt = 'Touchez le nouvel emplacement de l\'objet';
    else if (this.state.editMode) txt = this.state.selectedCube ? 'Glissez le cube pour le déplacer · touchez ➕ pour en ajouter' : 'Mode édition : touchez un cube';
    h.textContent = txt; h.classList.toggle('hidden', !txt);
  }

  toggleEdit() {
    this.state.editMode = !this.state.editMode;
    this.state.placing = null; this.state.movingObj = null; this.state.selectedObj = null;
    if (this.state.selectedCube) this.openCubeSheet(this.world.cubes.get(this.state.selectedCube));
    this.updateHud(); this.updateHint();
    toast(this.state.editMode ? 'Mode édition activé' : 'Mode observation');
  }

  cycleCut() {
    let maxY = 0; for (const c of this.world.cubes.values()) maxY = Math.max(maxY, c.y);
    if (maxY === 0) { toast('Aucun étage : empilez des cubes pour utiliser la vue en coupe'); return; }
    if (this.r3d.cutY >= 999) this.r3d.cutY = maxY - 1; else if (this.r3d.cutY <= 0) this.r3d.cutY = 999; else this.r3d.cutY--;
    this.updateHud();
  }

  centerView() {
    const c = this.state.selectedCube && this.world.cubes.get(this.state.selectedCube);
    if (c) { this.r3d.focusCube(c); return; }
    let sx = 0, sy = 0, sz = 0, n = 0, maxD = 0;
    for (const cb of this.world.cubes.values()) { sx += cb.x; sy += cb.y; sz += cb.z; n++; }
    const cx = sx / n, cz = sz / n;
    for (const cb of this.world.cubes.values()) maxD = Math.max(maxD, Math.hypot(cb.x - cx, cb.z - cz));
    this.r3d.focusPoint(new THREE.Vector3(cx * CUBE, (sy / n) * CUBE + 1, cz * CUBE), Math.max(10, maxD * CUBE * 1.8 + 8));
  }

  // ---------------- interactions tactiles ----------------
  bindPointer() {
    const cv = this.canvas;
    let down = null; let lastTap = 0; let drag = null;
    cv.addEventListener('pointerdown', (e) => {
      down = { x: e.clientX, y: e.clientY, t: performance.now(), id: e.pointerId, n: (down && down.n) || 0 };
      if (this.mode !== 'play') return;
      // glisser un cube en mode édition
      if (this.state.editMode && this.state.selectedCube && !this.state.placing && !this.state.movingObj && e.isPrimary) {
        const hit = this.r3d.pick(this.world, e.clientX, e.clientY, true);
        if (hit && hit.type === 'cube' && hit.cube.id === this.state.selectedCube) {
          drag = { cube: hit.cube, start: [e.clientX, e.clientY], active: false };
          this.r3d.controls.enabled = false;
        }
      }
    }, { capture: true });
    cv.addEventListener('pointermove', (e) => {
      if (!drag) return;
      if (!drag.active && Math.hypot(e.clientX - drag.start[0], e.clientY - drag.start[1]) > 10) drag.active = true;
      if (drag.active) {
        const p = this.r3d.groundPoint(e.clientX, e.clientY, drag.cube.y * CUBE);
        if (p) {
          const gx = Math.round(p[0] / CUBE), gz = Math.round(p[1] / CUBE);
          drag.target = [gx, drag.cube.y, gz];
          const sel = this.r3d.cubes.sel; sel.position.set(gx * CUBE, drag.cube.y * CUBE + CUBE / 2, gz * CUBE);
          const free = (gx === drag.cube.x && gz === drag.cube.z) || this.world.isFree(gx, drag.cube.y, gz);
          sel.material.color.set(free ? 0x5ad48a : 0xff6b6b);
        }
      }
    });
    const up = (e) => {
      if (drag) {
        this.r3d.controls.enabled = true;
        this.r3d.cubes.sel.material.color.set(0xffd23f);
        if (drag.active && drag.target) {
          const [x, y, z] = drag.target;
          if (x !== drag.cube.x || z !== drag.cube.z) {
            if (this.world.moveCube(drag.cube.id, x, y, z)) toast('Cube déplacé'); else toast('Emplacement occupé');
          }
          drag = null; down = null; return;
        }
        drag = null;
      }
      if (!down || this.mode !== 'play') return;
      const dt = performance.now() - down.t; const dist = Math.hypot(e.clientX - down.x, e.clientY - down.y);
      if (dt < 400 && dist < 10) {
        const now = performance.now(); const dbl = now - lastTap < 320; lastTap = now;
        this.onTap(e.clientX, e.clientY, dbl);
      }
      down = null;
    };
    cv.addEventListener('pointerup', up);
    cv.addEventListener('pointercancel', () => { if (drag) { this.r3d.controls.enabled = true; drag = null; } down = null; });
    // tout geste manuel arrête le suivi de caméra
    this.r3d.controls.addEventListener('start', () => { if (!drag) { this.r3d.flyTo = null; } });
  }

  onTap(x, y, dbl) {
    const s = this.state; const w = this.world;
    const hit = this.r3d.pick(w, x, y, s.editMode);
    if (!hit) { if (!s.editMode) this.closeSheet(); return; }
    if (hit.type === 'ghost') {
      const [gx, gy, gz] = hit.grid;
      const src = w.cubes.get(s.selectedCube);
      const c = w.addCube(gx, gy, gz, src ? src.theme : 'maison');
      if (c) { w.addLog('construction', '🧊', `Nouveau cube « ${c.name} » ajouté`, c.id); s.selectedCube = c.id; this.openCubeSheet(c); toast('Cube ajouté et connecté'); }
      return;
    }
    if ((s.placing || s.movingObj) && hit.type === 'cube') {
      if (!hit.local) { toast('Touchez le sol à l\'intérieur du cube'); return; }
      const [lx, lz] = hit.local;
      if (s.placing) {
        const r = Math.atan2(-lx, -lz);
        w.addObject(hit.cube.id, s.placing, lx, lz, Math.round(r / (Math.PI / 4)) * (Math.PI / 4));
        toast(`${OBJECT_TYPES[s.placing].label} posé`); s.placing = null;
      } else {
        const { cubeId, objId } = s.movingObj; const cb = w.cubes.get(cubeId); const o = cb && cb.objects.find((q) => q.id === objId);
        if (o && hit.cube.id === cubeId) { o.x = clamp(lx, -1.75, 1.75); o.z = clamp(lz, -1.75, 1.75); w.touch(); toast('Objet déplacé'); }
        else if (o) { w.removeObject(cubeId, objId); w.addObject(hit.cube.id, o.type, lx, lz, o.r); toast('Objet déplacé dans un autre cube'); }
        s.movingObj = null;
      }
      this.updateHint(); this.openCubeSheet(hit.cube); return;
    }
    if (hit.type === 'char') {
      this.openCharSheet(hit.ch);
      if (dbl) this.r3d.followId = hit.ch.id;
      return;
    }
    if (hit.type === 'cube') {
      if (s.editMode && hit.obj && s.selectedCube === hit.cube.id) { s.selectedObj = hit.obj.id; this.openCubeSheet(hit.cube); return; }
      s.selectedObj = null;
      if (dbl || s.selectedCube === hit.cube.id) this.r3d.focusCube(hit.cube);
      this.openCubeSheet(hit.cube);
      return;
    }
    this.closeSheet();
  }

  // ---------------- panneau (bottom sheet) ----------------
  sheet(title, bodyHtml, onBind, key) {
    let s = $('#sheet');
    if (!s) {
      s = el(`<div id="sheet" class="sheet closed"><div class="grip"></div><div class="sheet-head"><h2></h2><button class="xbtn" data-x>✕</button></div><div class="sheet-body"></div></div>`);
      document.body.appendChild(s);
      s.querySelector('[data-x]').addEventListener('click', () => this.closeSheet());
      // glisser vers le bas pour fermer
      let sy = null;
      s.querySelector('.grip').addEventListener('pointerdown', (e) => { sy = e.clientY; });
      s.addEventListener('pointerup', (e) => { if (sy !== null && e.clientY - sy > 60) this.closeSheet(); sy = null; });
      requestAnimationFrame(() => s.classList.remove('closed'));
    } else s.classList.remove('closed');
    const body = s.querySelector('.sheet-body');
    const keep = s.dataset.key === key ? body.scrollTop : 0;
    s.dataset.key = key || '';
    s.querySelector('h2').innerHTML = title;
    body.innerHTML = bodyHtml;
    body.scrollTop = keep;
    if (body._handlers) body.removeEventListener('click', body._handlers);
    const handlers = {}; onBind && onBind(handlers, body);
    body._handlers = (e) => { const b = e.target.closest('[data-act]'); if (b && handlers[b.dataset.act]) { e.stopPropagation(); handlers[b.dataset.act](b, e); } };
    body.addEventListener('click', body._handlers);
    return body;
  }
  closeSheet(instant) {
    const s = $('#sheet');
    this.state.selectedCube = null; this.state.selectedChar = null; this.state.selectedObj = null; this.r3d.followId = null;
    this.sheetKind = null;
    if (s) { if (instant) s.remove(); else { s.classList.add('closed'); setTimeout(() => { if (s.classList.contains('closed')) s.remove(); }, 300); } }
    this.updateHint();
  }
  refreshSheet() {
    if (this.mode !== 'play') return;
    if (this.sheetKind === 'char' && this.state.selectedChar) {
      const ch = this.world.chars.get(this.state.selectedChar);
      if (ch) this.openCharSheet(ch, true); else this.closeSheet();
    } else if (this.sheetKind === 'cube' && this.state.selectedCube && !this.state.editMode) {
      const c = this.world.cubes.get(this.state.selectedCube); if (c) this.openCubeSheet(c, true);
    }
  }

  openCubeSheet(cube, refresh = false) {
    if (!cube) return;
    if (document.activeElement && document.activeElement.tagName === 'INPUT' && refresh) return;
    const w = this.world; const s = this.state;
    s.selectedCube = cube.id; s.selectedChar = null; this.sheetKind = 'cube';
    if (!refresh) this.r3d.followId = null;
    const th = THEMES[cube.theme];
    const occ = [...cube.chars].map((id) => w.chars.get(id)).filter(Boolean);
    const evs = w.events.filter((e) => e.cubeId === cube.id);
    const title = `${th.icon} ${esc(cube.name)}`;
    let html;
    if (!s.editMode) {
      html = `<div class="row" style="margin-bottom:8px"><span class="tag">${th.label}</span><span class="tag">${occ.length}/${MAX_PER_CUBE} habitants</span><span class="tag">${cube.awake ? '● simulation complète' : '○ en veille'}</span><span class="tag">x${cube.x} y${cube.y} z${cube.z}</span></div>
        ${evs.length ? `<div class="card" style="margin:8px 0">🎉 En cours : ${evs.map((e) => EVENT_LABELS[e.type]).join(', ')}</div>` : ''}
        <div class="grid3" style="margin:10px 0"><button class="btn y" data-act="edit">✏️ Éditer</button><button class="btn" data-act="addChar" ${occ.length >= MAX_PER_CUBE ? 'disabled' : ''}>➕ Habitant</button><button class="btn" data-act="focus">🎯 Zoomer</button></div>
        <div class="sec">Habitants</div>
        ${occ.length ? occ.map((ch) => this.charRow(ch)).join('') : '<div class="empty">Personne ici pour le moment.</div>'}
        <div class="sec">Objets (${cube.objects.length})</div>
        <div class="wrap">${cube.objects.map((o) => `<span class="tag">${OBJECT_TYPES[o.type].label}${o.state.dirty ? ' (sale)' : ''}${o._users && o._users.size ? ' · ' + o._users.size + ' 👤' : ''}</span>`).join('')}</div>`;
    } else html = this.cubeEditHtml(cube);
    this.sheet(title, html, (H) => {
      H.edit = () => { s.editMode = true; this.openCubeSheet(cube); this.updateHud(); this.updateHint(); };
      H.focus = () => this.r3d.focusCube(cube, 6);
      H.char = (b) => { const ch = w.chars.get(b.dataset.id); if (ch) this.openCharSheet(ch); };
      H.addChar = () => { const ch = w.createChar(cube.id); if (ch) { w.addLog('naissance', '👶', `${ch.name} arrive dans « ${cube.name} »`, cube.id, [ch.id]); toast(`${ch.name} a rejoint le cube`); this.openCubeSheet(cube); } else toast('Cube plein (32 habitants maximum)'); };
      this.bindCubeEdit(H, cube);
    }, 'cube:' + cube.id + ':' + s.editMode);
    this.updateHint();
  }

  charRow(ch) {
    const e = dominantEmotion(ch);
    return `<div class="list-item" data-act="char" data-id="${ch.id}"><div class="dot" style="background:${ch.color}">${EMO_ICON[e] || '🙂'}</div>
      <div class="li-main"><b>${esc(ch.name)}</b><small>${esc(ch.act ? ch.act.label : (this.world.cubes.get(ch.cubeId)?.awake ? 'réfléchit…' : 'vit sa vie (cube en veille)'))}</small></div>
      <span class="tag">${Math.round(mood(ch) * 100)}%</span></div>`;
  }

  cubeEditHtml(cube) {
    const w = this.world; const s = this.state;
    const selObj = s.selectedObj && cube.objects.find((o) => o.id === s.selectedObj);
    const linkBtn = (side) => {
      const adj = w.adjacent(cube, side); const on = cube.links[side];
      const st = adj ? (on && adj.links[{ px: 'nx', nx: 'px', pz: 'nz', nz: 'pz', py: 'ny', ny: 'py' }[side]] ? 'connecté' : 'fermé') : (on ? 'ouvert' : 'fermé');
      return `<button class="btn ${on ? '' : 'off'}" data-act="link" data-side="${side}">${SIDE_LABELS[side]}<span class="st">${st}</span></button>`;
    };
    return `
      ${selObj ? `<div class="card" style="margin-bottom:10px;border-color:var(--yellow)"><div class="row"><b style="flex:1">Objet : ${OBJECT_TYPES[selObj.type].label}</b></div>
        <div class="grid3" style="margin-top:8px"><button class="btn sm" data-act="objRot">⟳ Tourner</button><button class="btn sm" data-act="objMove">✥ Déplacer</button><button class="btn sm red" data-act="objDel">🗑 Retirer</button></div></div>` : ''}
      <div class="grid3"><button class="btn" data-act="rename">✏️ Nom</button><button class="btn" data-act="dup">⧉ Dupliquer</button><button class="btn red" data-act="del">🗑 Supprimer</button></div>
      <div class="sec">Thème</div>
      <div class="grid4">${THEME_KEYS.map((k) => `<button class="btn sm ${k === cube.theme ? 'on' : ''}" data-act="theme" data-k="${k}" title="${THEMES[k].label}">${THEMES[k].icon}<span style="font-size:10px">${THEMES[k].label.split(' ')[0]}</span></button>`).join('')}</div>
      <div class="sec">Couleur du sol</div>
      <div class="colors"><button data-act="tint" data-c="" style="background:${THEMES[cube.theme].floor}" class="${!cube.tint ? 'on' : ''}" title="Couleur du thème"></button>${PALETTE.map((c) => `<button data-act="tint" data-c="${c}" style="background:${c}" class="${cube.tint === c ? 'on' : ''}"></button>`).join('')}</div>
      <div class="sec">Connexions (6 côtés)</div>
      <div class="links">${['nx', 'px', 'nz', 'pz', 'py', 'ny'].map(linkBtn).join('')}</div>
      <div class="sec">Déplacer le cube (ou glissez-le)</div>
      <div class="row" style="gap:16px">
        <div class="sidepad"><span></span><button class="btn" data-act="mv" data-d="f">▲</button><span></span><button class="btn" data-act="mv" data-d="l">◀</button><button class="btn" data-act="focus2">🎯</button><button class="btn" data-act="mv" data-d="r">▶</button><span></span><button class="btn" data-act="mv" data-d="b">▼</button><span></span></div>
        <div style="display:flex;flex-direction:column;gap:6px"><button class="btn" data-act="mv" data-d="u">⤒ Monter</button><button class="btn" data-act="mv" data-d="dn">⤓ Descendre</button></div>
      </div>
      <div class="sec">Habitants (${cube.chars.size}/${MAX_PER_CUBE})</div>
      <div class="grid3"><button class="btn" data-act="addChar">➕ 1</button><button class="btn" data-act="add5">➕ 5</button><button class="btn" data-act="fill">Remplir (32)</button></div>
      <div style="margin-top:8px">${[...cube.chars].slice(0, 32).map((id) => this.charRow(w.chars.get(id))).join('')}</div>
      <div class="sec">Objets (${cube.objects.length}) — touchez un objet dans la vue pour le modifier</div>
      <div class="wrap">${cube.objects.map((o) => `<button class="btn sm ${o.id === s.selectedObj ? 'on' : ''}" data-act="selObj" data-id="${o.id}">${OBJECT_TYPES[o.type].label}</button>`).join('')}</div>
      <div class="sec">Ajouter un objet</div>
      <div class="grid3">${OBJECT_KEYS.map((k) => `<button class="btn sm" data-act="place" data-k="${k}">${OBJECT_TYPES[k].label}</button>`).join('')}</div>`;
  }

  bindCubeEdit(H, cube) {
    const w = this.world; const s = this.state;
    const reopen = () => this.openCubeSheet(cube);
    H.rename = async () => { const n = await promptBox('Nom du cube', cube.name); if (n) { cube.name = n; w.touch(); reopen(); } };
    H.dup = () => { const c = w.duplicateCube(cube.id); if (c) { toast('Cube dupliqué (objets + habitants)'); s.selectedCube = c.id; this.openCubeSheet(c); this.r3d.focusCube(c); } };
    H.del = async () => {
      if (!(await confirmBox(`Supprimer « ${cube.name} » et ses ${cube.chars.size} habitants ?`, 'Supprimer'))) return;
      w.addLog('construction', '🗑️', `Le cube « ${cube.name} » a été retiré`, null);
      w.removeCube(cube.id); this.closeSheet(); toast('Cube supprimé');
    };
    H.theme = (b) => { w.setTheme(cube.id, b.dataset.k); s.selectedObj = null; reopen(); };
    H.tint = (b) => { cube.tint = b.dataset.c || null; w.touch(); reopen(); };
    H.link = (b) => {
      const side = b.dataset.side; const on = !cube.links[side];
      w.setLink(cube, side, on);
      if (!on) for (const id of cube.chars) { const ch = w.chars.get(id); if (ch && ch.act && ch.act.path) abortAct(w, ch); }
      toast(`${SIDE_LABELS[side]} : ${on ? 'ouvert' : 'fermé'}`); reopen();
    };
    H.mv = (b) => {
      const d = b.dataset.d; let dx = 0, dy = 0, dz = 0;
      if (d === 'u') dy = 1; else if (d === 'dn') dy = -1;
      else {
        // directions relatives à la caméra
        const f = this.r3d.controls.target.clone().sub(this.r3d.camera.position); f.y = 0; f.normalize();
        const fx = Math.abs(f.x) > Math.abs(f.z) ? [Math.sign(f.x), 0] : [0, Math.sign(f.z)];
        const rt = [-fx[1], fx[0]];
        const v = d === 'f' ? fx : d === 'b' ? [-fx[0], -fx[1]] : d === 'r' ? [-rt[0], -rt[1]] : rt;
        dx = v[0]; dz = v[1];
      }
      if (w.moveCube(cube.id, cube.x + dx, cube.y + dy, cube.z + dz)) reopen(); else toast(cube.y + dy < 0 ? 'Impossible de descendre sous le sol' : 'Emplacement occupé');
    };
    H.focus2 = () => this.r3d.focusCube(cube, 7);
    H.add5 = () => { let n = 0; for (let i = 0; i < 5; i++) if (w.createChar(cube.id)) n++; toast(n ? `${n} habitant(s) ajouté(s)` : 'Cube plein'); reopen(); };
    H.fill = () => { let n = 0; while (w.createChar(cube.id)) n++; toast(n ? `${n} habitant(s) ajouté(s) — cube plein` : 'Cube déjà plein'); reopen(); };
    H.selObj = (b) => { s.selectedObj = b.dataset.id; reopen(); };
    H.place = (b) => { s.placing = b.dataset.k; s.movingObj = null; this.updateHint(); toast('Touchez le sol du cube'); };
    H.objRot = () => { const o = cube.objects.find((x) => x.id === s.selectedObj); if (o) { o.r += Math.PI / 4; w.touch(); reopen(); } };
    H.objMove = () => { s.movingObj = { cubeId: cube.id, objId: s.selectedObj }; s.placing = null; this.updateHint(); };
    H.objDel = () => { w.removeObject(cube.id, s.selectedObj); s.selectedObj = null; toast('Objet retiré'); reopen(); };
  }

  // ---------------- fiche personnage ----------------
  openCharSheet(ch, refresh = false) {
    if (refresh && document.activeElement && ['INPUT', 'SELECT'].includes(document.activeElement.tagName)) return;
    const w = this.world; const s = this.state;
    s.selectedChar = ch.id; s.selectedCube = null; this.sheetKind = 'char';
    const cube = w.cubes.get(ch.cubeId);
    const e = dominantEmotion(ch); const m = mood(ch);
    const best = bestFriendId(ch);
    const rels = Object.entries(ch.rel).filter(([id]) => w.chars.has(id)).sort((a, b) => Math.abs(b[1].a) + b[1].f * 0.3 - (Math.abs(a[1].a) + a[1].f * 0.3)).slice(0, 14);
    const habits = Object.entries(ch.habits).sort((a, b) => b[1].n - a[1].n).slice(0, 6);
    const slotLbl = ['la nuit', 'le matin tôt', 'le matin', "l'après-midi", 'en fin de journée', 'le soir'];
    const grp = groupOf(w, ch.id);
    const skills = Object.entries(ch.skills).filter(([, v]) => v > 0.05).sort((a, b) => b[1] - a[1]);
    const bar = (v, color) => `<div class="bar"><i style="width:${Math.round(v * 100)}%;background:${color || (v > 0.6 ? 'var(--good)' : v > 0.3 ? 'var(--yellow)' : 'var(--bad)')}"></i></div>`;
    const edit = s.editMode;
    const same = cube ? [...cube.chars].filter((id) => id !== ch.id && !ch.rel[id]).map((id) => w.chars.get(id)) : [];
    const html = `
      <div class="row" style="margin-bottom:10px"><div class="dot" style="background:${ch.color};width:48px;height:48px;font-size:24px">${EMO_ICON[e]}</div>
        <div class="li-main"><b style="font-size:16px">${esc(ch.act ? ch.act.label : '…')}</b><small>${cube ? `${THEMES[cube.theme].icon} ${esc(cube.name)}` : ''} · ${personalitySummary(ch)}</small><small>Humeur ${Math.round(m * 100)}% · ${EMOTION_LABELS[e] || 'Sereine'}${grp ? ` · 👥 ${esc(grp.name)}` : ''}</small></div></div>
      <div class="grid4"><button class="btn sm ${this.r3d.followId === ch.id ? 'on' : ''}" data-act="follow">🎥 Suivre</button><button class="btn sm" data-act="rename">✏️ Nom</button><button class="btn sm" data-act="color">🎨</button><button class="btn sm red" data-act="delete">🗑</button></div>
      ${s.selectedCube !== null ? '' : ''}
      <div class="sec">Besoins</div>
      ${Object.keys(NEED_LABELS).map((k) => `<div class="kv"><span>${NEED_LABELS[k]}</span>${bar(ch.needs[k])}<span class="n">${Math.round(ch.needs[k] * 100)}</span></div>`).join('')}
      <div class="sec">Émotions</div>
      ${Object.keys(EMOTION_LABELS).map((k) => `<div class="kv"><span>${EMO_ICON[k]} ${EMOTION_LABELS[k]}</span>${bar(ch.emo[k], '#9b8cff')}<span class="n">${Math.round(ch.emo[k] * 100)}</span></div>`).join('')}
      <div class="sec">Personnalité ${edit ? '(modifiable)' : ''}</div>
      ${TRAITS.map((k) => edit ? `<div class="kv"><span>${TRAIT_LABELS[k]}</span><input type="range" min="0" max="100" value="${Math.round(ch.traits[k] * 100)}" data-trait="${k}"/><span class="n">${Math.round(ch.traits[k] * 100)}</span></div>` : `<div class="kv"><span>${TRAIT_LABELS[k]}</span>${bar(ch.traits[k], '#59b7ff')}<span class="n">${Math.round(ch.traits[k] * 100)}</span></div>`).join('')}
      <div class="sec">Relations (${Object.keys(ch.rel).length})</div>
      ${rels.length ? rels.map(([id, r]) => { const o = w.chars.get(id); const lbl = relLabel(r, id === best); const cls = lbl === 'Meilleur ami' ? 'best' : lbl === 'Ami' ? 'friend' : lbl === 'Rival' ? 'rival' : '';
        return `<div class="list-item"><div class="dot" style="background:${o.color}"></div><div class="li-main" data-act="goto" data-id="${id}"><b>${esc(o.name)}</b><small>Affinité ${Math.round(r.a)} · ${r.n} rencontre(s)</small></div>${edit ? `<div class="seg"><button data-act="setRel" data-id="${id}" data-v="-70" class="${r.a <= -35 ? 'on' : ''}">⚡</button><button data-act="setRel" data-id="${id}" data-v="0" class="${r.a > -35 && r.a < 35 ? 'on' : ''}">·</button><button data-act="setRel" data-id="${id}" data-v="70" class="${r.a >= 35 ? 'on' : ''}">❤️</button></div>` : `<span class="tag ${cls}">${lbl}</span>`}</div>`; }).join('') : '<div class="empty">Ne connaît encore personne.</div>'}
      ${edit && same.length ? `<div class="row" style="margin-top:6px"><select id="introduce">${same.map((o) => `<option value="${o.id}">${esc(o.name)}</option>`).join('')}</select><button class="btn sm" data-act="introduce">Présenter</button></div>` : ''}
      <div class="sec">Compétences</div>
      ${skills.length ? skills.map(([k, v]) => `<div class="kv"><span>${SKILL_LABEL[k] || k}</span>${bar(v / 10, '#ffb84d')}<span class="n">${v.toFixed(1)}</span></div>`).join('') : '<div class="empty">Pas encore de compétence.</div>'}
      <div class="sec">Habitudes</div>
      ${habits.length ? habits.map(([k, h]) => { const best = h.h.indexOf(Math.max(...h.h)); return `<div class="mem"><span class="tm">${h.n}×</span><span>${ACT_LABEL[k] || k} — surtout ${slotLbl[best]}</span></div>`; }).join('') : '<div class="empty">Aucune habitude encore.</div>'}
      <div class="sec">Inventaire</div>
      <div class="inv">${ch.inventory.length ? ch.inventory.map((it) => `<span title="${ITEMS[it.type]?.label}">${ITEMS[it.type]?.icon || '❔'}</span>`).join('') : '<span style="font-size:13px;color:var(--muted)">Vide</span>'}</div>
      ${edit ? `<div class="wrap" style="margin-top:8px">${Object.keys(ITEMS).map((k) => `<button class="btn sm" data-act="give" data-k="${k}">+${ITEMS[k].icon}</button>`).join('')}</div>` : ''}
      <div class="sec">Statistiques</div>
      <div class="wrap"><span class="tag">🚶 ${Math.round(ch.stats.steps)} pas</span><span class="tag">🚪 ${ch.stats.crossings} passages</span><span class="tag">🤝 ${ch.stats.friendsMade} amitiés</span><span class="tag">🎁 ${ch.stats.gifts} cadeaux</span><span class="tag">🏆 ${ch.stats.wins} victoires</span><span class="tag">💥 ${ch.stats.fights} bagarres</span><span class="tag">🎂 jour ${ch.bday + 1}/12</span></div>
      <div class="sec">Mémoire</div>
      ${ch.memory.length ? ch.memory.slice().reverse().map((mm) => `<div class="mem"><span class="tm">${fmtTime(w, mm.t)}</span><span>${mm.v > 0 ? '😊' : mm.v < 0 ? '😞' : '•'} ${esc(mm.text)}</span></div>`).join('') : '<div class="empty">Aucun souvenir pour l\'instant.</div>'}`;
    const body = this.sheet(`<span style="color:${ch.color}">●</span> ${esc(ch.name)}`, html, (H) => {
      H.follow = () => { this.r3d.followId = this.r3d.followId === ch.id ? null : ch.id; this.openCharSheet(ch); };
      H.rename = async () => { const n = await promptBox('Nom du personnage', ch.name); if (n) { w.names.delete(ch.name); ch.name = n; w.names.add(n); this.openCharSheet(ch); } };
      H.color = () => { const i = PALETTE.indexOf(ch.color); ch.color = PALETTE[(i + 1) % PALETTE.length]; this.openCharSheet(ch); };
      H.delete = async () => { if (await confirmBox(`Retirer ${ch.name} de l'univers ?`, 'Retirer')) { w.addLog('départ', '👋', `${ch.name} a quitté l'univers`, ch.cubeId); w.removeChar(ch.id); this.closeSheet(); } };
      H.goto = (b) => { const o = w.chars.get(b.dataset.id); if (o) { this.openCharSheet(o); const c = w.cubes.get(o.cubeId); if (c) this.r3d.focusCube(c, 7); } };
      H.setRel = (b) => { const o = w.chars.get(b.dataset.id); const v = +b.dataset.v; const r1 = getRel(ch, o.id), r2 = getRel(o, ch.id); r1.a = r2.a = v; r1.f = Math.max(r1.f, 30); r2.f = Math.max(r2.f, 30); this.openCharSheet(ch); };
      H.introduce = () => { const id = $('#introduce').value; const o = w.chars.get(id); if (o) { const r1 = getRel(ch, id), r2 = getRel(o, ch.id); r1.f = r2.f = Math.max(r1.f, 10); r1.a += 10; r2.a += 10; toast(`${ch.name} et ${o.name} se connaissent maintenant`); this.openCharSheet(ch); } };
      H.give = (b) => { if (ch.inventory.length >= 6) ch.inventory.shift(); ch.inventory.push({ type: b.dataset.k, t: w.time }); this.openCharSheet(ch); };
    }, 'char:' + ch.id + ':' + edit);
    body.querySelectorAll('[data-trait]').forEach((inp) => inp.addEventListener('input', () => { ch.traits[inp.dataset.trait] = +inp.value / 100; inp.nextElementSibling.textContent = inp.value; }));
  }

  // ---------------- écrans plein écran ----------------
  openScreen(title, bodyHtml) {
    const d = el(`<div class="screen"><div class="screen-head"><button class="xbtn" data-close>←</button><h2>${title}</h2></div><div class="screen-body">${bodyHtml}</div></div>`);
    document.body.appendChild(d);
    const close = () => d.remove();
    d.querySelector('[data-close]').addEventListener('click', () => { this.overlays = this.overlays.filter((f) => f !== close); close(); });
    this.overlays.push(close);
    d._close = close;
    return d;
  }
  closeScreen(d) { this.overlays = this.overlays.filter((f) => f !== d._close); d.remove(); }

  showCharacters() {
    const w = this.world;
    let filter = this.state.selectedCube ? 'cube' : 'all', q = '', sort = 'name';
    const cubeId = this.state.selectedCube;
    const d = this.openScreen(`Habitants (${w.chars.size})`, `
      <input id="cq" placeholder="Rechercher un nom…"/>
      <div class="row" style="margin:10px 0;justify-content:space-between">
        <div class="seg" id="cf"><button data-act="f" data-v="all">Tous</button>${cubeId ? '<button data-act="f" data-v="cube">Ce cube</button>' : ''}<button data-act="f" data-v="awake">Visibles</button></div>
        <div class="seg" id="cs"><button data-act="s" data-v="name">A-Z</button><button data-act="s" data-v="mood">Humeur</button><button data-act="s" data-v="friends">Amis</button></div>
      </div>
      <div id="clist"></div>`);
    const render = () => {
      let list = [...w.chars.values()];
      if (filter === 'cube') list = list.filter((c) => c.cubeId === cubeId);
      if (filter === 'awake') list = list.filter((c) => w.cubes.get(c.cubeId)?.awake);
      if (q) list = list.filter((c) => c.name.toLowerCase().includes(q));
      const fc = (c) => Object.values(c.rel).filter((r) => r.a >= 35).length;
      list.sort(sort === 'name' ? (a, b) => a.name.localeCompare(b.name) : sort === 'mood' ? (a, b) => mood(b) - mood(a) : (a, b) => fc(b) - fc(a));
      const shown = list.slice(0, 300);
      $('#clist', d).innerHTML = (shown.length ? shown.map((c) => this.charRow(c)).join('') : '<div class="empty">Aucun habitant trouvé.</div>') + (list.length > 300 ? `<div class="empty">… et ${list.length - 300} autres (affinez la recherche)</div>` : '');
      $$('#cf button', d).forEach((b) => b.classList.toggle('on', b.dataset.v === filter));
      $$('#cs button', d).forEach((b) => b.classList.toggle('on', b.dataset.v === sort));
    };
    $('#cq', d).addEventListener('input', (e) => { q = e.target.value.toLowerCase(); render(); });
    bindActs(d, {
      f: (b) => { filter = b.dataset.v; render(); }, s: (b) => { sort = b.dataset.v; render(); },
      char: (b) => { const ch = w.chars.get(b.dataset.id); this.closeScreen(d); if (ch) { this.openCharSheet(ch); const c = w.cubes.get(ch.cubeId); this.r3d.focusCube(c, 7); } },
    });
    render();
  }

  showHistory() {
    const w = this.world; let f = 'all';
    const d = this.openScreen('Historique', `<div class="chips" id="hf">
      ${[['all', 'Tout'], ['événement', '🎉 Événements'], ['social', '🤝 Social'], ['découverte', '🔎 Découvertes'], ['accident', '💥 Accidents'], ['construction', '🏗️ Construction'], ['restaurant', '🍲 Restaurant'], ['visite', '🚪 Visites'], ['objet', '🎁 Objets']].map(([k, l]) => `<button class="btn sm" data-act="f" data-v="${k}">${l}</button>`).join('')}
      </div><div id="hl"></div>`);
    const render = () => {
      const list = w.log.filter((e) => f === 'all' || e.type === f).slice(-400).reverse();
      $('#hl', d).innerHTML = list.length ? list.map((e, i) => `<div class="log-item" data-act="go" data-c="${e.cubeId || ''}"><span class="ic">${e.icon}</span><div style="flex:1">${esc(e.text)}</div><span class="tm">${fmtTime(w, e.t)}</span></div>`).join('') : '<div class="empty">Rien à afficher pour l\'instant. Laissez vivre votre univers !</div>';
      $$('#hf button', d).forEach((b) => b.classList.toggle('on', b.dataset.v === f));
    };
    bindActs(d, { f: (b) => { f = b.dataset.v; render(); }, go: (b) => { const c = w.cubes.get(b.dataset.c); if (c) { this.closeScreen(d); if (this.mode !== 'play') this.enterPlay(); this.openCubeSheet(c); this.r3d.focusCube(c); } } });
    render();
  }

  showCatalog() {
    const w = this.world;
    let kind = 'row', sel = null;
    const d = this.openScreen('Catalogue des cubes', `
      <div style="color:var(--muted);font-size:13px">Choisissez un thème : le cube est ajouté et connecté ${this.state.selectedCube ? 'à côté du cube sélectionné' : 'près de la zone regardée'}.</div>
      <div class="grid2" style="margin-top:12px">${THEME_KEYS.map((k) => { const t = THEMES[k]; return `<button class="card theme-card" data-act="theme" data-k="${k}"><span class="sw" style="background:${t.floor}"></span><span class="em">${t.icon}</span><b>${t.label}</b><small>${t.desc}</small><small style="opacity:.7">${t.objects.map(([o]) => OBJECT_TYPES[o].label).filter((v, i, a) => a.indexOf(v) === i).join(' · ')}</small></button>`; }).join('')}</div>
      <div class="sec">Grandes structures</div>
      <div class="card">
        <div class="seg" id="sk" style="flex-wrap:wrap">${['row', 'tower', 'grid', 'pyramid', 'city'].map((k) => `<button data-act="kind" data-k="${k}">${kindLabel(k)}</button>`).join('')}</div>
        <div class="grid2" style="margin-top:10px"><label>Nombre de cubes<input id="ss" type="number" min="1" value="12"/></label><label>Habitants par cube<input id="sc" type="number" min="0" max="32" value="4"/></label></div>
        <label style="display:block;margin-top:8px">Thème<select id="st"><option value="">Mélange de tous les thèmes</option>${THEME_KEYS.map((k) => `<option value="${k}">${THEMES[k].icon} ${THEMES[k].label}</option>`).join('')}</select></label>
        <button class="btn y" style="width:100%;margin-top:12px" data-act="build">🏗️ Construire</button>
        <div id="sprog" style="margin-top:8px;font-size:13px;color:var(--muted)"></div>
      </div>`);
    const renderKind = () => $$('#sk button', d).forEach((b) => b.classList.toggle('on', b.dataset.k === kind));
    renderKind();
    bindActs(d, {
      theme: (b) => {
        const theme = b.dataset.k;
        if (this.mode !== 'play') { this.closeScreen(d); $('#menu')?.remove(); this.enterPlay(); }
        const anchor = this.state.selectedCube ? w.cubes.get(this.state.selectedCube) : this.nearestCube();
        const spot = anchor ? w.findFreeNear(anchor.x, anchor.y, anchor.z) : [0, 0, 0];
        const c = w.addCube(spot[0], spot[1], spot[2], theme);
        if (!c) { toast('Impossible d\'ajouter ici'); return; }
        for (let i = 0; i < 3; i++) w.createChar(c.id);
        w.addLog('construction', THEMES[theme].icon, `Nouveau cube « ${c.name} » avec 3 habitants`, c.id);
        if (d.isConnected) this.closeScreen(d);
        this.openCubeSheet(c); this.r3d.focusCube(c);
        toast(`${THEMES[theme].label} ajouté`);
      },
      kind: (b) => { kind = b.dataset.k; renderKind(); },
      build: () => {
        const n = Math.max(1, Math.floor(+$('#ss', d).value || 1)); const per = clamp(Math.floor(+$('#sc', d).value || 0), 0, 32); const theme = $('#st', d).value;
        this.buildAsync(kind, n, per, theme, (done, total) => { const p = $('#sprog', d); if (p) p.textContent = done < total ? `Construction… ${done}/${total} cubes` : `Terminé : ${total} cubes construits.`; });
      },
    });
  }

  nearestCube() {
    const t = this.r3d.controls.target; let best = null, bd = Infinity;
    for (const c of this.world.cubes.values()) { const d = (c.x * CUBE - t.x) ** 2 + (c.z * CUBE - t.z) ** 2 + (c.y * CUBE - t.y) ** 2; if (d < bd) { bd = d; best = c; } }
    return best;
  }

  // Construction par tranches pour ne jamais bloquer l'interface.
  buildAsync(kind, n, per, theme, progress) {
    const w = this.world;
    const cells = structureCells(w, kind, n);
    let i = 0; let first = null; const total = cells.length;
    const themes = theme ? [theme] : THEME_KEYS;
    const tick = () => {
      const t0 = performance.now();
      while (i < total && performance.now() - t0 < 12) {
        const [x, y, z] = cells[i++];
        const c = w.addCube(x, y, z, pick(themes));
        if (c) { first = first || c; for (let k = 0; k < per; k++) w.createChar(c.id); }
      }
      progress(i, total);
      if (i < total) setTimeout(tick, 0);
      else {
        if (first) { w.addLog('construction', '🏗️', `Nouvelle structure (${kindLabel(kind)}) : ${total} cubes`, first.id); this.r3d.focusCube(first, 14); }
        toast(`${kindLabel(kind)} de ${total} cubes construite`);
      }
    };
    tick();
  }

  showSettings() {
    const s = this.settings;
    const seg = (key, opts) => `<div class="seg">${opts.map(([v, l]) => `<button data-act="set" data-k="${key}" data-v="${v}" class="${String(s[key]) === String(v) ? 'on' : ''}">${l}</button>`).join('')}</div>`;
    const d = this.openScreen('Paramètres', `
      <div class="sec">Graphismes</div>
      <div class="setting"><div>Style d'affichage<small>Rétro pixel : cubes en plastique avec écran · 3D : maisons de poupée en coupe</small></div>${seg('style', [['pixel', 'Rétro pixel'], ['3d', '3D']])}</div>
      <div class="setting"><div>Écrans animés<small>Nombre de cubes proches dont l'écran est animé</small></div>${seg('maxScreens', [[24, '24'], [48, '48'], [96, '96']])}</div>
      <div class="setting"><div>Qualité<small>Résolution et ombres plus fines</small></div>${seg('quality', [['low', 'Éco'], ['medium', 'Normal'], ['high', 'Haute']])}</div>
      <div class="setting"><div>Ombres dynamiques</div><input type="checkbox" data-chk="shadows" ${s.shadows ? 'checked' : ''}/></div>
      <div class="setting"><div>Bulles d'émotion</div><input type="checkbox" data-chk="bubbles" ${s.bubbles ? 'checked' : ''}/></div>
      <div class="setting"><div>Économie de batterie<small>Limite à 30 images/s</small></div><input type="checkbox" data-chk="battery" ${s.battery ? 'checked' : ''}/></div>
      <div class="setting"><div>Rotation du menu<small>La caméra tourne lentement sur l'écran d'accueil</small></div><input type="checkbox" data-chk="autoRotateMenu" ${s.autoRotateMenu ? 'checked' : ''}/></div>
      <div class="sec">Simulation et performances</div>
      <div class="setting"><div>Rayon de simulation complète<small>Cubes autour de la caméra simulés image par image. Les autres vivent en arrière-plan (simulation simplifiée).</small></div>${seg('simRadius', [[2.2, '2'], [3.2, '3'], [4.5, '4'], [6, '6']])}</div>
      <div class="setting"><div>Cubes actifs maximum</div>${seg('maxActive', [[16, '16'], [48, '48'], [96, '96']])}</div>
      <div class="setting"><div>Distance de détail<small>Au-delà, les cubes sont affichés en version simplifiée</small></div>${seg('detail', [[5, 'Courte'], [8, 'Moyenne'], [14, 'Longue']])}</div>
      <div class="setting"><div>Habitants affichés au maximum</div>${seg('maxChars', [[400, '400'], [900, '900'], [2000, '2000']])}</div>
      <div class="sec">Univers actuel</div>
      <div class="grid2"><button class="btn" data-act="rename">✏️ Renommer</button><button class="btn" data-act="export">📤 Exporter</button><label class="btn">📥 Importer<input type="file" accept=".json,application/json" id="imp" class="hidden"/></label><button class="btn red" data-act="reset">♻️ Réinitialiser</button></div>
      <div class="sec">À propos</div>
      <div class="card" style="font-size:13px;line-height:1.5">CUBIVERSE 1.0 — une création <b>Super<span style="color:var(--yellow)">Essence</span>. Studio</b>.<br/>Simulation 100 % locale : aucune connexion, aucun abonnement, aucune IA distante. Les habitants décident seuls grâce à un système de besoins, d'émotions, de personnalité, de mémoire et d'habitudes.</div>`);
    const apply = () => { saveSettings(s); this.r3d.applySettings(s); this.applySimSettings(); this.state.forceRebuild = true; };
    bindActs(d, {
      set: (b) => { const k = b.dataset.k; const v = b.dataset.v; s[k] = isNaN(+v) ? v : +v; apply(); $$(`[data-k="${k}"]`, d).forEach((x) => x.classList.toggle('on', x === b)); },
      rename: async () => { const n = await promptBox("Nom de l'univers", this.world.name); if (n) { this.world.name = n; this.save(); toast('Univers renommé'); } },
      export: () => this.exportWorld(),
      reset: async () => {
        if (!(await confirmBox(`Réinitialiser « ${this.world.name} » ? Tout son contenu sera remplacé par un petit village.`, 'Réinitialiser'))) return;
        const id = this.world.id; const name = this.world.name; const w = starterWorld(name); w.id = id; this.setWorld(w); await this.save(); toast('Univers réinitialisé');
      },
    });
    $$('[data-chk]', d).forEach((c) => c.addEventListener('change', () => { s[c.dataset.chk] = c.checked; apply(); }));
    $('#imp', d).addEventListener('change', (e) => this.importWorld(e.target.files[0]));
  }

  exportWorld() {
    const json = JSON.stringify(serialize(this.world));
    const name = `cubiverse-${this.world.name.replace(/[^\w-]+/g, '_')}.json`;
    if (window.CubiverseNative && window.CubiverseNative.shareText) { window.CubiverseNative.shareText(name, json); return; }
    const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([json], { type: 'application/json' })); a.download = name; a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 2000);
    toast('Fichier exporté');
  }
  async importWorld(file) {
    if (!file) return;
    try {
      const data = JSON.parse(await file.text());
      const w = deserialize(data);
      const metas = await listWorlds(); if (metas.some((m) => m.id === w.id)) { w.id = newId('u'); w.name += ' (importé)'; }
      await this.save(true); this.setWorld(w); await this.save(); toast(`Univers « ${w.name} » importé`);
    } catch (e) { toast('Fichier invalide'); }
  }

  async showGallery() {
    await this.save(true);
    const d = this.openScreen('Galerie des univers', '<div id="gl"></div><button class="btn y" style="width:100%;margin-top:6px;padding:14px" data-act="new">✨ Nouvel univers</button>');
    const render = async () => {
      const metas = await listWorlds();
      $('#gl', d).innerHTML = metas.length ? metas.map((m) => `<div class="gal-item">${m.thumb ? `<img src="${m.thumb}" alt=""/>` : '<div class="ph">🧊</div>'}
        <div class="li-main"><b>${esc(m.name)}${m.id === this.world.id ? ' <span class="tag friend">actuel</span>' : ''}</b><small>${m.cubes} cubes · ${m.chars} habitants · jour ${m.day}</small><small>${new Date(m.updated).toLocaleString('fr-FR')}</small>
        <div class="wrap" style="margin-top:6px"><button class="btn sm y" data-act="open" data-id="${m.id}">Ouvrir</button><button class="btn sm" data-act="ren" data-id="${m.id}">✏️</button><button class="btn sm" data-act="dup" data-id="${m.id}">⧉</button><button class="btn sm red" data-act="del" data-id="${m.id}">🗑</button></div></div></div>`).join('') : '<div class="empty">Aucun univers sauvegardé.</div>';
    };
    bindActs(d, {
      open: async (b) => { const w = b.dataset.id === this.world.id ? this.world : await loadWorld(b.dataset.id); if (!w) { toast('Univers introuvable'); return; } this.setWorld(w); await this.save(); this.closeScreen(d); this.showMenu(); this.enterPlay(); },
      ren: async (b) => { const w = b.dataset.id === this.world.id ? this.world : await loadWorld(b.dataset.id); const n = await promptBox("Nom de l'univers", w.name); if (n) { w.name = n; await saveWorld(w); render(); } },
      dup: async (b) => { const src = b.dataset.id === this.world.id ? this.world : await loadWorld(b.dataset.id); const copy = deserialize(JSON.parse(JSON.stringify(serialize(src)))); copy.id = newId('u'); copy.name = src.name + ' (copie)'; await saveWorld(copy); toast('Univers dupliqué'); render(); },
      del: async (b) => {
        if (b.dataset.id === this.world.id) { toast("Impossible de supprimer l'univers ouvert"); return; }
        if (await confirmBox('Supprimer définitivement cet univers ?', 'Supprimer')) { await deleteWorld(b.dataset.id); render(); }
      },
      new: () => { this.closeScreen(d); this.newWorldDialog(); },
    });
    render();
  }
}

const app = new App();
window.__app = app;
app.boot();
