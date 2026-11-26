// Interface tactile : joystick, caméra au doigt, boutons d'action, roue des pouvoirs, mini-carte,
// carte du monde (téléportation), journal, réglages, mode photo, bulles de paroles.
// Clavier et souris pris en charge pour les essais sur ordinateur.
import * as THREE from 'three';
import { G, WORLD, QUALITY, settings, saveSettings, clamp, on, emit } from './core.js';
import { POWERS } from './powers.js';
import { REGIONS } from './terrain.js';
import { HEROES, loadHero } from './heromodel.js';

const $ = (s) => document.querySelector(s);
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; };

export class UI {
  constructor() {
    this.input = { moveX: 0, moveY: 0, up: false, down: false, powerHeld: false };
    this.hud = $('#hud');
    this.journal = [];
    this.build();
    this.bindTouch();
    this.bindKeys();
    on('journal', (j) => this.log(j.icon, j.text));
    on('monster', (m) => this.monsterEvent(m));
    on('godFly', (f) => this.setFlying(f));
  }

  build() {
    const h = this.hud;
    h.innerHTML = `
      <div id="status"><div id="clock"></div><div id="region"></div><div id="rep"><span class="rep-l">Crainte</span><div class="rep-bar"><div class="rep-fear"></div><div class="rep-worship"></div></div><span class="rep-r">Vénération</span></div></div>
      <div id="topright"><canvas id="minimap" width="140" height="140"></canvas><button id="btnMenu" class="round small" aria-label="Menu">☰</button></div>
      <div id="bossbar" class="hidden"><div class="boss-name">Titan de magma</div><div class="boss-track"><div class="boss-fill"></div></div></div>
      <div id="crosshair"></div>
      <div id="toasts"></div>
      <div id="bubbles"></div>
      <div id="joyzone"><div id="joy"><div id="joyknob"></div></div></div>
      <div id="actions">
        <button id="btnAttack" class="round big" aria-label="Poing divin"><span>✊</span><svg class="cd"><circle r="44" cx="50" cy="50"/></svg><div class="charge"></div></button>
        <button id="btnPower" class="round mid" aria-label="Pouvoir"><span id="powerIcon">✺</span><svg class="cd"><circle r="44" cx="50" cy="50"/></svg></button>
        <button id="btnWheel" class="round small" aria-label="Choisir un pouvoir">◈</button>
        <button id="btnJump" class="round mid" aria-label="Saut">⤒</button>
        <button id="btnDown" class="round mid hidden" aria-label="Descendre">⤓</button>
        <button id="btnFly" class="round small" aria-label="Vol">🜁</button>
        <button id="btnInteract" class="round small hidden" aria-label="Interaction">✋</button>
        <button id="btnTeleport" class="round small" aria-label="Téléportation">✦</button>
      </div>
      <div id="interactLabel" class="hidden"></div>
      <div id="wheel" class="hidden"></div>
      <div id="fps" class="hidden"></div>`;
    // roue des pouvoirs
    const wheel = $('#wheel');
    POWERS.forEach((p, i) => {
      const a = -Math.PI / 2 + i / POWERS.length * Math.PI * 2;
      const b = el('button', 'wheel-item', `<span class="wi-icon">${p.icon}</span><span class="wi-name">${p.name}</span>`);
      b.style.left = `calc(50% + ${Math.cos(a) * 130}px)`; b.style.top = `calc(50% + ${Math.sin(a) * 130}px)`;
      b.addEventListener('pointerup', (e) => { e.stopPropagation(); this.selectPower(i); this.toggleWheel(false); });
      wheel.appendChild(b);
    });
    wheel.addEventListener('pointerup', () => this.toggleWheel(false));
    this.selectPower(settings.power || 0);
    // menu
    const menu = el('div', 'hidden', ''); menu.id = 'menu';
    menu.innerHTML = `<div class="menu-panel">
      <div class="menu-tabs"><button data-tab="map" class="on">Carte</button><button data-tab="powers">Pouvoirs</button><button data-tab="journal">Journal</button><button data-tab="hero">Héros</button><button data-tab="settings">Réglages</button></div>
      <div class="menu-body">
        <div data-pane="map"><canvas id="worldmap" width="512" height="512"></canvas><p class="hint">Touchez un lieu pour vous y téléporter.</p></div>
        <div data-pane="powers" class="hidden" id="powerList"></div>
        <div data-pane="journal" class="hidden" id="journalList"></div>
        <div data-pane="hero" class="hidden" id="heroPane"></div>
        <div data-pane="settings" class="hidden" id="settingsPane"></div>
      </div>
      <div class="menu-foot"><button id="btnPhoto">Mode photo</button><button id="btnResume" class="primary">Reprendre</button></div></div>`;
    document.body.appendChild(menu);
    menu.querySelectorAll('.menu-tabs button').forEach((b) => b.addEventListener('click', () => this.tab(b.dataset.tab)));
    $('#btnResume').addEventListener('click', () => this.closeMenu());
    $('#btnPhoto').addEventListener('click', () => { this.closeMenu(); this.photo(true); });
    $('#btnMenu').addEventListener('click', () => this.openMenu('map'));
    $('#worldmap').addEventListener('pointerup', (e) => this.mapTap(e));
    // mode photo
    const ph = el('div', 'hidden', `<button id="phShot" class="round mid">◉</button><input id="phTime" type="range" min="0" max="1000"><button id="phExit" class="round small">✕</button><div id="phMsg"></div>`);
    ph.id = 'photobar'; document.body.appendChild(ph);
    $('#phExit').addEventListener('click', () => this.photo(false));
    $('#phShot').addEventListener('click', () => this.capture());
    $('#phTime').addEventListener('input', (e) => { G.env.dayTime = e.target.value / 1000; });
    this.buildSettings();
  }

