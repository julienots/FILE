// Événements dynamiques : fêtes, anniversaires, concours, courses, jeux collectifs,
// réunions de groupe, visites de voisinage, accidents et découvertes.
import { clamp, HALF, SIDES } from './constants.js';
import { rand, rrange, pick, chance, newId } from './rng.js';
import { abortAct, bubble, addItem, applyGains, gainSkill, setAnim, followPath, moveToward, turnTo, available } from './ai.js';
import { buildPath, cubeDistances } from './nav.js';
import { relate, remember, feel, mood, groupOf } from './social.js';
import { ITEMS } from './themes.js';

const EVENT_LABELS = {
  party: 'Fête', birthday: 'Anniversaire', contest: 'Concours', race: 'Course', hideGroup: 'Cache-cache géant',
  meeting: 'Réunion', visitGroup: 'Visite de voisinage', sport: 'Compétition sportive',
};
export { EVENT_LABELS };

function enlist(world, ev, ch, label) {
  abortAct(world, ch);
  ch.act = { kind: 'event', eventId: ev.id, label, phase: 'idle', t: 0, wi: 0, path: null };
  ev.parts.push(ch.id);
}
function sendTo(world, ch, cubeId, x, z, run = false) {
  const path = cubeId === ch.cubeId ? [{ cube: cubeId, x, z, kind: 'walk' }] : buildPath(world, ch, cubeId, x, z);
  if (!path) return false;
  Object.assign(ch.act, { path, wi: 0, phase: 'go', run });
  return true;
}
function release(world, ev) {
  for (const id of ev.parts) { const c = world.chars.get(id); if (c && c.act && c.act.eventId === ev.id) { c.act = null; c.y = 0; } }
}
function parts(world, ev) { return ev.parts.map((id) => world.chars.get(id)).filter((c) => c && c.act && c.act.eventId === ev.id); }
// Fait avancer les participants vers leur destination. Renvoie le nombre arrivé.
function moveParts(world, ev, list, dt) {
  let arrived = 0;
  for (const c of list) {
    const a = c.act;
    if (a.phase === 'go') {
      const r = followPath(world, c, a, dt);
      if (r === 'done') { a.phase = 'there'; a.arrivedAt = ev.t; }
      else if (r === 'fail') { a.phase = 'there'; a.failed = true; }
    } else arrived++;
  }
  return arrived;
}
function ring(n, i, r = 0.75, cx = 0, cz = 0) { const a = (i / n) * Math.PI * 2; return [cx + Math.sin(a) * r, cz + Math.cos(a) * r]; }
function pairBond(world, list, amt) {
  for (let i = 0; i < list.length; i++) for (let j = i + 1; j < Math.min(list.length, i + 4); j++) relate(world, list[i], list[j], amt, 2, false);
}

export class EventManager {
  constructor(world) { this.world = world; this.timer = 0; }

  update(dt, activeCubes) {
    const w = this.world;
    for (const ev of [...w.events]) {
      ev.t += dt;
      const done = this.tick(ev, dt);
      if (done || ev.t > 90) { release(w, ev); w.events.splice(w.events.indexOf(ev), 1); }
    }
    this.timer += dt;
    if (this.timer < 4) return;
    this.timer = 0;
    const maxEvents = Math.max(1, Math.floor(activeCubes.length / 5));
    if (w.events.length >= maxEvents) return;
    // anniversaires d'abord (dépendent du calendrier)
    const dayIdx = Math.floor(w.day) % 12; const dayNum = Math.floor(w.day);
    for (const cube of activeCubes) {
      for (const id of cube.chars) {
        const ch = w.chars.get(id);
        if (ch && ch.bday === dayIdx && ch.lastBday !== dayNum && !w.isNight && available(ch) && rand() < 0.3) { ch.lastBday = dayNum; this.start('birthday', cube, ch); return; }
      }
    }
    const cube = pick(activeCubes); if (!cube) return;
    if (cube._lastEvent && w.time - cube._lastEvent < 70) return;
    const free = [...cube.chars].map((id) => w.chars.get(id)).filter((c) => c && available(c));
    const r = rand();
    const avgMood = free.reduce((s, c) => s + mood(c), 0) / Math.max(1, free.length);
    if (free.length >= 4 && avgMood > 0.55 && r < 0.10) return this.start('party', cube);
    if (free.length >= 3 && r < 0.16) return this.start('contest', cube);
    if (free.length >= 3 && r < 0.21) return this.start('race', cube);
    if (free.length >= 4 && r < 0.26) return this.start('hideGroup', cube);
    if (r < 0.31) return this.start('meeting', cube);
    if (free.length >= 2 && r < 0.36) return this.start('visitGroup', cube);
    if (free.length >= 1 && r < 0.43) return this.accident(cube, free);
    if (free.length >= 1 && r < 0.48) return this.discovery(cube, free);
  }

