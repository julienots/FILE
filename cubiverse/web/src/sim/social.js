// Relations, mémoire, émotions, groupes.
import { clamp, TRAITS } from './constants.js';
import { groupName } from './names.js';

export function getRel(a, bId) {
  let r = a.rel[bId];
  if (!r) { r = a.rel[bId] = { a: 0, f: 0, n: 0, last: 0 }; }
  return r;
}

export function relLabel(r, isBest) {
  if (!r) return 'Inconnu';
  if (isBest) return 'Meilleur ami';
  if (r.a >= 35) return 'Ami';
  if (r.a <= -35) return 'Rival';
  if (r.f >= 4) return 'Connaissance';
  return 'Inconnu';
}

export function bestFriendId(ch) {
  let best = null, bv = 69;
  for (const id in ch.rel) { const r = ch.rel[id]; if (r.a > bv && r.f > 25) { bv = r.a; best = id; } }
  return best;
}

export function compat(a, b) {
  let d = 0; const ks = ['sociability', 'playfulness', 'curiosity', 'kindness'];
  for (const k of ks) d += Math.abs(a.traits[k] - b.traits[k]);
  return 1 - d / ks.length; // 0..1
}

export function remember(world, ch, text, v = 0, withId = null) {
  ch.memory.push({ t: world.time, text, v, w: withId });
  if (ch.memory.length > 40) ch.memory.shift();
}

export function feel(ch, emo, amount) {
  ch.emo[emo] = clamp((ch.emo[emo] || 0) + amount, 0, 1);
  if (emo === 'joy' && amount > 0) { ch.emo.sadness = clamp(ch.emo.sadness - amount * 0.5, 0, 1); ch.emo.anger = clamp(ch.emo.anger - amount * 0.4, 0, 1); }
}

export function mood(ch) {
  const n = ch.needs; const e = ch.emo;
  const needAvg = (n.energy + n.hunger + n.fun + n.social + n.comfort + n.curiosity) / 6;
  return clamp(needAvg * 0.7 + e.joy * 0.45 - e.sadness * 0.4 - e.anger * 0.35 - e.fear * 0.2, 0, 1);
}

export function dominantEmotion(ch) {
  let best = 'neutral', bv = 0.28;
  for (const k in ch.emo) if (ch.emo[k] > bv) { bv = ch.emo[k]; best = k; }
  return best;
}

// Modifie la relation dans les deux sens et détecte les changements de statut.
export function relate(world, a, b, delta, fam = 3, quiet = false) {
  const ra = getRel(a, b.id), rb = getRel(b, a.id);
  const c = 0.6 + compat(a, b) * 0.8;
  const before = ra.a;
  const d = delta > 0 ? delta * c : delta * (1.4 - c * 0.5);
  ra.a = clamp(ra.a + d, -100, 100); rb.a = clamp(rb.a + d * (0.8 + b.traits.kindness * 0.3), -100, 100);
  ra.f = clamp(ra.f + fam, 0, 100); rb.f = clamp(rb.f + fam, 0, 100);
  ra.n++; rb.n++; ra.last = rb.last = world.time;
  if (quiet) return;
  if (before < 35 && ra.a >= 35 && rb.a >= 30) {
    a.stats.friendsMade++; b.stats.friendsMade++;
    world.addLog('social', '🤝', `${a.name} et ${b.name} sont devenus amis`, a.cubeId, [a.id, b.id]);
    remember(world, a, `Est devenu ami avec ${b.name}`, 1, b.id); remember(world, b, `Est devenu ami avec ${a.name}`, 1, a.id);
    a.bubble = b.bubble = { icon: '❤️', t: 2.5 };
  } else if (before > -35 && ra.a <= -35) {
    world.addLog('social', '⚡', `${a.name} et ${b.name} sont maintenant rivaux`, a.cubeId, [a.id, b.id]);
    remember(world, a, `Rivalité avec ${b.name}`, -1, b.id); remember(world, b, `Rivalité avec ${a.name}`, -1, a.id);
  } else if (before < 70 && ra.a >= 70 && ra.f > 25 && bestFriendId(a) === b.id) {
    world.addLog('social', '💛', `${b.name} est désormais le meilleur ami de ${a.name}`, a.cubeId, [a.id, b.id]);
    remember(world, a, `${b.name} est devenu son meilleur ami`, 1, b.id);
  }
}

// Recalcule les groupes d'amis (composantes connexes des amitiés réciproques).
export function updateGroups(world) {
  const chars = world.chars;
  const seen = new Set(); const comps = [];
  for (const ch of chars.values()) {
    if (seen.has(ch.id)) continue;
    const comp = []; const q = [ch]; seen.add(ch.id);
    while (q.length) {
      const c = q.pop(); comp.push(c.id);
      for (const id in c.rel) {
        if (seen.has(id)) continue;
        const o = chars.get(id); if (!o) continue;
        if (c.rel[id].a >= 35 && o.rel[c.id] && o.rel[c.id].a >= 35) { seen.add(id); q.push(o); }
      }
      if (comp.length > 40) break;
    }
    if (comp.length >= 3) comps.push(comp);
  }
  const old = world.groups; const next = [];
  for (const members of comps) {
    let match = null, bestOverlap = 0;
    for (const g of old) {
      const ov = g.members.filter((m) => members.includes(m)).length;
      if (ov > bestOverlap) { bestOverlap = ov; match = g; }
    }
    if (match && bestOverlap >= 2 && !next.includes(match)) { match.members = members; next.push(match); }
    else {
      const g = { id: 'g' + Math.random().toString(36).slice(2, 8), name: groupName(), members, created: world.time };
      next.push(g);
      const names = members.slice(0, 4).map((m) => chars.get(m).name).join(', ');
      world.addLog('social', '👥', `Nouveau groupe « ${g.name} » : ${names}${members.length > 4 ? '…' : ''}`, chars.get(members[0]).cubeId, members.slice(0, 6));
    }
  }
  world.groups = next;
}

export function groupOf(world, chId) { return world.groups.find((g) => g.members.includes(chId)) || null; }

export function personalitySummary(ch) {
  const t = ch.traits; const out = [];
  if (t.sociability > 0.68) out.push('sociable'); else if (t.sociability < 0.32) out.push('timide');
  if (t.playfulness > 0.68) out.push('joueur');
  if (t.curiosity > 0.68) out.push('curieux');
  if (t.kindness > 0.68) out.push('gentil');
  if (t.temper > 0.68) out.push('soupe au lait'); else if (t.temper < 0.25) out.push('calme');
  if (t.diligence > 0.68) out.push('travailleur'); else if (t.diligence < 0.25) out.push('flemmard');
  if (t.energy > 0.7) out.push('énergique'); else if (t.energy < 0.3) out.push('paisible');
  return out.length ? out.join(', ') : 'équilibré';
}
export { TRAITS };