  buildSettings() {
    const p = $('#settingsPane');
    const q = Object.entries(QUALITY).map(([k, v]) => `<button data-q="${k}" class="${settings.quality === k ? 'on' : ''}">${v.label}</button>`).join('');
    p.innerHTML = `
      <div class="row"><label>Qualité graphique</label><div class="seg" id="qSeg">${q}</div></div>
      <p class="hint" id="qHint">Le changement de qualité redémarre le monde (votre partie est sauvegardée).</p>
      <div class="row"><label>Sensibilité caméra</label><input type="range" id="sSens" min="30" max="250" value="${settings.sensitivity * 100}"></div>
      <div class="row"><label>Inverser l'axe vertical</label><input type="checkbox" id="sInv" ${settings.invertY ? 'checked' : ''}></div>
      <div class="row"><label>Volume</label><input type="range" id="sVol" min="0" max="100" value="${settings.volume * 100}"></div>
      <div class="row"><label>Vibrations</label><input type="checkbox" id="sHap" ${settings.haptics ? 'checked' : ''}></div>
      <div class="row"><label>Durée d'une journée</label><select id="sDay">${[6, 10, 14, 20, 30].map((m) => `<option value="${m}" ${settings.dayLength === m ? 'selected' : ''}>${m} min</option>`).join('')}</select></div>
      <div class="row"><label>Afficher les images/s</label><input type="checkbox" id="sFps" ${settings.showFps ? 'checked' : ''}></div>
      <div class="row"><label>Nouvelle partie</label><button id="sReset" class="danger">Effacer le monde</button></div>`;
    p.querySelectorAll('#qSeg button').forEach((b) => b.addEventListener('click', () => { settings.quality = b.dataset.q; saveSettings(); emit('save'); setTimeout(() => location.reload(), 300); }));
    $('#sSens').addEventListener('input', (e) => { settings.sensitivity = e.target.value / 100; saveSettings(); });
    $('#sInv').addEventListener('change', (e) => { settings.invertY = e.target.checked; saveSettings(); });
    $('#sVol').addEventListener('input', (e) => { settings.volume = e.target.value / 100; saveSettings(); G.audio && G.audio.setVolume(settings.volume); });
    $('#sHap').addEventListener('change', (e) => { settings.haptics = e.target.checked; saveSettings(); });
    $('#sDay').addEventListener('change', (e) => { settings.dayLength = +e.target.value; saveSettings(); });
    $('#sFps').addEventListener('change', (e) => { settings.showFps = e.target.checked; saveSettings(); $('#fps').classList.toggle('hidden', !settings.showFps); });
    $('#sReset').addEventListener('click', () => { if (confirm('Effacer définitivement ce monde et recommencer ?')) { emit('resetWorld'); } });
    $('#fps').classList.toggle('hidden', !settings.showFps);
  }