  start(type, cube, honoree = null) {
    const w = this.world;
    const ev = { id: newId('e'), type, cubeId: cube.id, parts: [], t: 0, phase: 0, data: {} };
    const free = [...cube.chars].map((id) => w.chars.get(id)).filter((c) => c && available(c) && c !== honoree);
    const ok = this['start_' + type](ev, cube, free, honoree);
    if (!ok) { release(w, ev); return null; }
    cube._lastEvent = w.time;
    w.events.push(ev);
    return ev;
  }

  // ----- Fête -----
  start_party(ev, cube, free) {
    const w = this.world; const list = free.slice(0, 12);
    if (list.length < 3) return false;
    list.forEach((c, i) => { enlist(w, ev, c, 'fait la fête'); const [x, z] = ring(list.length, i, 0.55 + rand() * 0.6); sendTo(w, c, cube.id, x, z); });
    w.addLog('événement', '🎉', `Une fête éclate dans « ${cube.name} » (${list.length} invités)`, cube.id, list.map((c) => c.id));
    bubble(list[0], '🎉', 3);
    return true;
  }
  tick_party(ev, dt) {
    const w = this.world; const list = parts(w, ev); if (!list.length) return true;
    moveParts(w, ev, list, dt);
    for (const c of list) if (c.act.phase === 'there') setAnim(c, ev.t > 22 ? 'cheer' : (Math.floor(ev.t / 6 + c.id.length) % 3 === 0 ? 'jump' : 'dance'));
    if (Math.floor(ev.t) % 2 === 0 && Math.floor(ev.t - dt) % 2 !== 0) w.fx.push({ type: 'confetti', cube: ev.cubeId, x: rrange(-1, 1), z: rrange(-1, 1), t: 0, dur: 2.5 });
    if (ev.t > 25) {
      for (const c of list) { applyGains(c, { fun: 0.5, social: 0.4 }); feel(c, 'joy', 0.5); remember(w, c, 'A participé à une super fête', 1); gainSkill(c, 'musique', 0.2); }
      pairBond(w, list, 5);
      return true;
    }
    return false;
  }

  // ----- Anniversaire -----
  start_birthday(ev, cube, free, hero) {
    const w = this.world;
    ev.data.hero = hero.id;
    enlist(w, ev, hero, 'fête son anniversaire');
    sendTo(w, hero, cube.id, 0, 0);
    // invités : amis proches (jusqu'à 4 cubes) + présents dans le cube
    const dist = cubeDistances(w, cube.id, 4);
    const guests = [];
    for (const id in hero.rel) {
      const o = w.chars.get(id);
      if (o && hero.rel[id].a > 25 && dist.has(o.cubeId) && available(o) && w.cubes.get(o.cubeId).awake) guests.push(o);
    }
    for (const c of free) if (!guests.includes(c) && guests.length < 10) guests.push(c);
    const list = guests.slice(0, 10);
    if (!list.length) { bubble(hero, '🎂', 3); remember(w, hero, 'A fêté son anniversaire tout seul', 0); return false; }
    list.forEach((c, i) => { enlist(w, ev, c, `fête l'anniversaire de ${hero.name}`); const [x, z] = ring(list.length, i, 0.6); sendTo(w, c, cube.id, x, z); });
    w.addLog('événement', '🎂', `C'est l'anniversaire de ${hero.name} ! ${list.length} invité(s) arrivent`, cube.id, [hero.id, ...list.map((c) => c.id)]);
    remember(w, hero, `Anniversaire fêté avec ${list.length} invité(s)`, 1);
    bubble(hero, '🎂', 4);
    return true;
  }
  tick_birthday(ev, dt) {
    const w = this.world; const list = parts(w, ev);
    const hero = w.chars.get(ev.data.hero); if (!hero || !list.includes(hero)) return true;
    const guests = list.filter((c) => c !== hero);
    moveParts(w, ev, list, dt);
    if (hero.act.phase === 'there') { setAnim(hero, ev.t < 30 ? 'idle' : 'cheer'); }
    const allThere = guests.every((g) => g.act.phase === 'there') || ev.t > 30;
    if (!ev.data.cake && hero.act.phase === 'there' && allThere) { ev.data.cake = ev.t; w.fx.push({ type: 'cake', cube: hero.cubeId, x: hero.x + 0.25, z: hero.z, t: 0, dur: 16 }); }
    for (const g of guests) if (g.act.phase === 'there') { turnTo(g, hero.x, hero.z, dt); setAnim(g, ev.data.cake ? 'cheer' : 'wave'); }
    if (ev.data.cake) {
      const k = ev.t - ev.data.cake;
      setAnim(hero, k < 5 ? 'cheer' : k < 10 ? 'laugh' : 'dance');
      if (k > 5 && !ev.data.gifts) {
        ev.data.gifts = true;
        for (const g of guests) {
          const idx = g.inventory.findIndex((it) => ITEMS[it.type]?.gift);
          const type = idx >= 0 ? g.inventory.splice(idx, 1)[0].type : 'cadeau';
          addItem(w, hero, type);
          relate(w, g, hero, 6, 3, true);
        }
        bubble(hero, '🎁', 3); feel(hero, 'joy', 0.8);
      }
      if (k > 12) {
        for (const g of guests) { applyGains(g, { fun: 0.3, social: 0.4 }); remember(w, g, `A fêté l'anniversaire de ${hero.name}`, 1, hero.id); }
        applyGains(hero, { fun: 0.5, social: 0.6 });
        return true;
      }
    }
    return false;
  }

