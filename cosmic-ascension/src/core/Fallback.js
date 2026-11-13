import { bus } from './EventBus.js';
import { State } from './State.js';
import { Config } from './Config.js';
import { EVOLUTIONS } from '../data/evolutions.js';
// SYSTÈMES DE SECOURS (Claude A) — progression/PV/mini-HUD minimaux pour que le jeu soit jouable tant que B n'a pas livré
// systems/ + ui/. B: mets `Config.systemsOwner = 'B'` avant le boot (ou supprime l'appel dans main.js) et ce module ne fait plus rien.
export function initFallback(root) {
  if (Config.systemsOwner === 'B') return;
  const h = () => State.data.hero, maxHp = () => 100 * (1 + h().stage);
  const el = document.createElement('div');
  el.style.cssText = 'position:absolute;left:3vmin;top:3vmin;width:44vmin;color:#fff;font:600 3vmin system-ui;text-shadow:0 1px 4px #000;pointer-events:none;transition:opacity .3s';
  el.innerHTML = `<div id="fb-name"></div>
    <div style="height:2.4vmin;border-radius:2vmin;background:#0008;border:1px solid #fff6;margin:.8vmin 0;overflow:hidden"><div id="fb-hp" style="height:100%;background:linear-gradient(90deg,#ff5a8a,#ffb0a0);transition:width .2s"></div></div>
    <div style="height:1.6vmin;border-radius:2vmin;background:#0008;border:1px solid #fff6;overflow:hidden"><div id="fb-xp" style="height:100%;background:linear-gradient(90deg,#6ff2ff,#b48aff);transition:width .3s"></div></div>
    <div id="fb-dust" style="margin-top:.8vmin;font-size:2.6vmin"></div>`;
  root.appendChild(el); const $ = (id) => el.querySelector('#' + id);
  const toast = document.createElement('div'); toast.style.cssText = 'position:absolute;left:50%;top:22%;transform:translateX(-50%);color:#fff;font:800 4.2vmin system-ui;max-width:90vw;white-space:normal;text-shadow:0 0 18px #b48aff,0 2px 6px #000;opacity:0;transition:opacity .4s;pointer-events:none;text-align:center'; root.appendChild(toast);
  let toastT; const say = (text, ms = 2600) => { toast.textContent = text; toast.style.opacity = 1; clearTimeout(toastT); toastT = setTimeout(() => toast.style.opacity = 0, ms); };
  const draw = () => { const s = h(), cur = EVOLUTIONS[s.stage], nx = EVOLUTIONS[s.stage + 1];
    $('fb-name').textContent = cur.name; $('fb-hp').style.width = Math.max(0, s.hp / maxHp() * 100) + '%';
    $('fb-xp').style.width = (nx ? Math.min(1, (s.xp - cur.xp) / (nx.xp - cur.xp)) * 100 : 100) + '%'; $('fb-dust').textContent = '✦ ' + State.data.inventory.cosmicDust; };
  let dead = false, pending = false;
  const checkEvolve = () => { const s = h(), nx = EVOLUTIONS[s.stage + 1]; if (!nx || s.xp < nx.xp || pending || dead) return; pending = true;
    s.stage++; s.maxHp = maxHp(); s.hp = s.maxHp; bus.emit('hero:evolved', { stage: s.stage }); say('ÉVOLUTION — ' + nx.name, 3400); draw(); };
  const xp = (n) => { h().xp += n; bus.emit('hero:xp', { amount: n, total: h().xp }); draw(); checkEvolve(); };
  const hurt = (n, source) => { const s = h(); if (dead || n <= 0) return; s.hp -= n; bus.emit('hero:damaged', { amount: n, source }); draw();
    if (s.hp <= 0) { dead = true; bus.emit('hero:died', { source }); say('Tu renais…', 1800); setTimeout(() => { s.hp = maxHp(); dead = false; draw(); bus.emit('hero:respawn', {}); }, 1700); } };
  bus.on('pickup:collected', ({ amount = 1 }) => { State.data.inventory.cosmicDust += amount; xp(6 * amount); });
  bus.on('enemy:killed', ({ xp: n = 10 }) => xp(n));
  bus.on('boss:defeated', ({ xp: n = 1500, id }) => { say('BOSS VAINCU !', 4000); xp(n); });
  bus.on('boss:spawn', ({ id }) => say(id === 'devourer' ? '⚠ DÉVOREUR DE PLANÈTES ⚠' : '⚠ SERPENT CÉLESTE ⚠', 3500));
  const NAMES = { house: 'Maison', greenhouse: 'Serre', lab: 'Laboratoire', forge: 'Forge d’étoiles', hangar: 'Hangar', observatory: 'Observatoire', temple: 'Temple', tower: 'Tour d’énergie' };
  bus.on('build:request', ({ planetId, slot, buildingId, cost }) => { const inv = State.data.inventory;
    if (inv.cosmicDust < cost) return say(`Il faut ${cost} ✦ de poussière (tu en as ${inv.cosmicDust})`, 2600);
    inv.cosmicDust -= cost; State.data.homePlanet.buildings.push({ slot, id: buildingId }); State.data.homePlanet.level = 1 + Math.floor(State.data.homePlanet.buildings.length / 2);
    bus.emit('build:placed', { planetId, slot, buildingId }); say(`${NAMES[buildingId] ?? buildingId} construit !`); xp(60); State.save(); });
  bus.on('secret:found', ({ xp: n = 100 }) => { say('✧ Secret découvert ✧'); xp(n); });
  bus.on('npc:interact', ({ name, line }) => say(`${name} : « ${line} »`, 4200));
  bus.on('enemy:touch', ({ dmg }) => hurt(dmg, 'enemy'));
  bus.on('boss:hit-hero', ({ dmg }) => hurt(dmg, 'boss'));
  bus.on('ship:bump', ({ speed = 0 }) => hurt(Math.round(2 + speed * .03), 'bump'));
  bus.on('cinematic:start', ({ id }) => { if (id === 'evolution') el.style.opacity = 0; });
  bus.on('cinematic:end', ({ id }) => { el.style.opacity = 1; if (id === 'evolution') { pending = false; checkEvolve(); } });
  bus.on('scene:entered', () => { pending = false; el.style.opacity = 1; setTimeout(checkEvolve, 1500); });
  setInterval(() => { const s = h(); if (!dead && s.hp < maxHp()) { s.hp = Math.min(maxHp(), s.hp + maxHp() * .02); draw(); } }, 1000);   // régénération lente
  if (!(h().hp > 0)) h().hp = maxHp(); draw();
}
