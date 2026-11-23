// La campagne : 7 chapitres scénarisés. Chaque chapitre est un script async qui pilote
// le moteur de blocs, le monde 3D, le monstre, l'audio adaptatif et les effets. Les attentes
// utilisent le temps de jeu (la pause les fige) et sont annulées proprement si l'on quitte.
import * as THREE from 'three';
import { TetrisMode } from './tetrisMode.js';
import { RunnerMode } from './runner.js';
import { Monster } from '../world/monster.js';
import { cellX, cellY } from '../world/board3d.js';
import { W, H, VISIBLE } from './tetris.js';
import { CHAPTERS } from '../ui/ui.js';
import { rand, randi, pick, chance, clamp, lerp, easeInOut, easeOutCubic } from '../core/util.js';

export const RETRY = Symbol('retry');

// ——— utilitaires communs ———
function playTetris(game, opts) {
  const mode = new TetrisMode(game, Object.assign({ name: 'chapter' }, opts));
  game.setMode(mode, 'world');
  return mode;
}
function linesReached(mode, n) { return () => mode.engine.lines >= n; }

async function chapterIntro(game, n) {
  const c = CHAPTERS[n - 1];
  game.ui.banner(`CHAPITRE ${n}`, c.name, c.desc);
  game.audio.sfx(n === 1 ? 'ping' : 'stinger', { vol: n === 1 ? 1 : 0.5 });
  await game.sched.wait(2.6);
  game.ui.banner(null);
}

async function complete(game, n, text) {
  const S = game.sched;
  game.save.completeChapter(n);
  game.checkUnlocks();
  game.ui.banner('CHAPITRE TERMINÉ', CHAPTERS[n - 1].name, text || '');
  await S.wait(3.2);
  game.ui.banner(null);
  if (n < 7) {
    game.ui.modal({
      title: `Chapitre ${n + 1}`, text: CHAPTERS[n].name,
      buttons: [
        { label: 'Continuer', primary: true, act: () => { game.ui.modal(null); game.startChapter(n + 1); } },
        { label: 'Menu principal', act: () => { game.ui.modal(null); game.showMenu(); } },
      ],
    });
  }
}

/** Remplit les cellules vivantes (yeux) et les fait ramper. */
function stepLiving(engine) {
  const groups = new Map();
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const c = engine.grid[y][x];
    if (c && c.live) { if (!groups.has(c.pid)) groups.set(c.pid, []); groups.get(c.pid).push([x, y]); }
  }
  const p = engine.active;
  const activeCells = p ? engine.cells(p).map(([x, y]) => x + ',' + y) : [];
  let moved = 0;
  for (const [pid, cells] of groups) {
    const tryMove = (dx, dy) => {
      const ok = cells.every(([x, y]) => {
        const nx = x + dx, ny = y + dy;
        if (nx < 0 || nx >= W || ny < 0 || ny >= VISIBLE) return false;
        if (activeCells.includes(nx + ',' + ny)) return false;
        const c = engine.grid[ny][nx];
        return !c || (c.live && c.pid === pid);
      });
      if (!ok) return false;
      const data = cells.map(([x, y]) => engine.grid[y][x]);
      cells.forEach(([x, y]) => { engine.grid[y][x] = null; });
      cells.forEach(([x, y], i) => { engine.grid[y + dy][x + dx] = data[i]; });
      cells.forEach((c) => { c[0] += dx; c[1] += dy; });
      return true;
    };
    if (tryMove(0, -1)) { moved++; continue; }
    const d = chance(0.5) ? 1 : -1;
    if (tryMove(d, 0) || tryMove(-d, 0)) moved++;
  }
  return moved;
}

/** Bloc qui tombe « derrière le joueur » : entre la caméra et l'écran. */
function pieceBehind(game) {
  const w = game.world;
  const grp = new THREE.Group();
  const mat = w.matFor(pick(['T', 'L', 'Z', 'O']));
  for (const [x, y] of [[0, 0], [1, 0], [0, 1], [1, 1]]) { const m = new THREE.Mesh(w.cellMesh.geometry, mat); m.position.set(x, y, 0); m.castShadow = true; grp.add(m); }
  grp.scale.setScalar(2.2);
  const cam = w.camera.position;
  grp.position.set(rand(-7, 7), cam.y + 18, cam.z - 8);
  w.scene.add(grp);
  game.audio.sfx('whoosh');
  let vy = 0;
  const tick = (dt) => {
    vy -= 40 * dt; grp.position.y += vy * dt; grp.rotation.z += dt * 3;
    if (grp.position.y < cam.y - 16) { w.scene.remove(grp); game.audio.sfx('thud'); game.fx.shake(0.4, 0.3); return true; }
    return false;
  };
  game.addTicker(tick);
}

/** Apparition éclair du monstre devant l'écran (une fraction de seconde). */
async function flashMonster(game, dur = 0.14) {
  const w = game.world;
  const m = new Monster('humanoid', { shadows: false });
  m.root.position.set(rand(-3, 3), -9, 6); m.root.scale.setScalar(1.6); m.setState('reach');
  m.lookAt(w.camera.position);
  w.scene.add(m.root); m.update(0.016);
  game.audio.sfx('stinger', { vol: 0.7 }); game.fx.glitch(0.9); game.fx.aberration(0.02);
  await game.sched.wait(dur);
  w.scene.remove(m.root);
}

