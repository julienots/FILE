import * as THREE from 'three';
import { U } from './shared.js';
import { mulberry32 } from './noise.js';
import { DOME_R, WORLD_R } from './terrain.js';

/*
 * GAMEPLAY : quêtes, objets à ramasser, inventaire, créatures hostiles, combat, météo, sauvegarde.
 * Tout vit dans la même scène que le mode Créateur : ce que le Créateur change, l'habitant le retrouve.
 */
const rnd = mulberry32(909);

export const ITEMS = {
  apple: { name: 'Pomme', icon: '🍎', desc: 'Rend 1 segment de vie.' },
  shard: { name: 'Éclat de voûte', icon: '🔷', desc: 'Un fragment lumineux tombé du dôme.' },
  herb: { name: 'Herbe sauvage', icon: '🌿', desc: 'Utilisée par les guérisseurs.' },
  essence: { name: 'Essence d\'ombre', icon: '🟣', desc: 'Reste d\'un Ombrelin vaincu.' },
  gold: { name: 'Pièces', icon: '🪙', desc: 'Monnaie de Clairval.' },
};

const QUESTS = {
  main: {
    title: 'Les Éclats de la Voûte', giver: 'Vieil Oswin', kind: 'main',
    offer: ['Ah… toi aussi, tu l\'as vu ? La voûte du ciel se fissure.', 'Trois éclats en sont tombés : au pied de la grande cascade, au cœur de la forêt de l\'est, et sur le haut plateau du nord.', 'Rapporte-les-moi. Peut-être comprendrons-nous qui nous observe là-haut.'],
    steps: ['Trouver les 3 éclats de voûte', 'Rapporter les éclats au Vieil Oswin'],
    need: { shard: 3 },
    progress: ['Les éclats brillent d\'une lumière bleue. Tu ne peux pas les manquer.'],
    done: ['Ils sont tièdes… comme si quelqu\'un les avait tenus dans sa main.', 'Garde ceci, voyageur : ta force grandit. (+2 segments de vie)'],
  },
  apples: {
    title: 'Les pommes de Brune', giver: 'Brune', kind: 'side',
    offer: ['Mon étal est vide ! Les pommiers près du moulin débordent de fruits.', 'Rapporte-m\'en 5 et je te paierai bien.'],
    steps: ['Cueillir 5 pommes près du moulin', 'Rapporter les pommes à Brune'],
    need: { apple: 5 },
    progress: ['Les pommiers sont à l\'ouest, autour du moulin.'],
    done: ['Merveilleux ! Voilà 30 pièces, et une pomme pour la route.'],
  },
  gloom: {
    title: 'Ombres dans la forêt', giver: 'Garrick', kind: 'side',
    offer: ['Des Ombrelins rôdent dans la forêt de l\'est. Des boules de ténèbres aux yeux violets.', 'Frappe-les avec ton bâton. Débarrasse-nous de 4 d\'entre eux.'],
    steps: ['Vaincre 4 Ombrelins dans la forêt de l\'est', 'Revenir voir Garrick'],
    need: { kills: 4 },
    progress: ['Ils viennent de la forêt, à l\'est de la rivière. Méfie-toi.'],
    done: ['Tu es plus solide que tu n\'en as l\'air. Prends ces 50 pièces.'],
  },
};

const SHARD_SPOTS = [[3, -40], [58, 4], [-14, -66]];