  // ----- Concours (dessin, cuisine, construction, danse, sport) -----
  start_contest(ev, cube, free) {
    const w = this.world;
    const kinds = [];
    const has = (t) => cube.objects.some((o) => o.type === t);
    if (has('easel') || has('blackboard')) kinds.push(['dessin', 'draw', 'art']);
    if (has('stove')) kinds.push(['cuisine', 'cook', 'cuisine']);
    if (has('workbench') || has('sandcastle')) kinds.push(['construction', 'build', 'construction']);
    if (has('jukebox') || has('piano')) kinds.push(['danse', 'dance', 'musique']);
    if (has('weights') || has('treadmill') || has('pool')) kinds.push(['haltérophilie', 'lift', 'sport']);
    kinds.push(['danse', 'dance', 'musique'], ['blagues', 'talk', 'social']);
    const [name, anim, skill] = pick(kinds);
    const list = free.slice(0, 6);
    if (list.length < 2) return false;
    ev.data = { name, anim, skill };
    list.forEach((c, i) => { enlist(w, ev, c, `participe au concours de ${name}`); sendTo(w, c, cube.id, -1.2 + (i / Math.max(1, list.length - 1)) * 2.4, 0.9); });
    w.addLog('événement', '🏅', `Concours de ${name} dans « ${cube.name} » (${list.length} participants)`, cube.id, list.map((c) => c.id));
    return true;
  }
  tick_contest(ev, dt) {
    const w = this.world; const list = parts(w, ev); if (list.length < 1) return true;
    moveParts(w, ev, list, dt);
    const { anim, skill, name } = ev.data;
    if (!ev.data.go && (list.every((c) => c.act.phase === 'there') || ev.t > 20)) { ev.data.go = ev.t; list.forEach((c) => bubble(c, '💪', 1.5)); }
    if (!ev.data.go) return false;
    const k = ev.t - ev.data.go;
    for (const c of list) { if (c.act.phase !== 'there') continue; c.ry = Math.PI; if (k < 12) setAnim(c, anim); }
    if (k >= 12 && !ev.data.winner) {
      let best = null, bs = -1;
      for (const c of list) { const s = (c.skills[skill] || 0) + rand() * 3; if (s > bs) { bs = s; best = c; } }
      ev.data.winner = best.id; best.stats.wins++;
      bubble(best, '🏆', 4); feel(best, 'joy', 0.7);
      remember(w, best, `A gagné le concours de ${name}`, 1);
      w.addLog('événement', '🏆', `${best.name} remporte le concours de ${name} !`, ev.cubeId, [best.id]);
      for (const c of list) { gainSkill(c, skill, 0.4); applyGains(c, { fun: 0.3, social: 0.2 }); if (c !== best) { if (c.traits.temper > 0.65 && chance(0.6)) { feel(c, 'anger', 0.3); bubble(c, '😤'); } else { relate(w, c, best, 3, 2, true); } remember(w, c, `A participé au concours de ${name}`, 0); } }
    }
    if (ev.data.winner) {
      for (const c of list) setAnim(c, c.id === ev.data.winner ? 'cheer' : (c.emo.anger > 0.4 ? 'sulk' : 'cheer'));
      return k > 16;
    }
    return false;
  }