// ════════════════════════ CHAPITRE 1 : THE GAME ════════════════════════
async function chapter1(game) {
  const S = game.sched, A = game.audio, fx = game.fx, ui = game.ui;
  await chapterIntro(game, 1);
  const mode = playTetris(game, { goal: 'Objectif : 15 lignes', startLevel: 2, hooks: {
    onClear: (d) => { if (d.n >= 4 && game.save.addSecret('eye-1')) { ui.toast('👁 Secret trouvé : un œil s\'est ouvert'); game.checkUnlocks(); } },
  } });
  const e = mode.engine;
  // Les premières minutes : tout est normal.
  await S.until(linesReached(mode, 4));
  // Anomalie 1 : la pièce change de direction toute seule
  await S.until(() => e.active && e.active.y < 17, 8);
  mode.nudge(); await S.wait(0.5); mode.nudge();
  await S.until(linesReached(mode, 6));
  // Anomalie 2 : une pièce refuse de tourner
  await S.until(() => e.active && e.active.y > 16, 8);
  const refusing = e.active ? e.active.id : -1; e.rules.rotateBlocked = true; ui.setPadEnabled('rotCW', false); ui.setPadEnabled('rotCCW', false);
  await S.until(() => !e.active || e.active.id !== refusing, 12);
  e.rules.rotateBlocked = false; ui.setPadEnabled('rotCW', true); ui.setPadEnabled('rotCCW', true);
  await S.until(linesReached(mode, 7));
  // Anomalie 3 : la musique ralentit
  A.setTempo(0.82); A.setCorruption(0.15); await S.wait(7); A.setTempo(1); A.setCorruption(0);
  await S.until(linesReached(mode, 8));
  // Anomalie 4 : un bloc reste suspendu dans les airs
  await S.until(() => e.active && e.active.y < 17 && e.active.y > 6, 8);
  if (e.active) { const ny = Math.max(e.active.y, Math.min(14, e.ghostY() + 6)); if (e.fits(e.active, e.active.rot, e.active.x, ny)) e.active.y = ny; }
  e.rules.gravityMul = 0.0001; e.rules.frozen = true; A.sfx('reverse', { vol: 0.5 }); fx.glitch(0.2);
  await S.wait(2.4);
  e.rules.gravityMul = 1; e.rules.frozen = false;
  await S.until(linesReached(mode, 9));
  // Anomalie 5 : un visage apparaît brièvement dans la grille
  mode.flashFace('stare', 0.28); A.sfx('whisper', { vol: 0.6 });
  await S.until(linesReached(mode, 10));
  // Anomalie 6 : le score augmente sans raison
  for (let i = 0; i < 6; i++) { e.score += 111; await S.wait(0.12); }
  A.sfx('glitch'); fx.glitch(0.4);
  await S.until(linesReached(mode, 11));
  // Anomalie 7 : des bruits étranges viennent de l'écran
  A.sfx('creak'); await S.wait(1.2); for (let i = 0; i < 3; i++) { A.sfx('knock', { vol: 0.5 }); fx.shake(0.06, 0.15); await S.wait(0.45); }
  await S.until(linesReached(mode, 12));
  // Anomalie 8 : la pièce suit le doigt / le curseur
  ui.toast('…', 1200);
  let follow = 5;
  mode.hooks.onUpdate = (dt) => {
    if (follow <= 0 || !e.active || game.pointer.x === null) return;
    follow -= dt;
    mode._followT = (mode._followT || 0) + dt;
    if (mode._followT > 0.14) {
      mode._followT = 0;
      const col = Math.round((game.pointer.x - 0.5) * 12 + 4.5);
      const c = mode.activeCenter();
      if (c && Math.abs(col - c.x) >= 1) e.move(Math.sign(col - c.x));
    }
  };
  await S.wait(5);
  mode.hooks.onUpdate = null;
  await S.until(linesReached(mode, 13));
  // Anomalie 9 : une silhouette observe depuis le fond
  game.world.showWatcher(0.35); A.setTempo(0.7); A.setCorruption(0.3); A.layer('drone', 0.3);
  game.world.corruption = 0.2;
  await S.wait(3); game.world.showWatcher(0);
  await S.until(linesReached(mode, 14));
  mode.flashFace('stare', 0.5); mode.nudge(); A.sfx('whisper');
  await S.until(linesReached(mode, 15));
  // ——— Climax : la musique s'arrête net ———
  A.stopMusic(true); A.layer('drone', 0);
  e.rules.frozen = true;
  await S.wait(1.6);
  A.layer('heartbeat', 0.6);
  game.world.corruption = 0.6; fx.glitch(0.6);
  game.say('TU N\'AURAIS JAMAIS DÛ ME RÉVEILLER.', { dur: 4, cls: 'giant' });
  A.sfx('stinger'); fx.flash(0xff0000, 0.4); fx.shake(0.3, 1);
  await S.wait(3.5);
  // La partie devient incontrôlable
  e.rules.frozen = false; e.rules.gravityMul = 6; mode.invertControls = true;
  A.layer('static', 0.6); fx.glitch(0.35, true); fx.red(0.3);
  for (let i = 0; i < 10; i++) {
    await S.wait(0.6);
    if (e.over) break;
    e.addGarbage(1, randi(0, 9), 'X'); A.sfx('glitch', { vol: 0.6 }); fx.shake(0.15, 0.2);
    if (i % 3 === 0) mode.flashFace('scream', 0.3);
  }
  A.layer('static', 0); A.layer('heartbeat', 0); fx.glitch(0, true); fx.red(0);
  fx.setDark(1); A.stopLayers();
  mode.hooks.onGameOver = () => {};
  e.over = true;
  await S.wait(2);
  fx.fade(0);
  await complete(game, 1, 'Quelque chose s\'est réveillé.');
}

// ════════════════════════ CHAPITRE 2 : THE CRACK ════════════════════════
const EYE_TARGET = [[3, 0], [4, 0], [5, 0], [6, 0], [1, 1], [2, 1], [3, 1], [4, 1], [5, 1], [6, 1], [7, 1], [8, 1], [2, 0], [2, 2], [7, 0], [7, 2], [3, 2], [4, 2], [5, 2], [6, 2]];

async function eyePuzzle(game, mode) {
  const S = game.sched, A = game.audio, e = mode.engine, w = game.world, ui = game.ui;
  const set = new Set(EYE_TARGET.map(([x, y]) => x + ',' + y));
  let solved = false;
  while (!solved) {
    // préparer
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (e.grid[y][x]) w.particles.blockBurst(cellX(x), cellY(y), 0.2, w.colorOf(e.grid[y][x].t), 0.5);
    e.grid = Array.from({ length: H }, () => new Array(W).fill(null));
    e.active = null; e.hold = null; e.holdUsed = false;
    e.rules.noClear = true; e.rules.forcedNext = ['I', 'T', 'T', 'I', 'I'];
    w.setTargets(EYE_TARGET);
    mode.setGoal('PUZZLE : remplis exactement les cases lumineuses');
    e.spawnPending = true;
    let wrong = false, placed = 0;
    mode.hooks.onLock = (d) => {
      placed++;
      for (const [x, y] of d.cells) if (!set.has(x + ',' + y)) wrong = true;
    };
    await S.until(() => wrong || EYE_TARGET.every(([x, y]) => e.grid[y][x]) || placed >= 7);
    mode.hooks.onLock = null;
    if (wrong || placed >= 7 && !EYE_TARGET.every(([x, y]) => e.grid[y][x])) {
      A.sfx('rotateFail'); game.fx.shake(0.2, 0.3); ui.popup('PAS COMME ÇA', 'red');
      game.say('…recommence.', { dur: 1.5, cls: 'small' });
      await S.wait(1.4);
      continue;
    }
    solved = true;
  }
  // l'œil s'ouvre : la forme devient noire et regarde le joueur
  e.rules.frozen = true; e.active = null;
  w.setTargets([]);
  for (const [x, y] of EYE_TARGET) e.grid[y][x] = { t: 'X' };
  for (const [x, y] of [[4, 1], [5, 1]]) e.grid[y][x] = { t: 'X', face: 'stare' };
  A.sfx('eyeOpen'); game.fx.glitch(0.5); w.redPulse = 30;
  ui.popup('IL TE VOIT', 'red');
  await S.wait(2.2);
  e.rules.noClear = false; e.rules.forcedNext = [];
  const full = e.fullRows(); void full;
  for (let y = 0; y < 3; y++) for (let x = 0; x < W; x++) if (e.grid[y][x]) { w.particles.blockBurst(cellX(x), cellY(y), 0.2, 0x111111, 0.6, 'void'); e.grid[y][x] = null; }
  A.sfx('clear', { n: 3, pitch: 0.5 });
  e.rules.frozen = false; e.spawnPending = true;
  mode.setGoal('Objectif : 10 lignes');
}

