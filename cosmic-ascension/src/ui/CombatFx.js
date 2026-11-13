import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { ABILITIES } from '../data/abilities.js';
import { Combat } from '../systems/Combat.js';
import { h } from './dom.js';
import { buzz } from './haptics.js';
// Retours de combat côté UI (B): flash de dégâts, combo, barre de boss, écran K.O., refus de capacité, recharge du bouton ✦.
export function mountCombatFx(root, hud) {
  const layer = h('div.layer.cfx.noev'); root.appendChild(layer);
  const vign = h('div.vign'); layer.appendChild(vign);
  const combo = h('div.combo.hidden'); layer.appendChild(combo);
  const bName = h('div.bname'), bFill = h('i'), bPh = h('div.bph'), boss = h('div.bossbar.hidden', {}, bName, h('div.bbar', {}, bFill), bPh); layer.appendChild(boss);
  const ko = h('div.ko.hidden', {}, h('h1', { text: 'K.O.' }), h('p')); layer.appendChild(ko);
  bus.on('hero:damaged', ({ amount, hp, maxHp }) => {
    vign.classList.remove('hit'); void vign.offsetWidth; vign.classList.add('hit'); buzz(amount > 20 ? 45 : 22);
    vign.classList.toggle('low', hp / maxHp < .3);
  });
  bus.on('hero:stats', h_ => vign.classList.toggle('low', h_.hp / h_.maxHp < .3 && h_.hp > 0));
  let comboT;
  bus.on('combat:combo', ({ n }) => { combo.textContent = `${n} COMBO`; combo.classList.remove('hidden', 'pop'); void combo.offsetWidth; combo.classList.add('pop'); clearTimeout(comboT); comboT = setTimeout(() => combo.classList.add('hidden'), 1100); });
  bus.on('boss:spawn', ({ name, title }) => { boss.classList.remove('hidden'); bName.innerHTML = ''; bName.append(name, h('small', { text: title })); bFill.style.transform = 'scaleX(1)'; bPh.textContent = ''; buzz(80); });
  bus.on('boss:damaged', ({ hp, maxHp }) => { bFill.style.transform = `scaleX(${hp / maxHp})`; });
  bus.on('boss:phase', ({ phase }) => { bPh.textContent = `PHASE ${phase + 1}`; boss.classList.remove('shake'); void boss.offsetWidth; boss.classList.add('shake'); buzz(60); });
  for (const ev of ['boss:defeated', 'boss:despawn']) bus.on(ev, () => boss.classList.add('hidden'));
  bus.on('boss:defeated', ({ name }) => hud.toast(`★ ${name} vaincu !`, 'questDone'));
  bus.on('hero:died', ({ lostDust }) => { ko.classList.remove('hidden'); ko.lastChild.textContent = lostDust ? `−${lostDust} ✧` : ''; buzz(120); });
  bus.on('hero:respawn', () => ko.classList.add('hidden'));
  const REASON = { energy: 'Pas assez d’énergie', cooldown: 'Capacité en recharge', locked: 'Capacité verrouillée' };
  let failT = 0;
  bus.on('ability:fail', ({ reason }) => { const n = performance.now(); if (n - failT > 900) { failT = n; hud.toast(REASON[reason] ?? reason, 'warn'); } });
  // anneau de recharge sur le bouton capacité
  const cd = h('div.cd'); const attach = () => { const b = document.querySelector('.b-special'); if (b && !b.contains(cd)) b.prepend(cd); };
  setInterval(() => { attach(); const ab = [...State.data.hero.abilities].reverse().find(a => a !== 'hop'); const p = ab ? Combat.cooldown(ab) : 0;
    const en = ab && State.data.hero.energy < (ABILITIES[ab]?.cost ?? 0); cd.style.setProperty('--p', (p * 100).toFixed(0) + '%'); cd.classList.toggle('noen', !!en); }, 100);
}