  // ---------- entrées tactiles ----------
  bindTouch() {
    const joy = $('#joy'), knob = $('#joyknob'), zone = $('#joyzone');
    let joyId = null, jx = 0, jy = 0;
    const R = 56;
    zone.addEventListener('pointerdown', (e) => {
      if (joyId !== null) return;
      joyId = e.pointerId; zone.setPointerCapture(e.pointerId);
      jx = e.clientX; jy = e.clientY;
      joy.style.left = jx + 'px'; joy.style.top = jy + 'px'; joy.classList.add('on');
      G.audio && G.audio.unlock();
    });
    zone.addEventListener('pointermove', (e) => {
      if (e.pointerId !== joyId) return;
      let dx = e.clientX - jx, dy = e.clientY - jy; const L = Math.hypot(dx, dy);
      if (L > R) { dx = dx / L * R; dy = dy / L * R; }
      knob.style.transform = `translate(${dx}px, ${dy}px)`;
      this.input.moveX = dx / R; this.input.moveY = -dy / R;
    });
    const end = (e) => { if (e.pointerId !== joyId) return; joyId = null; this.input.moveX = this.input.moveY = 0; knob.style.transform = ''; joy.classList.remove('on'); };
    zone.addEventListener('pointerup', end); zone.addEventListener('pointercancel', end);
    // caméra : glisser n'importe où ailleurs (y compris en mode photo)
    const look = $('#lookzone');
    // suivi robuste des doigts : un toucher dont Android a perdu la fin ne doit jamais
    // transformer la rotation en zoom (doigt « fantôme »)
    const ptr = new Map();
    const now = () => performance.now();
    look.addEventListener('pointerdown', (e) => {
      if (e.isPrimary) ptr.clear();
      for (const [id, q] of ptr) if (now() - q.t > 1500) ptr.delete(id);
      ptr.set(e.pointerId, { x: e.clientX, y: e.clientY, t: now() });
      try { look.setPointerCapture(e.pointerId); } catch (err) { /* ignore */ }
      G.audio && G.audio.unlock();
    });
    look.addEventListener('pointermove', (e) => {
      const p = ptr.get(e.pointerId); if (!p) return;
      const dx = e.clientX - p.x, dy = e.clientY - p.y;
      p.t = now();
      if (ptr.size >= 2) { // pincer pour zoomer (deux doigts réellement posés)
        const others = [...ptr.entries()].filter(([id]) => id !== e.pointerId);
        const o = others[0][1];
        const d0 = Math.hypot(p.x - o.x, p.y - o.y);
        p.x = e.clientX; p.y = e.clientY;
        const d1 = Math.hypot(p.x - o.x, p.y - o.y);
        if (d0 > 20 && d1 > 20) G.cam.zoom(d0 / d1);
        return;
      }
      p.x = e.clientX; p.y = e.clientY;
      G.cam.rotate(dx, dy);
    });
    const lend = (e) => ptr.delete(e.pointerId);
    look.addEventListener('pointerup', lend); look.addEventListener('pointercancel', lend); look.addEventListener('lostpointercapture', lend);
    look.addEventListener('wheel', (e) => G.cam.zoom(e.deltaY > 0 ? 1.1 : 0.9), { passive: true });
    // boutons
    const hold = (id, down, up) => {
      const b = $(id);
      b.addEventListener('pointerdown', (e) => { e.preventDefault(); b.setPointerCapture(e.pointerId); b.classList.add('pressed'); G.audio && G.audio.unlock(); down && down(); });
      const u = () => { b.classList.remove('pressed'); up && up(); };
      b.addEventListener('pointerup', u); b.addEventListener('pointercancel', u);
    };
    hold('#btnAttack', () => { this.chargeStart = G.time; this.charging = true; }, () => { if (!this.charging) return; this.charging = false; const c = clamp((G.time - this.chargeStart) / 1.2, 0, 1); G.powers.punch(c); });
    hold('#btnPower', () => { this.input.powerHeld = true; this.castSelected(); }, () => { this.input.powerHeld = false; });
    hold('#btnWheel', () => this.toggleWheel());
    hold('#btnJump', () => { if (G.god.state === 'fly') this.input.up = true; else G.god.jump(); }, () => { this.input.up = false; });
    hold('#btnDown', () => { this.input.down = true; }, () => { this.input.down = false; });
    hold('#btnFly', () => G.god.toggleFly());
    hold('#btnInteract', () => this.interact());
    hold('#btnTeleport', () => this.openMenu('map'));
  }