async function chapter2(game) {
  const S = game.sched, A = game.audio, fx = game.fx, ui = game.ui, w = game.world;
  await chapterIntro(game, 2);
  const mode = playTetris(game, { goal: 'Objectif : 10 lignes', corruption: 0.3, startLevel: 2 });
  const e = mode.engine;
  A.setTempo(0.85); A.setCorruption(0.25); A.layer('whispers', 0.15); A.layer('drone', 0.15);
  let randomEvents = true;
  // événements aléatoires discrets (pas de jumpscare)
  const ambient = async () => {
    while (randomEvents) {
      await S.wait(rand(9, 16));
      if (!randomEvents || e.rules.frozen) continue;
      const k = randi(0, 3);
      if (k === 0) mode.flashFace(pick(['stare', 'cute']), 0.3);
      else if (k === 1) mode.nudge();
      else if (k === 2) { A.sfx('whisper'); w.showWatcher(0.25); await S.wait(1.5); w.showWatcher(0); }
      else { A.sfx('knock', { vol: 0.4 }); }
    }
  };
  ambient().catch(() => {});
  await S.until(linesReached(mode, 2));
  // il écrit avec des blocs
  e.rules.frozen = true; A.sfx('reverse');
  w.writeWord('HI', 15); fx.glitch(0.3);
  await S.wait(2.2);
  game.say('…tu m\'entends ?', { dur: 2.5, cls: 'small' }); A.sfx('whisper');
  await S.wait(2.6);
  w.clearWord(); e.rules.frozen = false;
  await S.until(linesReached(mode, 4));
  // puzzle : reconstituer la forme
  e.rules.frozen = true;
  game.say('CONSTRUIS-MOI UN ŒIL.', { dur: 2.5 });
  A.sfx('stinger', { vol: 0.5 });
  await S.wait(2.6);
  e.rules.frozen = false;
  await eyePuzzle(game, mode);
  await S.until(linesReached(mode, 6));
  // l'écran devient complètement noir
  fx.setDark(1); A.stopMusic(true); A.sfx('whisper', { vol: 1.2 });
  e.rules.frozen = true;
  await S.wait(2.8);
  fx.setDark(0); fx.fade(0);
  // question
  game.say('QUI ES-TU ?', { dur: 30, cls: 'giant' });
  const ans = await ui.choice('Réponds-lui.', [
    { label: 'Un joueur.', value: 'player' },
    { label: 'Personne.', value: 'nobody' },
    { label: '…le dernier bloc.', value: 'block' },
  ]);
  game.say('');
  if (ans === 'player') game.say('UN JOUEUR. TU JOUES AVEC MOI. ALORS JOUONS.', { dur: 3.5, cls: 'small' });
  else if (ans === 'nobody') game.say('PERSONNE NE JOUE AUSSI LONGTEMPS.', { dur: 3.5, cls: 'small' });
  else {
    game.say('…COMMENT CONNAIS-TU CE NOM ?', { dur: 3.5, cls: 'small' });
    if (game.save.addSecret('eye-2')) { ui.toast('👁 Secret trouvé'); game.checkUnlocks(); }
  }
  await S.wait(3.6);
  A.playMusic('game'); A.setTempo(0.75); A.setCorruption(0.4);
  e.rules.frozen = false;
  // pièces avec visages
  mode.hooks.onSpawn = (p) => { if (chance(0.3)) { p.face = pick(['stare', 'cute']); p.faceIndex = randi(0, 3); } };
  await S.until(linesReached(mode, 8));
  // faux écran de chargement → jumpscare
  randomEvents = false;
  e.rules.frozen = true; A.stopMusic(true);
  ui.fake('loading', true);
  ui.say('');
  const bar = document.getElementById('fakebar');
  for (let i = 0; i <= 99; i += randi(3, 9)) { bar.style.width = Math.min(99, i) + '%'; await S.wait(0.12); }
  bar.style.width = '99%'; document.getElementById('fakelbl').textContent = 'Chapitre suivant… 99%';
  await S.wait(2.2);
  document.getElementById('fakelbl').textContent = 'Il ne veut pas que tu partes.';
  A.sfx('whisper');
  await S.wait(1.6);
  await game.scare2D(0.9);
  ui.fake(null, false);
  A.playMusic('game'); A.setTempo(0.7); A.setCorruption(0.5);
  e.rules.frozen = false;
  randomEvents = true; ambient().catch(() => {});
  await S.until(linesReached(mode, 10));
  randomEvents = false;
  // THE BLOCK observe depuis la grille ; fissure de l'intérieur
  e.rules.frozen = true; A.stopMusic(true); A.layer('heartbeat', 0.5);
  w.showWatcher(0.9);
  await S.wait(1.5);
  w.writeWord('OUT', 16); A.sfx('reverse');
  game.say('LAISSE-MOI SORTIR.', { dur: 3 });
  await S.wait(3);
  A.sfx('knock'); fx.shake(0.3, 0.3); w.crack(0.5, 1, 0.45); A.sfx('crack');
  await S.wait(1.2);
  fx.fade(1);
  await S.wait(1);
  A.stopLayers(); w.showWatcher(0); w.clearWord();
  fx.fade(0); e.over = true;
  await complete(game, 2, 'Il y a une fissure dans l\'écran.');
}