export function createGame(ctx) {
  const { scene, terrain, world, player, villagers, T, solve, ui } = ctx;
  const state = load() || { inv: {}, quests: {}, kills: 0, hp: 8, maxHp: 8, builds: [], weather: 'clear', shards: [] };
  const save = () => { try { localStorage.setItem('goddome-save', JSON.stringify(state)); } catch (e) { /* stockage indisponible */ } };
  function load() { try { return JSON.parse(localStorage.getItem('goddome-save')); } catch (e) { return null; } }

  // ---------------------------------------------------------- objets à ramasser
  const pickups = [];
  const glowMat = (c) => new THREE.SpriteMaterial({ map: T.glow, color: c, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: 0.8, fog: false });
  const appleGeo = new THREE.SphereGeometry(0.13, 12, 10), appleMat = new THREE.MeshStandardMaterial({ color: '#c8281e', roughness: 0.35 });
  const shardGeo = new THREE.OctahedronGeometry(0.35, 0), shardMat = new THREE.MeshStandardMaterial({ color: '#bff4ff', emissive: '#38cfff', emissiveIntensity: 2.5, roughness: 0.1, metalness: 0.2, transparent: true, opacity: 0.9 });
  const herbGeo = new THREE.ConeGeometry(0.12, 0.35, 5), herbMat = new THREE.MeshStandardMaterial({ color: '#5fae3a', roughness: 0.7 });
  function addPickup(type, x, z, extra = {}) {
    const g = new THREE.Group();
    let mesh;
    if (type === 'apple') { mesh = new THREE.Mesh(appleGeo, appleMat); const leaf = new THREE.Mesh(herbGeo, herbMat); leaf.scale.setScalar(0.4); leaf.position.y = 0.14; mesh.add(leaf); }
    if (type === 'shard') { mesh = new THREE.Mesh(shardGeo, shardMat); mesh.scale.set(1, 1.8, 1); const s = new THREE.Sprite(glowMat('#6fe0ff')); s.scale.setScalar(3.2); g.add(s); const beam = new THREE.Sprite(glowMat('#6fe0ff')); beam.scale.set(0.6, 14, 1); beam.position.y = 6; g.add(beam); }
    if (type === 'herb') { mesh = new THREE.Mesh(herbGeo, herbMat); }
    mesh.castShadow = true; g.add(mesh);
    const y = world.groundAt(x, z);
    g.position.set(x, y + (type === 'shard' ? 1.1 : 0.25), z);
    scene.add(g);
    const p = { type, g, mesh, base: g.position.y, t: rnd() * 6, ...extra };
    pickups.push(p);
    return p;
  }
  // pommes au verger du moulin, herbes un peu partout
  for (let i = 0; i < 14; i++) { const a = rnd() * 6.28, r = 4 + rnd() * 12; const x = -58 + Math.cos(a) * r, z = -6 + Math.sin(a) * r; if (terrain.vegetationOK(x, z, 0.5)) addPickup('apple', x, z); }
  for (let i = 0; i < 22; i++) { const a = rnd() * 6.28, r = 20 + rnd() * 80; const x = Math.cos(a) * r, z = Math.sin(a) * r; if (terrain.vegetationOK(x, z, 0.4)) addPickup('herb', x, z); }
  function spawnShards() {
    SHARD_SPOTS.forEach(([x, z], i) => { if (!state.shards.includes(i) && !pickups.some(p => p.shardId === i)) addPickup('shard', x, z, { shardId: i }); });
  }
  if (state.quests.main?.stage === 1) spawnShards();

  // ---------------------------------------------------------- créatures : Ombrelins
  const enemies = [];
  const gloomMat = new THREE.MeshStandardMaterial({ color: '#2a1838', roughness: 0.25, metalness: 0.1, emissive: '#3a1050', emissiveIntensity: 0.6 });
  const eyeMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(3, 1.2, 4), fog: false });
  const ENEMY_ZONES = [[60, 0, 22], [48, -28, 14], [70, 28, 14]];
  function spawnEnemy(x, z) {
    const g = new THREE.Group();
    const body = new THREE.Mesh(new THREE.SphereGeometry(0.55, 20, 14), gloomMat.clone()); body.castShadow = true; body.position.y = 0.5; g.add(body);
    for (const sx of [-1, 1]) { const e = new THREE.Mesh(new THREE.SphereGeometry(0.08, 8, 6), eyeMat); e.position.set(sx * 0.18, 0.62, 0.46); g.add(e); }
    const aura = new THREE.Sprite(glowMat('#9a4dff')); aura.material.opacity = 0.35; aura.scale.setScalar(2.4); aura.position.y = 0.5; g.add(aura);
    g.position.set(x, world.groundAt(x, z), z);
    scene.add(g);
    const e = { g, body, hp: 3, t: rnd() * 5, home: new THREE.Vector2(x, z), target: null, hit: 0, cd: 0, dead: 0, vx: 0, vz: 0 };
    enemies.push(e);
    return e;
  }
  ENEMY_ZONES.forEach(([cx, cz, r]) => { for (let i = 0; i < 3; i++) { const a = rnd() * 6.28, d = rnd() * r; spawnEnemy(cx + Math.cos(a) * d, cz + Math.sin(a) * d); } });

  // ---------------------------------------------------------- particules d'impact
  const bursts = [];
  function burst(pos, color = '#7fe3ff', n = 14, size = 0.6) {
    for (let i = 0; i < n; i++) {
      const s = new THREE.Sprite(glowMat(color)); s.scale.setScalar(size); s.position.copy(pos);
      scene.add(s);
      bursts.push({ s, v: new THREE.Vector3((rnd() - 0.5) * 6, rnd() * 5 + 1, (rnd() - 0.5) * 6), life: 0.8 + rnd() * 0.4 });
    }
  }

  // ---------------------------------------------------------- météo (pluie dans le dôme)
  const RAIN = 7000;
  const rainGeo = new THREE.BufferGeometry();
  const rp = new Float32Array(RAIN * 3); for (let i = 0; i < RAIN; i++) { rp[i * 3] = rnd(); rp[i * 3 + 1] = rnd(); rp[i * 3 + 2] = rnd(); }
  rainGeo.setAttribute('position', new THREE.BufferAttribute(rp, 3));
  const rainMat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false,
    uniforms: { uTime: U.uTime, uCenter: { value: new THREE.Vector3() }, uArea: { value: 60 }, uIntensity: { value: 0 } },
    vertexShader: `uniform float uTime; uniform vec3 uCenter; uniform float uArea; varying float vA;
      void main(){ vec3 p = position;
        float fall = fract(p.y - uTime * (0.9 + p.x * 0.2) * (40.0 / uArea));
        vec3 w = vec3((p.x - 0.5) * uArea + uCenter.x, fall * uArea * 0.7 + uCenter.y - uArea * 0.15, (p.z - 0.5) * uArea + uCenter.z);
        vA = step(length(w.xz), ${(DOME_R - 4).toFixed(1)}) * step(length(w), ${(DOME_R - 2).toFixed(1)});
        vec4 mv = modelViewMatrix * vec4(w, 1.0); gl_Position = projectionMatrix * mv; gl_PointSize = min(40.0, 260.0 / -mv.z); }`,
    fragmentShader: `uniform float uIntensity; varying float vA;
      void main(){ vec2 c = gl_PointCoord - 0.5; float a = smoothstep(0.035, 0.0, abs(c.x)) * smoothstep(0.5, 0.1, abs(c.y)); gl_FragColor = vec4(vec3(0.75, 0.82, 0.9), a * 0.45 * uIntensity * vA); }`,
  });
  const rain = new THREE.Points(rainGeo, rainMat); rain.frustumCulled = false; rain.renderOrder = 6; scene.add(rain);
  let rainLevel = state.weather === 'rain' ? 1 : 0;

  // ---------------------------------------------------------- quêtes
  const npcByName = n => villagers.list.find(v => v.name === n);
  const questFor = npc => Object.entries(QUESTS).find(([, q]) => q.giver === npc.name);
  function count(item) { return state.inv[item] || 0; }
  function give(item, n = 1) { state.inv[item] = count(item) + n; if (state.inv[item] <= 0) delete state.inv[item]; save(); ui.refreshInventory(); }
  function questReady(id) {
    const q = QUESTS[id];
    if (q.need.kills) return state.kills - (state.quests[id].killBase || 0) >= q.need.kills;
    return Object.entries(q.need).every(([k, n]) => count(k) >= n);
  }
  function objective() {
    const order = ['main', 'gloom', 'apples'];
    for (const id of order) {
      const s = state.quests[id];
      if (!s || s.stage >= 2) continue;
      const q = QUESTS[id];
      if (questReady(id)) return `${q.title} · ${q.steps[1]}`;
      let extra = '';
      if (q.need.shard) extra = ` (${count('shard')}/3)`;
      if (q.need.apple) extra = ` (${Math.min(5, count('apple'))}/5)`;
      if (q.need.kills) extra = ` (${Math.min(4, state.kills - (s.killBase || 0))}/4)`;
      return `${q.title} · ${q.steps[0]}${extra}`;
    }
    if (!state.quests.main) return 'Parlez au Vieil Oswin, près du puits';
    return 'Explorez votre monde';
  }
  function linesFor(npc) {
    const qe = questFor(npc);
    if (!qe) return null;
    const [id, q] = qe, s = state.quests[id];
    if (!s) return q.offer;
    if (s.stage === 1) return questReady(id) ? q.done : q.progress;
    return null;
  }
  function dialogEnd(npc) {
    const qe = questFor(npc);
    if (!qe) return;
    const [id, q] = qe, s = state.quests[id];
    if (!s) {
      state.quests[id] = { stage: 1, killBase: state.kills };
      ui.toast(`Nouvelle quête : ${q.title}`, q.kind === 'main' ? 'main' : 'side');
      if (id === 'main') spawnShards();
    } else if (s.stage === 1 && questReady(id)) {
      s.stage = 2;
      if (id === 'main') { give('shard', -3); state.maxHp += 2; state.hp = state.maxHp; }
      if (id === 'apples') { give('apple', -5); give('gold', 30); give('apple', 1); }
      if (id === 'gloom') give('gold', 50);
      ui.toast(`Quête accomplie : ${q.title}`, 'done');
    }
    save(); ui.refreshQuests(); ui.refreshLife();
  }
  // marqueurs « ! » au-dessus des donneurs de quête
  const markers = Object.values(QUESTS).map(q => {
    const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: makeMarkTex(), depthWrite: false, transparent: true, fog: false }));
    s.scale.set(0.6, 0.6, 1); scene.add(s); return { q, s, npc: npcByName(q.giver) };
  });
  function makeMarkTex() {
    const c = document.createElement('canvas'); c.width = c.height = 64; const g = c.getContext('2d');
    g.fillStyle = '#ffd27a'; g.strokeStyle = '#3a2410'; g.lineWidth = 6; g.font = 'bold 54px Georgia'; g.textAlign = 'center';
    g.strokeText('!', 32, 52); g.fillText('!', 32, 52);
    const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
  }

  // ---------------------------------------------------------- combat
  let attackCd = 0;
  function attack() {
    if (attackCd > 0) return;
    attackCd = 0.55;
    player.state.attack = 1;
    const p = player.root.position, yaw = player.root.rotation.y;
    const front = new THREE.Vector3(p.x + Math.sin(yaw) * 1.3, p.y + 1, p.z + Math.cos(yaw) * 1.3);
    setTimeout(() => burst(front, '#7fe3ff', 10, 0.7), 140);
    for (const e of enemies) {
      if (e.dead) continue;
      const d = e.g.position.distanceTo(front);
      if (d < 2.0) {
        e.hp--; e.hit = 0.25;
        const k = new THREE.Vector2(e.g.position.x - p.x, e.g.position.z - p.z).normalize();
        e.vx = k.x * 9; e.vz = k.y * 9;
        if (e.hp <= 0) {
          e.dead = 1; state.kills++;
          burst(e.g.position.clone().add(new THREE.Vector3(0, 0.5, 0)), '#b06bff', 22, 0.9);
          if (rnd() < 0.7) give('essence', 1);
          ui.toast('Ombrelin vaincu', 'side');
          ui.refreshQuests(); save();
        }
      }
    }
  }
  let invuln = 0;
  function hurt(n, from) {
    if (invuln > 0) return;
    state.hp = Math.max(0, state.hp - n); invuln = 1.0;
    ui.refreshLife(); ui.hurtFlash();
    if (from) { const p = player.root.position, k = new THREE.Vector2(p.x - from.x, p.z - from.z).normalize(); const s = solve(p.x + k.x * 1.5, p.z + k.y * 1.5, 0.35); p.x = s.x; p.z = s.z; }
    if (state.hp <= 0) {
      ui.toast('Vous vous réveillez au village…', 'main');
      state.hp = state.maxHp; ui.refreshLife();
      ctx.respawn();
    }
    save();
  }

  // ---------------------------------------------------------- interaction contextuelle
  function nearestPickup() {
    const p = player.root.position; let best = null, bd = 2.0;
    for (const k of pickups) { const d = k.g.position.distanceTo(p) - (k.type === 'shard' ? 0.8 : 0); if (d < bd) { bd = d; best = k; } }
    return best;
  }
  function collect(k) {
    scene.remove(k.g); pickups.splice(pickups.indexOf(k), 1);
    give(k.type, 1);
    if (k.type === 'shard') { state.shards.push(k.shardId); burst(k.g.position, '#7fe3ff', 26, 1.0); ui.toast(`Éclat de voûte trouvé (${count('shard')}/3)`, 'main'); }
    else ui.toast(`+1 ${ITEMS[k.type].name}`, 'item');
    ui.refreshQuests(); save();
  }
  function useItem(id) {
    if (id === 'apple' && count('apple') > 0 && state.hp < state.maxHp) { give('apple', -1); state.hp = Math.min(state.maxHp, state.hp + 1); ui.refreshLife(); save(); return true; }
    if (id === 'herb' && count('herb') > 0 && state.hp < state.maxHp) { give('herb', -1); state.hp = Math.min(state.maxHp, state.hp + 2); ui.refreshLife(); save(); return true; }
    return false;
  }

  // ---------------------------------------------------------- boucle
  function update(dt, mode, time) {
    attackCd = Math.max(0, attackCd - dt); invuln = Math.max(0, invuln - dt);
    // objets
    for (const k of pickups) { k.t += dt; k.mesh.rotation.y += dt * (k.type === 'shard' ? 1.5 : 0.6); k.g.position.y = k.base + Math.sin(k.t * 2) * (k.type === 'shard' ? 0.2 : 0.04); }
    // particules
    for (let i = bursts.length - 1; i >= 0; i--) {
      const b = bursts[i]; b.life -= dt; b.v.y -= 9 * dt; b.s.position.addScaledVector(b.v, dt); b.s.material.opacity = Math.max(0, b.life);
      if (b.life <= 0) { scene.remove(b.s); b.s.material.dispose(); bursts.splice(i, 1); }
    }
    // créatures
    const p = player.root.position, playing = mode === 'play';
    for (const e of enemies) {
      const g = e.g;
      if (e.dead) {
        e.dead += dt; g.scale.setScalar(Math.max(0.001, 1 - e.dead * 3));
        if (e.dead > 45) { e.dead = 0; e.hp = 3; g.position.set(e.home.x, world.groundAt(e.home.x, e.home.y), e.home.y); g.scale.setScalar(1); }
        continue;
      }
      e.t += dt; e.hit = Math.max(0, e.hit - dt);
      const dx = p.x - g.position.x, dz = p.z - g.position.z, d = Math.hypot(dx, dz);
      let mx = 0, mz = 0, sp = 0;
      if (playing && d < 13 && Math.abs(p.y - g.position.y) < 4) { mx = dx / d; mz = dz / d; sp = 2.7; }
      else {
        if (!e.target || rnd() < dt * 0.2) { const a = rnd() * 6.28, r = rnd() * 6; e.target = new THREE.Vector2(e.home.x + Math.cos(a) * r, e.home.y + Math.sin(a) * r); }
        const tx = e.target.x - g.position.x, tz = e.target.y - g.position.z, td = Math.hypot(tx, tz);
        if (td > 0.5) { mx = tx / td; mz = tz / td; sp = 1.0; }
      }
      const hop = Math.abs(Math.sin(e.t * (sp > 2 ? 7 : 4)));
      const nx = g.position.x + (mx * sp * hop * 1.6 + e.vx) * dt, nz = g.position.z + (mz * sp * hop * 1.6 + e.vz) * dt;
      e.vx *= Math.pow(0.02, dt); e.vz *= Math.pow(0.02, dt);
      const s = solve(nx, nz, 0.5); g.position.x = s.x; g.position.z = s.z;
      g.position.y = world.groundAt(s.x, s.z) + hop * 0.35 * (sp > 0 ? 1 : 0.3);
      if (sp > 0) g.rotation.y = Math.atan2(mx, mz);
      e.body.scale.set(1 + (1 - hop) * 0.15, 1 - (1 - hop) * 0.18, 1 + (1 - hop) * 0.15);
      e.body.material.emissive.set(e.hit > 0 ? '#ffffff' : '#3a1050');
      e.body.material.emissiveIntensity = e.hit > 0 ? 3 : 0.6;
      e.cd = Math.max(0, e.cd - dt);
      if (playing && d < 1.1 && e.cd <= 0) { e.cd = 1.2; hurt(1, g.position); }
    }
    // marqueurs de quête
    for (const m of markers) {
      if (!m.npc) continue;
      const qe = questFor(m.npc), s = qe && state.quests[qe[0]];
      const show = !s || (s.stage === 1 && questReady(qe[0]));
      m.s.visible = show;
      m.s.material.color.set(s ? '#7fe3ff' : '#ffd27a');
      m.s.position.copy(m.npc.root.position).add(new THREE.Vector3(0, m.npc.height + 0.55 + Math.sin(time * 3) * 0.08, 0));
    }
    // météo
    const target = state.weather === 'rain' ? 1 : 0;
    rainLevel += (target - rainLevel) * Math.min(1, dt * 0.6);
    rainMat.uniforms.uIntensity.value = rainLevel;
    rain.visible = rainLevel > 0.01;
    const inside = U.uInside.value;
    rainMat.uniforms.uArea.value = THREE.MathUtils.lerp(DOME_R * 2, 60, inside);
    rainMat.uniforms.uCenter.value.copy(inside > 0.5 ? ctx.camera.position : new THREE.Vector3(0, 30, 0));
    U.uWind.value = 1 + rainLevel * 1.2;
    // interaction possible ?
    if (playing) {
      const pk = nearestPickup();
      ui.setAction(pk ? `Ramasser` : null, pk);
    }
  }

  return {
    _enemies: enemies, state, update, attack, linesFor, dialogEnd, objective, useItem, collect, nearestPickup, count, save, give,
    get rain() { return rainLevel; },
    setWeather(w) { state.weather = w; save(); },
    quests: () => Object.entries(QUESTS).map(([id, q]) => ({ id, ...q, state: state.quests[id] })),
  };
}