  // ----- Course à travers les cubes connectés -----
  start_race(ev, cube, free) {
    const w = this.world;
    // cherche une arrivée dans un autre cube, 1 à 3 cubes plus loin (horizontalement)
    const dist = cubeDistances(w, cube.id, 3);
    const opts = [...dist.keys()].filter((id) => id !== cube.id && w.cubes.get(id).y === cube.y && w.cubes.get(id).awake);
    const list = free.slice(0, 5);
    if (list.length < 2) return false;
    let finish;
    if (opts.length) finish = { cube: opts.sort((a, b) => dist.get(b) - dist.get(a))[0], x: 0, z: 0 };
    else finish = { cube: cube.id, x: 1.4, z: 1.4, local: true };
    ev.data = { finish, rank: [] };
    list.forEach((c, i) => { enlist(w, ev, c, 'participe à la course'); sendTo(w, c, cube.id, -1.4, -1.2 + i * 0.55); });
    const fc = w.cubes.get(finish.cube);
    w.addLog('événement', '🏁', `Course ! ${list.length} coureurs de « ${cube.name} » vers « ${fc.name} »`, cube.id, list.map((c) => c.id));
    return true;
  }
  tick_race(ev, dt) {
    const w = this.world; const list = parts(w, ev); if (list.length < 1) return true;
    const d = ev.data;
    if (!d.go) {
      moveParts(w, ev, list, dt);
      for (const c of list) if (c.act.phase === 'there') { setAnim(c, 'idle'); c.ry = Math.PI / 2; }
      if (list.every((c) => c.act.phase === 'there') || ev.t > 18) {
        d.go = ev.t;
        for (const c of list) {
          bubble(c, '3️⃣', 1);
          const path = buildPath(w, c, d.finish.cube, d.finish.x + rrange(-0.3, 0.3), d.finish.z + rrange(-0.3, 0.3));
          Object.assign(c.act, { path: path || [{ cube: c.cubeId, x: 1.5, z: c.z, kind: 'walk' }], wi: 0, phase: 'wait', run: true });
          c.speedMul = (0.8 + c.traits.energy * 0.3 + Math.min(0.4, (c.skills.sport || 0) * 0.05)) * (0.9 + rand() * 0.2);
        }
      }
      return false;
    }
    const k = ev.t - d.go;
    if (k < 3) { for (const c of list) { setAnim(c, 'idle'); if (Math.floor(k) !== Math.floor(k - dt)) bubble(c, ['3️⃣', '2️⃣', '1️⃣'][Math.min(2, Math.floor(k))], 1); } return false; }
    for (const c of list) {
      if (c.act.phase === 'wait') { c.act.phase = 'go'; bubble(c, '🏃', 1); }
      if (c.act.phase === 'go') {
        const r = followPath(w, c, c.act, dt);
        if (r !== 'moving') { c.act.phase = 'there'; if (r === 'done') d.rank.push(c.id); }
      } else setAnim(c, d.rank[0] === c.id ? 'cheer' : 'idle');
    }
    const finished = list.every((c) => c.act.phase === 'there');
    if ((finished || k > 45) && !d.done) {
      d.done = ev.t;
      for (const c of list) c.speedMul = 0.9 + c.traits.energy * 0.3;
      const win = w.chars.get(d.rank[0]);
      if (win) {
        win.stats.wins++; bubble(win, '🏆', 4); feel(win, 'joy', 0.7); remember(w, win, 'A gagné une course', 1);
        w.addLog('événement', '🥇', `${win.name} gagne la course !${d.rank[1] ? ' 2e : ' + w.chars.get(d.rank[1]).name : ''}`, win.cubeId, d.rank.slice(0, 3));
      }
      for (const c of list) { applyGains(c, { fun: 0.4, energy: -0.1 }); gainSkill(c, 'sport', 0.4); }
      pairBond(w, list, 3);
    }
    if (d.done) { for (const c of list) setAnim(c, c.id === d.rank[0] ? 'cheer' : 'laugh'); return ev.t - d.done > 3; }
    return false;
  }