// ════════════════════════ CHAPITRE 3 : BREAK THE SCREEN ════════════════════════
async function chapter3(game) {
  const S = game.sched, A = game.audio, fx = game.fx, ui = game.ui, w = game.world;
  await chapterIntro(game, 3);
  const mode = playTetris(game, { goal: 'Objectif : 4 lignes… si tu peux', corruption: 0.45, startLevel: 3, music: 'game', hooks: { onGameOver: () => {} } });
  const e = mode.engine;
  A.setTempo(0.7); A.setCorruption(0.5); A.layer('drone', 0.3);
  w.crack(0.5, 1, 0.45);
  const t0 = S.t;
  await S.until(() => e.lines >= 4 || S.t - t0 > 75 || e.over);
  // ——— le jeu s'arrête complètement ———
  e.rules.frozen = true; e.active = null; mode.controlsEnabled = false;
  A.stopMusic(true); A.layer('drone', 0);
  ui.showHud(false); ui.goal('');
  await S.wait(2);
  // les blocs tombent hors de la grille
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (e.grid[y][x]) { w.particles.blockBurst(cellX(x), cellY(y), 0.3, w.colorOf(e.grid[y][x].t), 0.4, 'pixels'); e.grid[y][x] = null; }
  A.sfx('clear', { n: 4, pitch: 0.4 });
  await S.wait(1.5);
  // ——— un énorme bloc noir apparaît au centre ———
  const block = new Monster('block');
  block.root.position.set(0, 0.5, -0.2); block.root.scale.setScalar(0.01); block.eyeOpen = 0;
  w.board.add(block.root);
  game.addTicker((dt) => { block.update(dt); return block.disposed; });
  A.sfx('thud'); fx.shake(0.3, 0.3);
  for (let i = 0; i <= 20; i++) { block.root.scale.setScalar(easeOutCubic(i / 20) * 0.9); await S.wait(0.02); }
  A.layer('heartbeat', 0.3);
  await S.wait(2);
  // ——— deux yeux rouges s'ouvrent ———
  A.sfx('eyeOpen');
  for (let i = 0; i <= 30; i++) { block.eyeOpen = i / 30; await S.wait(0.03); }
  w.redPulse = 20;
  await S.wait(1);
  // ——— zoom lent de la caméra ———
  const start = w.camera.position.clone(), look0 = new THREE.Vector3(0, w.camLook.y, 0);
  const endPos = new THREE.Vector3(0, 1.2, 15), endLook = new THREE.Vector3(0, 0.6, 0);
  w.camOverride = { pos: start.clone(), look: look0.clone() };
  fx.dof(0.6);
  for (let i = 0; i <= 120; i++) {
    const k = easeInOut(i / 120);
    w.camOverride.pos.lerpVectors(start, endPos, k); w.camOverride.look.lerpVectors(look0, endLook, k);
    if (i % 40 === 0) A.sfx('breath', { vol: 0.8 });
    await S.wait(1 / 30);
  }
  // ——— il respire ———
  block.breath = 3; block.lookAt(w.camera.position.clone());
  A.sfx('breath'); await S.wait(1.6); A.sfx('breath'); await S.wait(1.6);
  // ——— une main géométrique apparaît et frappe ———
  block.arm.visible = true;
  for (let i = 0; i <= 25; i++) { block.armExtend = i / 25 * 0.5; await S.wait(0.03); }
  A.sfx('creak');
  const strikes = [{ s: 0.55, txt: '' }, { s: 1.0, txt: '' }, { s: 1.6, txt: '' }];
  for (let k = 0; k < 3; k++) {
    await S.wait(k === 0 ? 0.8 : 1.4);
    block.armExtend = 1; block.punch();
    await S.wait(0.42);
    const hx = rand(-1, 1.5), hy = rand(0, 2);
    if (k < 2) {
      w.crack(hx, hy, strikes[k].s);
      A.sfx('punch'); A.sfx('crack');
      fx.shake(0.35 + k * 0.25, 0.4); fx.flash(0xffffff, 0.25 + k * 0.1); game.input.haptic(120, 255);
      if (k === 1) { fx.warp(0.9); fx.glitch(0.7); fx.aberration(0.03); A.sfx('glitch'); }
    } else {
      // ——— troisième frappe : l'écran explose ———
      w.shatter(hx, hy);
      A.sfx('shatter'); A.sfx('punch');
      fx.shake(1.2, 1.0); fx.flash(0xff2020, 0.8); fx.aberration(0.05); fx.warp(1.4); fx.glitch(1);
      game.input.haptic(400, 255);
    }
    await S.wait(0.6);
  }
  // ——— le monstre sort physiquement de l'écran ———
  block.root.visible = false; block.disposed = true;
  const mon = new Monster('humanoid');
  mon.root.position.set(0, -12, -1); mon.root.scale.setScalar(1.15); mon.setState('reach');
  mon.setScatter(1, true); mon.setScatter(0);
  w.scene.add(mon.root);
  let monAlive = true;
  game.addTicker((dt) => { mon.update(dt); return !monAlive; });
  mon.lookAt(w.camera.position);
  A.sfx('roar'); A.layer('drone', 0.6); w.redPulse = 60;
  fx.red(0.25);
  for (let i = 0; i < 90; i++) {
    mon.root.position.z = lerp(-1, 3.2, easeInOut(i / 90));
    mon.root.position.y = lerp(-12, -8.2, easeOutCubic(i / 90));
    if (i % 15 === 0) { A.sfx('monsterStep'); fx.shake(0.25, 0.2); w.particles.debris(rand(-5, 5), rand(-6, 6), 1.2, 0xdff4ff, 3); }
    await S.wait(1 / 30);
  }
  // ——— cri terrifiant ———
  mon.setState('scream');
  A.sfx('scream', { dur: 2.4 }); fx.shake(0.8, 2); fx.warp(1.2); fx.aberration(0.04);
  if (!game.save.s.softScares) fx.flash(0xff0000, 0.5);
  await S.wait(2.3);
  // ——— écran noir. silence total. ———
  fx.setDark(1); A.stopLayers(); A.stopMusic(true); fx.red(0); fx.dof(0);
  monAlive = false; w.scene.remove(mon.root);
  await S.wait(3);
  // ——— un petit bloc tombe au sol ———
  await livingBlock(game);
  fx.setDark(0); w.camOverride = null;
  await complete(game, 3, 'Il est sorti.');
}

/** §7 : le bloc vivant (petite scène dédiée). */
async function livingBlock(game) {
  const S = game.sched, A = game.audio, fx = game.fx;
  const scene = new THREE.Scene(); scene.background = new THREE.Color(0x000000);
  const cam = new THREE.PerspectiveCamera(50, 1, 0.05, 100);
  cam.position.set(0, 0.6, 4); cam.lookAt(0, 0.2, 0);
  const spot = new THREE.SpotLight(0xffffff, 26, 20, 0.42, 0.7, 1.4); spot.position.set(0, 6, 1); spot.castShadow = true; spot.shadow.mapSize.set(1024, 1024); scene.add(spot); scene.add(spot.target);
  scene.add(new THREE.AmbientLight(0x202030, 0.6));
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(40, 40), new THREE.MeshStandardMaterial({ color: 0x0b0b0e, roughness: 0.35, metalness: 0.5 }));
  floor.rotation.x = -Math.PI / 2; floor.receiveShadow = true; scene.add(floor);
  // débris de verre au sol
  for (let i = 0; i < 30; i++) {
    const g = new THREE.Mesh(new THREE.TetrahedronGeometry(rand(0.03, 0.12)), new THREE.MeshPhysicalMaterial({ color: 0xdff4ff, roughness: 0.05, transparent: true, opacity: 0.6, clearcoat: 1 }));
    g.position.set(rand(-3, 3), 0.03, rand(-3, 2)); g.rotation.set(rand(6), rand(6), rand(6)); scene.add(g);
  }
  const little = new Monster('block');
  little.root.scale.setScalar(0.1); little.eyeOpen = 0; little.breath = 0;
  little.root.position.set(0, 6, 0);
  scene.add(little.root);
  const big = new Monster('humanoid');
  big.root.position.set(0, -14, -6); big.root.scale.setScalar(1.05); big.setState('reach'); big.eyeOpen = 0; big.root.visible = false;
  scene.add(big.root);
  const prevScene = game.fx.scene, prevCam = game.fx.camera;
  game.fx.setView(scene, cam);
  game.setCustomUpdate((dt) => { little.update(dt); big.update(dt); });
  fx.setDark(0); fx.fade(0); fx.dof(0.3);
  // chute
  let vy = 0, bounces = 0;
  while (bounces < 3) {
    vy -= 22 * (1 / 60); little.root.position.y += vy / 60;
    if (little.root.position.y <= 0.2) { little.root.position.y = 0.2; vy = -vy * 0.35; bounces++; A.sfx('thud', { vol: 0.6 / bounces }); }
    await S.wait(1 / 60);
  }
  little.root.position.y = 0.2;
  // immobile… quelques secondes passent
  await S.wait(3.5);
  // il tourne légèrement
  A.sfx('creak', { vol: 0.4 });
  for (let i = 0; i < 30; i++) { little.root.rotation.y = Math.sin(i / 30 * Math.PI) * 0.35; await S.wait(1 / 30); }
  await S.wait(1.5);
  // deux petits yeux rouges
  A.sfx('eyeOpen', { vol: 0.6 });
  for (let i = 0; i <= 20; i++) { little.eyeOpen = i / 20; await S.wait(0.03); }
  little.lookAt(cam.position);
  await S.wait(1.8);
  // il rampe vers le joueur
  A.layer('heartbeat', 0.6);
  for (let i = 0; i < 80; i++) {
    little.root.position.z += 0.022; little.root.position.y = 0.2 + Math.abs(Math.sin(i * 0.5)) * 0.04;
    little.root.rotation.z = Math.sin(i * 0.5) * 0.08;
    if (i % 10 === 0) A.sfx('skitter', { vol: 0.5 });
    await S.wait(1 / 30);
  }
  await S.wait(0.6);
  // il saute vers la caméra — jumpscare
  for (let i = 0; i <= 12; i++) {
    const k = i / 12;
    little.root.position.set(0, 0.2 + Math.sin(k * Math.PI) * 0.5 + k * 0.3, lerp(1.76, 3.7, k));
    little.root.scale.setScalar(0.1 + k * 0.05);
    await S.wait(1 / 40);
  }
  game.jumpscare({ intensity: 0.9 });
  await S.wait(0.9);
  little.root.visible = false;
  // puis le véritable monstre apparaît derrière
  fx.flash(0x000000, 0.9);
  spot.color.set(0xff2020); spot.position.set(0, 7, -3); spot.target.position.set(0, 2, -6);
  cam.position.set(0, 1, 4); cam.lookAt(0, 2.5, -6);
  A.sfx('stinger');
  big.root.visible = true;
  for (let i = 0; i < 50; i++) { big.root.position.y = lerp(-14, -5.2, easeOutCubic(i / 50)); big.eyeOpen = i / 50; big.lookAt(cam.position); await S.wait(1 / 30); }
  big.setState('scream'); A.sfx('roar', { vol: 1 }); fx.shake(0.4, 1);
  await S.wait(1.4);
  fx.setDark(1); A.stopLayers();
  await S.wait(1.5);
  game.setCustomUpdate(null);
  game.fx.setView(prevScene, prevCam);
  fx.dof(0);
}