  bindKeys() {
    const keys = new Set();
    const upd = () => {
      if (this.joyKeys) { this.input.moveX = (keys.has('KeyD') ? 1 : 0) - (keys.has('KeyA') ? 1 : 0); this.input.moveY = (keys.has('KeyW') ? 1 : 0) - (keys.has('KeyS') ? 1 : 0); if (keys.has('ShiftLeft')) { this.input.moveX *= 1; } else { this.input.moveX *= 0.6; this.input.moveY *= 0.6; } }
      this.input.up = keys.has('Space') && G.god.state === 'fly'; this.input.down = keys.has('KeyC');
    };
    addEventListener('keydown', (e) => {
      if (e.repeat) return; keys.add(e.code); this.joyKeys = true; G.audio && G.audio.unlock();
      if (e.code === 'Space' && G.god.state !== 'fly') G.god.jump();
      if (e.code === 'KeyF') G.god.toggleFly();
      if (e.code === 'KeyQ') this.castSelected(), this.input.powerHeld = true;
      if (e.code === 'KeyE') this.interact();
      if (e.code === 'KeyR') this.toggleWheel();
      if (e.code === 'KeyM' || e.code === 'Escape') this.menuOpen ? this.closeMenu() : this.openMenu('map');
      if (e.code.startsWith('Digit')) { const i = +e.code.slice(5) - 1; if (i >= 0 && i < POWERS.length) this.selectPower(i); }
      upd();
    });
    addEventListener('keyup', (e) => { keys.delete(e.code); if (e.code === 'KeyQ') this.input.powerHeld = false; upd(); });
    addEventListener('mousedown', (e) => { if (e.button === 0 && e.target.id === 'lookzone' && e.shiftKey) { G.powers.punch(0); } });
  }

  castSelected() {
    const p = POWERS[this.power];
    if (!G.powers.ready(p.id)) { this.toast('⌛', `${p.name} se recharge…`, 1.2); return; }
    G.powers.cast(p.id, G.cam.aim());
  }

  selectPower(i) {
    this.power = i; settings.power = i; saveSettings();
    $('#powerIcon').textContent = POWERS[i].icon;
    this.toast(POWERS[i].icon, POWERS[i].name, 1.4);
  }

  toggleWheel(force) {
    const w = $('#wheel'), show = force ?? w.classList.contains('hidden');
    w.classList.toggle('hidden', !show);
    G.slowmo = show ? 0.25 : 1;
  }

  setFlying(f) { $('#btnDown').classList.toggle('hidden', !f); $('#btnJump').textContent = f ? '⤒' : '⤒'; $('#btnFly').classList.toggle('on', f); }