  // ----- Cache-cache géant -----
  start_hideGroup(ev, cube, free) {
    const w = this.world; const list = free.slice(0, 6);
    if (list.length < 3 || !cube.objects.length) return false;
    const seeker = list[0];
    ev.data = { seeker: seeker.id, found: [] };
    enlist(w, ev, seeker, 'cherche les autres (cache-cache)'); sendTo(w, seeker, cube.id, 0, 0);
    for (const c of list.slice(1)) {
      enlist(w, ev, c, 'se cache (cache-cache)');
      const o = pick(cube.objects); const ang = rand() * Math.PI * 2; const r = 0.3 + rand() * 0.25;
      sendTo(w, c, cube.id, clamp(o.x + Math.sin(ang) * r, -1.7, 1.7), clamp(o.z + Math.cos(ang) * r, -1.7, 1.7), true);
    }
    w.addLog('événement', '🙈', `Cache-cache géant dans « ${cube.name} » : ${seeker.name} compte !`, cube.id, list.map((c) => c.id));
    return true;
  }
  tick_hideGroup(ev, dt) {
    const w = this.world; const list = parts(w, ev);
    const seeker = w.chars.get(ev.data.seeker); if (!seeker || !list.includes(seeker)) return true;
    const hiders = list.filter((c) => c !== seeker);
    moveParts(w, ev, hiders, dt);
    for (const h of hiders) if (h.act.phase === 'there') setAnim(h, ev.data.found.includes(h.id) ? 'laugh' : 'hide');
    if (ev.t < 6) { setAnim(seeker, 'seek'); return false; }
    const left = hiders.filter((h) => !ev.data.found.includes(h.id));
    if (!left.length || ev.t > 60) {
      setAnim(seeker, 'cheer');
      if (!ev.data.end) { ev.data.end = ev.t; for (const c of list) { applyGains(c, { fun: 0.45, social: 0.25 }); remember(w, c, 'A joué à cache-cache géant', 1); } pairBond(w, list, 4); }
      return ev.t - ev.data.end > 3;
    }
    const target = left[0];
    setAnim(seeker, 'walk');
    if (moveToward(w, seeker, target.x, target.z, 0.65 * seeker.speedMul, dt) || Math.hypot(seeker.x - target.x, seeker.z - target.z) < 0.35) {
      ev.data.found.push(target.id); bubble(seeker, '👀', 1.2); bubble(target, '😆', 1.5);
    }
    return false;
  }

  // ----- Réunion de groupe -----
  start_meeting(ev, cube) {
    const w = this.world;
    // un groupe dont un membre est ici
    const here = [...cube.chars].map((id) => w.chars.get(id)).filter(Boolean);
    let group = null;
    for (const c of here) { group = groupOf(w, c.id); if (group) break; }
    if (!group) return false;
    const dist = cubeDistances(w, cube.id, 4);
    const list = group.members.map((id) => w.chars.get(id)).filter((c) => c && available(c) && dist.has(c.cubeId) && w.cubes.get(c.cubeId).awake).slice(0, 10);
    if (list.length < 3) return false;
    ev.data = { group: group.name };
    list.forEach((c, i) => { enlist(w, ev, c, `réunion « ${group.name} »`); const [x, z] = ring(list.length, i, 0.6); sendTo(w, c, cube.id, x, z); });
    w.addLog('événement', '👥', `Réunion du groupe « ${group.name} » dans « ${cube.name} »`, cube.id, list.map((c) => c.id));
    return true;
  }
  tick_meeting(ev, dt) {
    const w = this.world; const list = parts(w, ev); if (list.length < 2) return true;
    moveParts(w, ev, list, dt);
    const there = list.filter((c) => c.act.phase === 'there');
    if (!ev.data.go && (there.length === list.length || ev.t > 35)) ev.data.go = ev.t;
    for (const c of there) { turnTo(c, 0, 0, dt, 4); setAnim(c, ev.data.go ? (ev.t - ev.data.go < 10 ? (Math.floor((ev.t + c.name.length) / 2.5) % 2 ? 'talk' : 'laugh') : 'cheer') : 'idle'); }
    if (ev.data.go && ev.t - ev.data.go > 13) {
      for (const c of list) { applyGains(c, { social: 0.5, fun: 0.15 }); remember(w, c, `Réunion du groupe « ${ev.data.group} »`, 1); }
      pairBond(w, list, 4);
      return true;
    }
    return false;
  }