// ════════════════════════ CHAPITRE 4 : RUN ════════════════════════
function runStage(game, cfg) {
  return new Promise((resolve, reject) => {
    const start = (checkpoint) => {
      const r = new RunnerMode(game, Object.assign({}, cfg, {
        checkpointAt: checkpoint,
        onWin: (rr) => resolve(rr),
        onRetry: (cp) => { game.resumeInfo = { stage: cfg.stage || 1, cp }; reject(RETRY); },
      }));
      game.setMode(r, 'runner');
    };
    start(cfg.checkpointAt || 0);
  });
}

async function chapter4(game, resume) {
  const S = game.sched, A = game.audio, fx = game.fx;
  if (!resume) {
    await chapterIntro(game, 4);
    game.say('COURS.', { dur: 2, cls: 'giant' }); A.sfx('roar');
    await S.wait(1.5);
  }
  game.ch4Hits = resume ? game.ch4Hits : 0;
  const stage = resume ? resume.stage : 1;
  const track = (r) => { const h0 = r.hit.bind(r); r.hit = (s) => { if (!(r.p.inv > 0)) game.ch4Hits++; h0(s); }; };
  if (stage === 1) {
    const r = await runStage(game, {
      stage: 1, theme: 'arcade', length: 560, monster: 'humanoid', speed: 12, warp: 0.6, checkpointAt: resume ? resume.cp : 0,
      mix: ['low', 'high', 'wall', 'low', 'door', 'gap'],
      script: [{ at: 0.18, type: 'surge' }, { at: 0.3, type: 'say', text: 'TU NE PEUX PAS FUIR UN JEU.', dur: 2.5, cls: 'small' }, { at: 0.38, type: 'hide' }, { at: 0.5, type: 'checkpoint' }, { at: 0.6, type: 'vanish' }, { at: 0.78, type: 'lock', shape: 'T' }, { at: 0.9, type: 'surge' }],
      onStart: track,
    });
    void r;
    game.ui.banner('', 'PLATEAU GÉANT', 'La salle d\'arcade s\'effondre dans la grille…');
    A.sfx('shatter'); fx.flash(0xffffff, 0.8);
    await S.wait(2.4); game.ui.banner(null);
  }
  await runStage(game, {
    stage: 2, theme: 'board', length: 600, monster: 'humanoid', speed: 13.5, difficulty: 0.15, checkpointAt: resume && resume.stage === 2 ? resume.cp : 0,
    mix: ['fall', 'fall', 'low', 'high', 'wall', 'gap', 'door'],
    script: [{ at: 0.15, type: 'wallcrawl' }, { at: 0.3, type: 'leap' }, { at: 0.4, type: 'checkpoint' }, { at: 0.48, type: 'ambush' }, { at: 0.62, type: 'collapse' }, { at: 0.74, type: 'lock', shape: 'L' }, { at: 0.86, type: 'surge' }, { at: 0.92, type: 'wallcrawl' }],
    onStart: track,
  });
  if (game.ch4Hits <= 1 && game.save.addSecret('eye-3')) { game.ui.toast('👁 Secret trouvé'); game.checkUnlocks(); }
  A.stopMusic(); A.stopLayers();
  fx.fade(1); await S.wait(1); fx.fade(0);
  await complete(game, 4, 'Tu lui as échappé. Pour l\'instant.');
}