  // ---------- interaction contextuelle : bénir les habitants, restaurer une ruine ----------
  contextAction() {
    const god = G.god;
    if (!god || god.state === 'fly') return null;
    const ruin = G.villages && G.villages.nearestBuilding(god.pos.x, god.pos.z, (b) => b.state === 'ruin');
    if (ruin && Math.hypot(ruin.x - god.pos.x, ruin.z - god.pos.z) < ruin.rad + 14) return { kind: 'restore', b: ruin, label: 'Relever la ruine' };
    const near = G.people && G.people.list.some((p) => p.alive && p.pos.distanceTo(god.pos) < 25);
    if (near) return { kind: 'bless', label: 'Bénir les habitants' };
    return null;
  }
  interact() {
    const c = this.contextAction(); if (!c) return;
    const god = G.god;
    if (c.kind === 'restore') {
      god.playAction('raise', 1.2);
      G.fx.sphere(c.b.mesh.position.clone().add(new THREE.Vector3(0, 4, 0)), 14, 1, 0xfff4c8); G.fx.ring(c.b.mesh.position, 30, 1);
      G.villages.restore(c.b, true);
      const v = G.villages.villages[c.b.village]; v.worship = clamp(v.worship + 0.06, -1, 1);
      G.people.notify('bless', c.b.x, c.b.z, 40, 0);
      this.log('✦', `Le dieu relève une maison de ${v.name}.`);
    } else {
      god.playAction('pose', 1.4);
      const n = G.people.bless(god.pos.x, god.pos.z, 30);
      G.fx.ring(god.pos, 40, 1.2, 0xfff0c0); for (let i = 0; i < 20; i++) G.fx.glow(god.pos.clone().add(new THREE.Vector3((Math.random() - 0.5) * 30, 2, (Math.random() - 0.5) * 30)), 2);
      for (const v of G.villages.villages) if (Math.hypot(v.x - god.pos.x, v.z - god.pos.z) < v.r + 40) { v.worship = clamp(v.worship + 0.03 + n * 0.004, -1, 1); v.fear = Math.max(0, v.fear - 0.03); }
      this.log('✨', n ? `${n} habitants reçoivent la bénédiction divine.` : 'La lumière divine se répand.');
    }
  }

  // ---------- menus ----------
  openMenu(tab) {
    this.menuOpen = true; G.paused = true; $('#menu').classList.remove('hidden'); this.tab(tab);
    G.audio && G.audio.unlock();
  }
  closeMenu() { this.menuOpen = false; G.paused = false; $('#menu').classList.add('hidden'); }
  tab(t) {
    document.querySelectorAll('.menu-tabs button').forEach((b) => b.classList.toggle('on', b.dataset.tab === t));
    document.querySelectorAll('[data-pane]').forEach((p) => p.classList.toggle('hidden', p.dataset.pane !== t));
    if (t === 'map') this.drawWorldMap();
    if (t === 'powers') $('#powerList').innerHTML = `<div class="plist">${[{ icon: '✊', name: 'Poing divin', desc: 'Bouton ✊. Maintenir pour charger le Coup absolu, qui abat tout d\'un seul coup.' }, ...POWERS].map((p, i) => `<div class="pitem ${i - 1 === this.power ? 'on' : ''}" data-i="${i - 1}"><span class="pi">${p.icon}</span><div><b>${p.name}</b><p>${p.desc}</p></div></div>`).join('')}</div>`;
    if (t === 'powers') document.querySelectorAll('.pitem').forEach((x) => x.addEventListener('click', () => { const i = +x.dataset.i; if (i >= 0) { this.selectPower(i); this.tab('powers'); } }));
    if (t === 'hero') this.drawHeroes();
    if (t === 'journal') $('#journalList').innerHTML = this.journal.length ? this.journal.slice().reverse().map((j) => `<div class="jitem"><span>${j.icon}</span><div><small>Jour ${j.day}, ${j.hour}</small><p>${j.text}</p></div></div>`).join('') : '<p class="hint">Rien encore. Le monde attend vos actes.</p>';
  }

  drawHeroes() {
    const cur = settings.hero || 'celeste';
    $('#heroPane').innerHTML = `<div class="plist">${Object.entries(HEROES).map(([id, h]) => `<div class="pitem ${id === cur ? 'on' : ''}" data-hero="${id}"><span class="pi">${id === 'celeste' ? '✦' : '◉'}</span><div><b>${h.name}</b><p>${h.desc}</p></div></div>`).join('')}</div><p class="hint" id="heroMsg">Le héros choisi est gardé pour les prochaines parties.</p>`;
    document.querySelectorAll('[data-hero]').forEach((x) => x.addEventListener('click', () => this.chooseHero(x.dataset.hero)));
  }
  async chooseHero(id) {
    if (this.heroLoading || id === (settings.hero || 'celeste')) return;
    this.heroLoading = true;
    $('#heroMsg').textContent = 'Incarnation en cours…';
    try {
      const m = await loadHero(id);
      G.god.setModel(m);
      settings.hero = id; saveSettings();
      G.fx && G.fx.sphere(G.god.pos.clone().add(new THREE.Vector3(0, 3, 0)), 10, 0.7, 0xfff0c0);
      this.log('✦', `Le dieu prend la forme : ${HEROES[id].name}.`);
      this.drawHeroes();
    } catch (e) { console.error(e); $('#heroMsg').textContent = 'Ce héros n\'a pas pu être chargé.'; }
    this.heroLoading = false;
  }