  // ----- Visite de voisinage -----
  start_visitGroup(ev, cube, free) {
    const w = this.world;
    const nbs = w.neighbors(cube).filter(([, n]) => n.awake && n.chars.size > 0 && n.chars.size < 28);
    if (!nbs.length) return false;
    const [, dest] = pick(nbs);
    const list = free.slice(0, 3);
    if (list.length < 2) return false;
    ev.data = { dest: dest.id };
    list.forEach((c, i) => { enlist(w, ev, c, `rend visite aux voisins de « ${dest.name} »`); sendTo(w, c, dest.id, -0.3 + i * 0.3, 0.2); });
    w.addLog('événement', '🏡', `${list.map((c) => c.name).join(', ')} rendent visite aux voisins de « ${dest.name} »`, cube.id, list.map((c) => c.id));
    return true;
  }
  tick_visitGroup(ev, dt) {
    const w = this.world; const list = parts(w, ev); if (!list.length) return true;
    moveParts(w, ev, list, dt);
    const dest = w.cubes.get(ev.data.dest); if (!dest) return true;
    if (!ev.data.go && (list.every((c) => c.act.phase === 'there') || ev.t > 40)) {
      ev.data.go = ev.t;
      // les habitants viennent saluer
      for (const id of dest.chars) { const h = w.chars.get(id); if (h && available(h) && !list.includes(h)) { bubble(h, '👋', 2.5); for (const v of list) relate(w, v, h, 4, 5, true); } }
    }
    for (const c of list) if (c.act.phase === 'there') setAnim(c, ev.data.go && ev.t - ev.data.go < 3 ? 'coucou' : 'talk');
    if (ev.data.go && ev.t - ev.data.go > 8) { for (const c of list) { applyGains(c, { social: 0.4, curiosity: 0.3 }); remember(w, c, `A rendu visite aux voisins de « ${dest.name} »`, 1); } return true; }
    return false;
  }

  tick(ev, dt) { const f = this['tick_' + ev.type]; return f ? f.call(this, ev, dt) : true; }

  // ----- petits événements ponctuels -----
  accident(cube, free) {
    const w = this.world; const ch = pick(free);
    abortAct(w, ch);
    const kinds = [['trébuche et tombe', '🤕'], ['glisse sur une peau de banane', '🍌'], ['se cogne contre le mur', '💫']];
    const [txt, icon] = pick(kinds);
    ch.act = { kind: 'solo', id: 'accident', label: txt, anim: 'fall', dur: 1.4, t: 0, phase: 'do', lock: true, next: { kind: 'solo', id: 'getup', label: 'se relève', anim: 'getup', dur: 1.1, t: 0, phase: 'do' } };
    bubble(ch, icon, 2.5); feel(ch, 'surprise', 0.7); feel(ch, 'sadness', 0.1);
    w.addLog('accident', icon, `${ch.name} ${txt}`, cube.id, [ch.id], false);
    remember(w, ch, `Petit accident : ${txt}`, -0.3);
    // un voisin gentil vient aider, ou rit
    const helper = free.find((c) => c !== ch && c.traits.kindness > 0.55);
    const laugher = free.find((c) => c !== ch && c !== helper && c.traits.playfulness > 0.6);
    if (helper) {
      abortAct(w, helper);
      helper.act = { kind: 'solo', id: 'help', label: `aide ${ch.name} à se relever`, anim: 'handshake', path: [{ cube: cube.id, x: clamp(ch.x + 0.28, -1.7, 1.7), z: ch.z, kind: 'walk' }], wi: 0, phase: 'go', dur: 2, t: 0, faceChar: ch.id,
        onDone: (ww, c) => { relate(ww, ch, c, 8, 4); bubble(ch, '🙏'); remember(ww, ch, `${c.name} l'a aidé après une chute`, 1, c.id); } };
    }
    if (laugher) bubble(laugher, '😂', 2);
  }
  discovery(cube, free) {
    const w = this.world; const ch = pick(free);
    const type = pick(['cristal', 'fleur', 'livre', 'potion', 'cadeau', 'ballon']);
    addItem(w, ch, type);
    feel(ch, 'surprise', 0.8); feel(ch, 'joy', 0.3); bubble(ch, ITEMS[type].icon, 3);
    abortAct(w, ch);
    ch.act = { kind: 'solo', id: 'discover', label: `a trouvé : ${ITEMS[type].label.toLowerCase()} !`, anim: 'jump', dur: 2.4, t: 0, phase: 'do' };
    remember(w, ch, `A découvert ${ITEMS[type].label.toLowerCase()} par terre`, 1);
    w.addLog('découverte', '🔎', `${ch.name} découvre ${ITEMS[type].icon} dans « ${cube.name} »`, cube.id, [ch.id], false);
  }
}