// ════════════════════════ CHAPITRE 5 : THE CORRUPTION ════════════════════════
async function chapter5(game, resume) {
  const S = game.sched, A = game.audio, fx = game.fx, ui = game.ui, w = game.world;
  if (!resume) {
    await chapterIntro(game, 5);
    let kills = 0, keys = 0;
    const mode = playTetris(game, { goal: 'Active 3 mécanismes : efface les lignes dorées', corruption: 0.8, startLevel: 3, noGlass: true, hooks: {
      onSpawn: (p, m) => {
        m._spawnN = (m._spawnN || 0) + 1;
        if (m._spawnN % 3 === 0) { p.tag = 'live'; p.face = 'stare'; p.faceIndex = randi(0, 3); A.sfx('skitter', { vol: 0.5 }); }
      },
      onLock: (d, m) => {
        if (d.piece.tag === 'live') for (const [x, y] of d.cells) { const c = m.engine.grid[y][x]; if (c) c.live = true; }
      },
      onClear: (d, m) => {
        let k = 0;
        for (const row of d.removed) {
          if (row.some((c) => c && c.t === 'K')) { keys++; A.sfx('key'); ui.popup(`MÉCANISME ${keys}/3`, 'gold'); w.lightBurst = 4; }
          const pids = new Set(row.filter((c) => c && c.live).map((c) => c.pid)); k += pids.size;
        }
        if (k) { kills += k; A.sfx('scream', { vol: 0.25, dur: 0.6 }); ui.popup(`CRÉATURE DÉTRUITE ×${k}`, 'red'); }
        if (kills >= 6 && game.save.addSecret('eye-4')) { ui.toast('👁 Secret trouvé'); game.checkUnlocks(); }
        m.setGoal(`Mécanismes : ${keys}/3 · Créatures détruites : ${kills}`);
      },
    } });
    const e = mode.engine;
    w.setTheme({ ...w.theme, bg: 0x070306, fog: 0x070306, rim1: 0xff2030, rim2: 0x601020, panel: 0x0c0508, hemiSky: 0x704050, hemiGround: 0x100408 });
    A.setTempo(0.72); A.setCorruption(0.6); A.layer('drone', 0.35); A.layer('whispers', 0.2);
    // grille corrompue de départ + lignes dorées
    const seedRow = (y, key) => {
      const hole = randi(0, 9);
      for (let x = 0; x < W; x++) if (x !== hole) e.grid[y][x] = { t: x === (hole + 3) % W && key ? 'K' : (chance(0.3) ? 'X' : 'G') };
    };
    seedRow(0, true); seedRow(1, false); seedRow(2, true);
    let spreadT = 0, liveT = 0, keyT = 0;
    mode.hooks.onUpdate = (dt) => {
      if (e.rules.frozen) return;
      liveT += dt; spreadT += dt; keyT += dt;
      if (liveT > 1.1) { liveT = 0; if (stepLiving(e)) A.sfx('skitter', { vol: 0.25 }); }
      if (spreadT > 7) {
        spreadT = 0;
        // la corruption se propage
        const xs = [];
        for (let y = 0; y < VISIBLE; y++) for (let x = 0; x < W; x++) if (e.grid[y][x] && e.grid[y][x].t === 'X') xs.push([x, y]);
        if (xs.length) {
          const [x, y] = pick(xs);
          for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
            const c = e.grid[y + dy] && e.grid[y + dy][x + dx];
            if (c && c.t !== 'X' && c.t !== 'K') { c.t = 'X'; c.live = false; A.sfx('glitch', { vol: 0.3 }); break; }
          }
        }
      }
      if (keyT > 18 && keys < 3) { keyT = 0; e.addGarbage(1, randi(0, 9), 'G'); const row = e.grid[0]; const idx = row.findIndex((c) => c); if (idx >= 0) row[idx] = { t: 'K' }; }
    };
    // événements
    await S.until(() => keys >= 1 || e.lines >= 4);
    // une pièce se transforme en créature en pleine chute
    await S.until(() => e.active && e.active.y < 19, 8);
    if (e.active) { e.active.tag = 'live'; e.active.face = 'scream'; e.active.dark = true; A.sfx('roar', { vol: 0.5 }); fx.glitch(0.6); ui.popup('ELLE EST VIVANTE', 'red'); }
    await S.wait(6);
    // une pièce tombe derrière le joueur
    pieceBehind(game);
    await S.until(() => keys >= 2 || e.lines >= 9);
    // flash du monstre (une fraction de seconde)
    await flashMonster(game, 0.15);
    await S.wait(5);
    // écran de défaite qui cache une attaque
    e.rules.frozen = true; A.sfx('gameover');
    ui.fake('gameover', true);
    await new Promise((res) => { document.getElementById('fakeRetry').onclick = res; game.addTicker(() => { if (!game.mode || game.mode !== mode) { res(); return true; } return false; }); });
    ui.fake(null, false);
    await game.scare2D(0.8);
    e.rules.frozen = false;
    game.say('IL N\'Y A PAS DE « REJOUER ».', { dur: 3 });
    await S.until(() => keys >= 3);
    e.rules.frozen = true; mode.hooks.onUpdate = null;
    A.sfx('unlock'); ui.popup('PASSAGE OUVERT', 'gold');
    await S.wait(1.5);
    // faux retour au menu principal
    A.stopMusic(true); A.stopLayers();
    fx.fade(1); await S.wait(0.8);
    game.showFakeMenu(true);
    fx.fade(0);
    await S.wait(5.5);
    fx.glitch(1); A.sfx('glitch'); game.menuScene.setCorruption(1);
    document.getElementById('title').classList.add('corrupt');
    await S.wait(1.2);
    game.say('TU CROYAIS POUVOIR PARTIR ?', { dur: 2.5, cls: 'giant' });
    await S.wait(1.6);
    await game.scare2D(1);
    game.showFakeMenu(false);
  }
  // poursuite : THE CRAWLER dans la grille en ruine
  game.say('IL A CHANGÉ DE FORME.', { dur: 2.5, cls: 'small' });
  await S.wait(1);
  await runStage(game, {
    stage: 1, theme: 'collapse', length: 620, monster: 'crawler', speed: 14, difficulty: 0.2, regen: 0.5, checkpointAt: resume ? resume.cp : 0,
    mix: ['gap', 'gap', 'low', 'high', 'wall', 'fall', 'door'],
    script: [{ at: 0.12, type: 'surge' }, { at: 0.25, type: 'wallcrawl' }, { at: 0.35, type: 'collapse' }, { at: 0.45, type: 'checkpoint' }, { at: 0.52, type: 'hide' }, { at: 0.66, type: 'vanish' }, { at: 0.78, type: 'lock', shape: 'S' }, { at: 0.88, type: 'wallcrawl' }],
  });
  A.stopMusic(); A.stopLayers();
  await complete(game, 5, 'Les pièces vivantes t\'ont laissé passer.');
}