  mapImage() {
    if (this.mapCanvas) return this.mapCanvas;
    const S = 256, c = document.createElement('canvas'); c.width = c.height = S;
    const ctx = c.getContext('2d'), img = ctx.createImageData(S, S), T = G.terrain;
    for (let j = 0; j < S; j++) for (let i = 0; i < S; i++) {
      const x = -WORLD.half + (i + 0.5) / S * WORLD.size, z = -WORLD.half + (j + 0.5) / S * WORLD.size, h = T.heightAt(x, z);
      let r, g, b;
      if (h < WORLD.water) { const d = clamp((WORLD.water - h) / 12, 0, 1); r = 40 - d * 20; g = 90 - d * 30; b = 120 - d * 20; }
      else if (h > 150) { r = g = b = 225; }
      else if (h > 70) { r = 110; g = 104; b = 96; }
      else { const f = T.forestMask ? T.forestMask[T.idx(x, z)] / 255 : 0; r = 92 - f * 40; g = 120 - f * 30; b = 64 - f * 20; }
      const p = T.paint[T.idx(x, z)]; if (p === 1 || p === 3) { r = 170; g = 150; b = 110; }
      const nx = T.heightAt(x + 6, z) - h; const sh = clamp(1 - nx * 0.06, 0.6, 1.3);
      const k = (j * S + i) * 4; img.data[k] = r * sh; img.data[k + 1] = g * sh; img.data[k + 2] = b * sh; img.data[k + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
    this.mapCanvas = c;
    return c;
  }
  toMap(x, z, S) { return [(x + WORLD.half) / WORLD.size * S, (z + WORLD.half) / WORLD.size * S]; }
  drawWorldMap() {
    const cv = $('#worldmap'), ctx = cv.getContext('2d'), S = cv.width;
    ctx.drawImage(this.mapImage(), 0, 0, S, S);
    ctx.font = 'bold 15px Nunito, sans-serif'; ctx.textAlign = 'center';
    for (const v of G.villages.villages) {
      const [x, y] = this.toMap(v.x, v.z, S);
      ctx.fillStyle = '#f2d38a'; ctx.beginPath(); ctx.arc(x, y, 5, 0, 7); ctx.fill();
      ctx.fillStyle = 'rgba(0,0,0,0.6)'; ctx.fillText(v.name, x + 1, y - 9); ctx.fillStyle = '#fff4d6'; ctx.fillText(v.name, x, y - 10);
    }
    for (const [k, r] of Object.entries(REGIONS)) if (r.x !== undefined && k !== 'village') { const [x, y] = this.toMap(r.x, r.z, S); ctx.fillStyle = 'rgba(255,255,255,0.75)'; ctx.font = 'italic 13px Nunito, sans-serif'; ctx.fillText(r.name, x, y); }
    if (G.monster && G.monster.alive) { const [x, y] = this.toMap(G.monster.pos.x, G.monster.pos.z, S); ctx.fillStyle = '#ff5a1a'; ctx.beginPath(); ctx.arc(x, y, 7, 0, 7); ctx.fill(); }
    const [gx, gy] = this.toMap(G.god.pos.x, G.god.pos.z, S);
    ctx.save(); ctx.translate(gx, gy); ctx.rotate(-G.god.heading + Math.PI); ctx.fillStyle = '#ffd36a'; ctx.strokeStyle = '#000'; ctx.beginPath(); ctx.moveTo(0, -9); ctx.lineTo(6, 7); ctx.lineTo(-6, 7); ctx.closePath(); ctx.fill(); ctx.stroke(); ctx.restore();
  }
  mapTap(e) {
    const cv = $('#worldmap'), r = cv.getBoundingClientRect();
    const x = ((e.clientX - r.left) / r.width) * WORLD.size - WORLD.half, z = ((e.clientY - r.top) / r.height) * WORLD.size - WORLD.half;
    if (!G.terrain.inWorld(x, z, 30)) return;
    this.closeMenu();
    G.god.teleport(x, z);
  }

  // ---------- mode photo ----------
  photo(on) {
    G.cam.photo = on; G.freezeTime = on; G.photoMode = on;
    this.hud.classList.toggle('hidden', on);
    $('#photobar').classList.toggle('hidden', !on);
    $('#phTime').value = Math.round(G.env.dayTime * 1000);
  }
  capture() {
    G.renderNow && G.renderNow();
    let url;
    try { url = G.renderer.domElement.toDataURL('image/jpeg', 0.92); } catch (e) { this.photoMsg('Capture impossible.'); return; }
    if (window.Android && window.Android.saveImage) {
      const r = window.Android.saveImage(url);
      this.photoMsg(r === 'error' ? 'Échec de l\'enregistrement.' : r === 'permission' ? 'Autorisez l\'accès aux photos puis réessayez.' : `Photo enregistrée : ${r}`);
    } else { const a = document.createElement('a'); a.href = url; a.download = `EternalGod_${Date.now()}.jpg`; a.click(); this.photoMsg('Photo téléchargée.'); }
  }
  photoMsg(t) { const m = $('#phMsg'); m.textContent = t; m.classList.add('show'); clearTimeout(this._pm); this._pm = setTimeout(() => m.classList.remove('show'), 2500); }

  // ---------- notifications ----------
  toast(icon, text, dur = 3.5) {
    const t = el('div', 'toast', `<span>${icon}</span>${text}`);
    $('#toasts').appendChild(t);
    setTimeout(() => t.classList.add('out'), dur * 1000);
    setTimeout(() => t.remove(), dur * 1000 + 600);
    const all = $('#toasts').children; if (all.length > 4) all[0].remove();
  }
  log(icon, text) {
    const h = G.env ? G.env.hour() : 12;
    this.journal.push({ icon, text, day: G.env ? G.env.day : 1, hour: `${String(Math.floor(h)).padStart(2, '0')}h${String(Math.floor((h % 1) * 60)).padStart(2, '0')}` });
    if (this.journal.length > 120) this.journal.shift();
    this.toast(icon, text);
  }
  monsterEvent(m) {
    const bar = $('#bossbar');
    if (m.state === 'spawn') bar.classList.remove('hidden');
    if (m.state === 'hit') bar.querySelector('.boss-fill').style.width = `${Math.max(0, m.hp) * 100}%`;
    if (m.state === 'dead') setTimeout(() => bar.classList.add('hidden'), 1500);
    if (m.state === 'spawn') bar.querySelector('.boss-fill').style.width = '100%';
  }

  // ---------- mise à jour par image ----------
  update(dt, fps) {
    if (G.photoMode) return;
    const env = G.env, god = G.god;
    this.t = (this.t || 0) + dt;
    // horloge et lieu (4 fois par seconde suffit)
    if (this.t - (this.slow || 0) > 0.25) {
      this.slow = this.t;
      const h = env.hour();
      $('#clock').textContent = `${env.isNight() ? '☾' : '☀'} ${String(Math.floor(h)).padStart(2, '0')}:${String(Math.floor((h % 1) * 60)).padStart(2, '0')} · Jour ${env.day}`;
      let region = 'Terres sauvages', nearV = null, bd = 1e9;
      for (const v of G.villages.villages) { const d = Math.hypot(v.x - god.pos.x, v.z - god.pos.z); if (d < bd) { bd = d; nearV = v; } }
      const alt = god.pos.y - G.terrain.groundAt(god.pos.x, god.pos.z);
      if (bd < nearV.r + 40) region = nearV.name;
      else if (alt > 250) region = 'Haut des cieux';
      else if (Math.hypot(god.pos.x - REGIONS.volcano.x, god.pos.z - REGIONS.volcano.z) < 280) region = REGIONS.volcano.name;
      else if (Math.hypot(god.pos.x - REGIONS.lake.x, god.pos.z - REGIONS.lake.z) < 200) region = REGIONS.lake.name;
      else if (god.pos.z < -280) region = REGIONS.mountains.name;
      else if (god.pos.x < -150) region = REGIONS.forest.name;
      $('#region').textContent = region;
      $('.rep-fear').style.width = `${clamp(nearV.fear, 0, 1) * 50}%`;
      $('.rep-worship').style.width = `${clamp(nearV.worship, 0, 1) * 50}%`;
      $('#rep').title = nearV.name;
      // interaction contextuelle
      const c = this.contextAction();
      $('#btnInteract').classList.toggle('hidden', !c);
      const lab = $('#interactLabel'); lab.classList.toggle('hidden', !c); if (c) lab.textContent = c.label;
      if (settings.showFps) $('#fps').textContent = `${fps.toFixed(0)} i/s · ${G.renderer.info.render.calls} appels`;
      this.drawMinimap();
    }
    // recharges
    const cd = (id, sel) => { const k = G.powers.cdFrac(id); const c = $(sel); c.style.strokeDashoffset = `${276 * (1 - k)}`; c.parentNode.parentNode.classList.toggle('cooling', k > 0); };
    cd('punch', '#btnAttack .cd circle'); cd(POWERS[this.power].id, '#btnPower .cd circle');
    $('#btnAttack .charge').style.transform = `scale(${this.charging ? clamp((G.time - this.chargeStart) / 1.2, 0, 1) : 0})`;
    $('#crosshair').classList.toggle('on', G.powers.laserOn || ['meteor', 'lightning'].includes(POWERS[this.power].id));
    this.bubbles();
  }

  drawMinimap() {
    const cv = $('#minimap'), ctx = cv.getContext('2d'), S = cv.width, god = G.god;
    const span = 420, k = S / span;
    ctx.save(); ctx.clearRect(0, 0, S, S);
    ctx.beginPath(); ctx.arc(S / 2, S / 2, S / 2 - 2, 0, 7); ctx.clip();
    const img = this.mapImage(), ms = img.width / WORLD.size;
    ctx.translate(S / 2, S / 2); ctx.rotate(G.cam.yaw);
    ctx.drawImage(img, (god.pos.x + WORLD.half) * ms - span / 2 * ms, (god.pos.z + WORLD.half) * ms - span / 2 * ms, span * ms, span * ms, -S / 2, -S / 2, S, S);
    const dot = (x, z, col, r) => { ctx.fillStyle = col; ctx.beginPath(); ctx.arc((x - god.pos.x) * k, (z - god.pos.z) * k, r, 0, 7); ctx.fill(); };
    for (const v of G.villages.villages) dot(v.x, v.z, '#f2d38a', 4);
    if (G.monster && G.monster.alive) dot(G.monster.pos.x, G.monster.pos.z, '#ff4a10', 5);
    ctx.restore();
    ctx.fillStyle = '#ffd36a'; ctx.beginPath(); ctx.moveTo(S / 2, S / 2 - 7); ctx.lineTo(S / 2 + 5, S / 2 + 5); ctx.lineTo(S / 2 - 5, S / 2 + 5); ctx.fill();
    ctx.strokeStyle = 'rgba(232,190,110,0.85)'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(S / 2, S / 2, S / 2 - 2, 0, 7); ctx.stroke();
  }

  // bulles de paroles au-dessus des habitants proches
  bubbles() {
    const box = $('#bubbles');
    const list = (G.people ? G.people.speech : []).filter((s) => s.v.pos.distanceTo(G.camera.position) < 90).slice(-5);
    while (box.children.length < list.length) box.appendChild(el('div', 'bubble'));
    const v = new THREE.Vector3();
    [...box.children].forEach((b, i) => {
      const s = list[i];
      if (!s) { b.style.display = 'none'; return; }
      v.copy(s.v.pos); v.y += s.v.height + 0.6; v.project(G.camera);
      if (v.z > 1) { b.style.display = 'none'; return; }
      b.style.display = 'block';
      b.style.transform = `translate(${(v.x * 0.5 + 0.5) * innerWidth}px, ${(-v.y * 0.5 + 0.5) * innerHeight}px) translate(-50%, -100%)`;
      b.style.opacity = Math.min(1, s.t);
      if (b._t !== s.text) { b._t = s.text; b.innerHTML = `<b>${s.v.name}</b> ${s.text}`; }
    });
  }
}