// ════════════════════════ CHAPITRE 6 : THE KING ════════════════════════
async function chapter6(game) {
  const S = game.sched, A = game.audio, fx = game.fx, ui = game.ui, w = game.world;
  await chapterIntro(game, 6);
  let hp = 100, noLoss = true;
  const run = async () => {
    const king = new Monster('king');
    king.root.scale.setScalar(2.6);
    king.setState('idle');
    let kingAlive = true;
    const bossBar = document.createElement('div');
    bossBar.style.cssText = 'position:fixed;left:50%;transform:translateX(-50%);top:calc(64px + env(safe-area-inset-top,0px));width:70vw;max-width:360px;height:12px;border-radius:6px;background:rgba(0,0,0,.6);border:1px solid #ff3040;z-index:6;overflow:hidden';
    bossBar.innerHTML = '<i style="position:absolute;inset:0;width:100%;background:linear-gradient(90deg,#ff1020,#ffcc00);transition:width .3s"></i><span style="position:absolute;width:100%;text-align:center;top:14px;font-size:11px;letter-spacing:.3em">THE KING</span>';
    document.body.appendChild(bossBar);
    const cleanup = () => { kingAlive = false; bossBar.remove(); king.dispose(); w.warnColumn(0, 0); };
    const setHp = (v) => { hp = clamp(v, 0, 100); bossBar.firstChild.style.width = hp + '%'; };
    setHp(hp);
    let hand = null; // { cells, t }
    const damage = (amt, label) => {
      setHp(hp - amt);
      A.sfx('bossHit'); fx.shake(0.4, 0.3); fx.aberration(0.015);
      ui.popup(label || `-${amt}`, 'gold');
      // rayon vers le roi
      const chest = new THREE.Vector3(); king.chest.getWorldPosition(chest);
      for (let i = 0; i < 24; i++) {
        const k = i / 24;
        const p = new THREE.Vector3(lerp(rand(-4, 4), chest.x, k), lerp(-6, chest.y, k), lerp(0.5, chest.z, k));
        w.particles.sparks(p.x, p.y, p.z, 0xffd21f, 2, 3);
      }
      const n = Math.ceil(amt / 4);
      for (let i = 0; i < n; i++) king.breakChunk(w.scene);
      king.setState('anger'); setTimeout(() => kingAlive && king.state === 'anger' && king.setState('idle'), 700);
    };
    const mode = playTetris(game, { goal: 'Efface des lignes pour briser le Roi', corruption: 0.7, startLevel: 4, music: 'boss', noGlass: true, hooks: {
      onClear: (d, m) => {
        let dmg = d.n * 5 + (d.n >= 4 ? 12 : 0) + Math.max(0, d.combo) * 3;
        let extra = '';
        for (const row of d.removed) {
          if (row.some((c) => c && c.t === 'K')) { dmg += 10; extra = 'PIÈCE DORÉE ! '; }
          if (row.some((c) => c && c.hand)) { dmg += 18; extra = 'MAIN PIÉGÉE ! '; A.sfx('scream', { vol: 0.6, dur: 1 }); }
        }
        if (hand && hand.cells.every(([x, y]) => !(m.engine.grid[y] && m.engine.grid[y][x] && m.engine.grid[y][x].hand))) hand = null;
        damage(dmg, `${extra}-${dmg}`);
      },
      onLock: (d, m) => { if (d.piece.tag === 'gold') for (const [x, y] of d.cells) { if (m.engine.grid[y][x]) m.engine.grid[y][x].t = 'K'; } },
      onSpawn: (p, m) => { if (p.type === m._goldNext) { p.tag = 'gold'; m._goldNext = null; } },
      onGameOver: () => {
        noLoss = false; cleanup();
        ui.modal({ title: 'LE ROI T\'A ÉCRASÉ', text: 'Recommencer le combat ?', buttons: [
          { label: 'Recommencer', primary: true, act: () => { ui.modal(null); hp = 100; S.abort(); game.restartChapter6(run); } },
          { label: 'Menu principal', act: () => { ui.modal(null); game.showMenu(); } },
        ] });
      },
    } });
    const e = mode.engine;
    game.onModeExit = cleanup;
    w.bezels.forEach((b) => (b.visible = false));
    w.setLayout(0.22, game.save.s.controls === 'buttons' ? 0.26 : 0.07);
    king.root.position.set(0, -30, -8);
    w.scene.add(king.root);
    let kingY = 0, entered = false;
    const ndc = new THREE.Vector3();
    game.addTicker((dt) => {
      if (!kingAlive) return true;
      king.lookAt(w.camera.position); king.update(dt);
      if (entered) {
        // garde la tête du roi dans le haut de l'écran, au-dessus de la grille
        king.head.getWorldPosition(ndc); ndc.project(w.camera);
        kingY += (0.62 - ndc.y) * 6 * dt * 4;
        king.root.position.y = kingY + Math.sin(game.time * 0.8) * 0.3;
      }
      return false;
    });
    // entrée du roi
    e.rules.frozen = true;
    A.sfx('roar'); fx.shake(0.6, 2);
    for (let i = 0; i < 60; i++) { king.root.position.y = lerp(-30, -6, easeOutCubic(i / 60)); if (i % 12 === 0) A.sfx('monsterStep'); await S.wait(1 / 30); }
    kingY = king.root.position.y; entered = true;
    king.setState('scream'); A.sfx('scream', { dur: 2 }); fx.warp(0.8);
    game.say('JE SUIS TOUTES LES PIÈCES QUE TU AS PERDUES.', { dur: 3.5, cls: 'small' });
    await S.wait(2.5); king.setState('idle');
    e.rules.frozen = false;
    // boucle d'attaques
    let attackT = 6, phase = 1;
    while (hp > 0) {
      await S.wait(0.1);
      if (e.over) return;
      if (hp < 55 && phase === 1) { phase = 2; game.say('ASSEZ !', { dur: 1.5, cls: 'giant' }); A.setTempo(1.1); fx.glitch(0.15, true); ui.toast('Les pièces DORÉES infligent plus de dégâts'); }
      if (hp < 25 && phase === 2) { phase = 3; e.rules.gravityMul = 1.35; A.setTempo(1.2); A.sfx('roar'); w.corruption = 0.9; }
      if (phase >= 2 && !mode._goldNext && chance(0.02)) { const t = pick(['I', 'O', 'T']); mode._goldNext = t; e.rules.forcedNext.push(t); }
      // retrait de la main non piégée
      if (hand) { hand.t -= 0.1; if (hand.t <= 0) { for (const [x, y] of hand.cells) if (e.grid[y][x] && e.grid[y][x].hand) e.grid[y][x] = null; hand = null; A.sfx('whoosh'); } }
      attackT -= 0.1 * (phase === 3 ? 1.4 : 1);
      if (attackT > 0) continue;
      attackT = rand(7, 10);
      const kind = pick(phase === 1 ? ['garbage', 'column', 'hand'] : ['garbage', 'column', 'hand', 'column']);
      if (kind === 'garbage') {
        king.setState('slam'); A.sfx('creak');
        await S.wait(0.65);
        e.addGarbage(phase === 3 ? 2 : 1); A.sfx('punch'); fx.shake(0.6, 0.4); game.input.haptic(120, 255);
      } else if (kind === 'column') {
        const col = randi(0, 9);
        for (let i = 0; i < 15; i++) { w.warnColumn(col, i % 2 ? 0.35 : 0.1); await S.wait(0.1); }
        w.warnColumn(col, 0);
        king.punch(); A.sfx('punch');
        let top = 0; for (let y = 0; y < VISIBLE; y++) if (e.grid[y][col]) top = y + 1;
        for (let k = 0; k < 3 && top + k < VISIBLE - 2; k++) e.grid[top + k][col] = { t: 'X' };
        w.particles.debris(cellX(col), cellY(top + 1), 0.5, 0x111111, 10); fx.shake(0.4, 0.3);
      } else if (kind === 'hand' && !hand) {
        // la main entre dans la grille : complète les lignes autour pour la piéger
        let h = 0; for (let y = 0; y < VISIBLE; y++) if (e.grid[y].some((c) => c)) h = y + 1;
        const y0 = Math.min(VISIBLE - 6, Math.max(0, h - 1));
        const x0 = randi(0, 8);
        const cells = [];
        for (let dy = 0; dy < 2; dy++) for (let dx = 0; dx < 2; dx++) { const x = x0 + dx, y = y0 + dy; if (!e.grid[y][x]) { e.grid[y][x] = { t: 'M', hand: true }; cells.push([x, y]); } }
        if (cells.length) { hand = { cells, t: 12 }; king.setState('reach'); A.sfx('skitter'); ui.popup('PIÈGE SA MAIN !', 'gold'); setTimeout(() => kingAlive && king.setState('idle'), 1500); }
      }
    }
    // ——— victoire ———
    e.rules.frozen = true; mode.controlsEnabled = false;
    A.stopMusic(true); fx.glitch(0, true);
    king.setState('transform'); A.sfx('scream', { dur: 3 }); fx.shake(1, 3); fx.warp(1.5);
    for (let i = 0; i < 12; i++) { for (let k = 0; k < 4; k++) king.breakChunk(w.scene); A.sfx('bossHit', { vol: 0.6 }); await S.wait(0.18); }
    king.setScatter(1); fx.flash(0xffffff, 1);
    await S.wait(1.5);
    cleanup(); game.onModeExit = null;
    if (noLoss && game.save.addSecret('eye-5')) { ui.toast('👁 Secret trouvé'); }
    fx.fade(1); await S.wait(1.2); fx.fade(0);
    await complete(game, 6, 'Le Roi est tombé. Mais il reste un bloc.');
  };
  game.restartChapter6 = (fn) => { game.runScript(() => fn()); };
  await run();
}

// ════════════════════════ CHAPITRE 7 : THE LAST BLOCK ════════════════════════
async function chapter7(game) {
  const S = game.sched, A = game.audio, fx = game.fx, ui = game.ui, w = game.world;
  await chapterIntro(game, 7);
  const allSecrets = game.save.data.secrets.length >= 5;
  const mode = playTetris(game, { goal: '', corruption: 0.5, noMusic: true, startLevel: 1, noGlass: true });
  const e = mode.engine;
  w.glass.visible = false;
  w.setTheme({ ...w.theme, bg: 0x000000, fog: 0x000000, rim1: 0x404060, rim2: 0x202030, panel: 0x050508 });
  // grille presque pleine : un puits de 4 cases
  e.grid = Array.from({ length: H }, () => new Array(W).fill(null));
  // 4 lignes noires auxquelles il ne manque qu'une colonne, et des débris au-dessus
  for (let y = 0; y < 10; y++) for (let x = 0; x < W; x++) if (x !== 4 && (y < 4 || Math.random() < 0.55 - y * 0.04)) e.grid[y][x] = { t: 'X' };
  e.active = null; e.rules.frozen = true; e.spawnPending = false;
  A.layer('drone', 0.25); A.layer('wind', 0.3);
  const lines = [
    'Chaque partie que tu as abandonnée…',
    'Chaque pièce que tu n\'as jamais posée…',
    '…tombait ici. Dans le noir.',
    'J\'ÉTAIS FAIT DE TOUTES CES PIÈCES.',
    'Il ne manque qu\'UN bloc.',
  ];
  for (const l of lines) { game.say(l, { dur: 3.2, cls: l === l.toUpperCase() ? '' : 'white small' }); A.sfx('whisper', { vol: 0.5 }); await S.wait(3.4); }
  game.say('Pose-le… et je dormirai. Garde-le… et tu prendras ma place.', { dur: 5, cls: 'white small' });
  await S.wait(2);
  ui.toast(allSecrets ? 'Les cinq yeux te regardent. Tourne le bloc… encore… et encore.' : 'Pose le dernier bloc, ou appuie sur RÉSERVE pour le garder.', 5000);
  // le dernier bloc
  e.rules.frozen = false; e.rules.gravityMul = 0.04; e.rules.allowHold = true;
  e.spawn('I');
  let rotations = 0, golden = false, choice = null;
  const origHold = e.holdPiece.bind(e);
  e.holdPiece = () => { choice = 'loop'; return false; };
  mode.hooks.onLock = (d) => {
    if (choice) return;
    const filled = e.fullRows().length;
    choice = golden ? 'true' : (d.cells.some(([x]) => x === 4) && d.cells.every(([x]) => x === 4) ? 'peace' : 'loop');
    void filled;
  };
  const prevListener = e.listener;
  e.listener = (ev, d) => {
    prevListener(ev, d);
    if (ev === 'rotate' && allSecrets && !golden) {
      rotations++;
      if (rotations >= 12) { golden = true; e.active.tag = 'gold'; A.sfx('unlock'); fx.flash(0xffd21f, 0.5); ui.popup('LA VRAIE COULEUR', 'gold'); mode.world.redPulse = 0; }
    }
  };
  mode.hooks.onUpdate = () => { if (golden && e.active) e.active.type = 'I'; };
  let idle = 0;
  await S.until(() => { idle += 1 / 60; if (idle > 45 && e.active) e.rules.gravityMul = 2; return !!choice; });
  e.rules.frozen = true; mode.controlsEnabled = false; void origHold;
  await S.wait(0.8);
  A.stopLayers();
  e.spawnPending = false; e.active = null;
  if (choice === 'peace' || choice === 'true') { A.sfx('tetris'); await S.wait(1.5); }
  await ending(game, choice);
}

async function ending(game, kind) {
  const S = game.sched, A = game.audio, fx = game.fx, ui = game.ui, w = game.world;
  const e = game.mode.engine;
  if (kind === 'peace') {
    A.playMusic('ending');
    w.setTheme({ ...w.theme, bg: 0x10203a, fog: 0x10203a, rim1: 0x88aaff, rim2: 0xffc0e0, panel: 0x18284a, hemiSky: 0xc0d0ff, hemiGround: 0x302040 });
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const c = e.grid[y][x]; if (c && c.t === 'X') { c.t = 'G'; } }
    game.say('Merci. Je peux enfin dormir.', { dur: 4, cls: 'white' });
    await S.wait(4);
    game.say('FIN — LE REPOS', { dur: 4, cls: 'white giant' });
  } else if (kind === 'true') {
    A.playMusic('ending');
    w.applyCosmetics();
    const types = ['I', 'O', 'T', 'S', 'Z', 'J', 'L'];
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const c = e.grid[y][x]; if (c) { c.t = types[(x + y) % 7]; c.face = (x + y) % 9 === 0 ? 'cute' : undefined; w.pulse[y][x] = 1; } }
    w.corruption = 0;
    fx.flash(0xffffff, 0.8);
    game.say('Toutes ces couleurs… c\'était moi ?', { dur: 4, cls: 'white' });
    await S.wait(4.2);
    game.say('Merci de m\'avoir rendu mes couleurs.', { dur: 4, cls: 'white' });
    await S.wait(4.2);
    game.say('FIN — LA VRAIE COULEUR', { dur: 4, cls: 'white giant' });
    game.save.p.corruption = 0; game.save.p.healed = true;
  } else {
    // la boucle : tu deviens le monstre
    A.sfx('stinger');
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const c = e.grid[y][x]; if (c) c.face = (x * 7 + y) % 11 === 0 ? 'stare' : undefined; }
    w.corruption = 1; fx.red(0.5); fx.glitch(0.5, true);
    game.say('Alors c\'est toi, maintenant.', { dur: 3.5 });
    await S.wait(3.6);
    fx.glitch(0, true); fx.red(0);
    fx.setDark(1);
    await S.wait(1.5);
    fx.setDark(0);
    game.ui.banner('CHAPITRE 1', 'THE GAME', 'Une partie normale. Enfin… presque.');
    A.sfx('ping');
    await S.wait(2.6);
    game.ui.banner(null);
    game.say('FIN — LA BOUCLE', { dur: 4, cls: 'giant' });
  }
  game.save.addEnding(kind);
  game.save.completeChapter(7);
  game.checkUnlocks();
  await S.wait(4);
  fx.clearAll(); A.stopLayers();
  ui.modal({
    title: 'Merci d\'avoir joué', text: 'THE LAST BLOCK — une création SuperEssence Studio.' + (kind !== 'true' ? '<br><br>D\'autres fins existent…' : ''),
    buttons: [{ label: 'Menu principal', primary: true, act: () => { ui.modal(null); game.showMenu(); } }, { label: 'Crédits', act: () => { ui.modal(null); game.showMenu(); game.ui.panel('credits'); } }],
  });
}

export const CHAPTER_FNS = { 1: chapter1, 2: chapter2, 3: chapter3, 4: chapter4, 5: chapter5, 6: chapter6, 7: chapter7 };
export { stepLiving };
