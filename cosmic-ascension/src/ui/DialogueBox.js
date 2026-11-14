import { bus } from '../core/EventBus.js';
import { h } from './dom.js';
import { buzz } from './haptics.js';
// Boîte de dialogue (B): portrait, nom, texte en machine à écrire, choix. Toucher = accélérer / continuer.
export function mountDialogueBox(root, { onOpen } = {}) {
  const port = h('div.dl-port'), name = h('div.dl-name'), text = h('div.dl-text'), choices = h('div.dl-choices'), more = h('div.dl-more', { text: '▼' });
  const box = h('div.dlg.hidden', {}, h('div.dl-card', {}, port, h('div.dl-body', {}, name, text, choices, more)));
  root.appendChild(box);
  let full = '', shown = 0, timer = null, hasChoices = false;
  const done = () => shown >= full.length;
  function type() { clearInterval(timer); timer = setInterval(() => { shown = Math.min(full.length, shown + 2); text.textContent = full.slice(0, shown); if (done()) { clearInterval(timer); reveal(); } }, 22); }
  function reveal() { choices.classList.toggle('hidden', !hasChoices); more.classList.toggle('hidden', hasChoices); }
  box.addEventListener('pointerdown', e => {
    if (e.target.closest('.dl-choice')) return; e.preventDefault(); buzz(6);
    if (!done()) { shown = full.length; text.textContent = full; clearInterval(timer); reveal(); }
    else if (!hasChoices) bus.emit('dialogue:choose', { i: 0 });
  });
  bus.on('dialogue:start', ({ radio }) => { box.classList.remove('hidden'); box.classList.toggle('radio', !!radio); onOpen?.(true); });
  bus.on('dialogue:end', () => { box.classList.add('hidden'); clearInterval(timer); onOpen?.(false); });
  bus.on('dialogue:node', n => {
    port.textContent = (n.speaker ?? '?').replace(/^(Le|La|L’|Dr|Chevalier|Capitaine|Aïeule)\s*/i, '').split(/[\s,’-]+/).filter(Boolean).slice(0, 2).map(w => w[0].toUpperCase()).join(''); name.textContent = n.speaker ?? ''; full = n.text; shown = 0; text.textContent = '';
    hasChoices = n.choices.length > 0; choices.innerHTML = ''; choices.classList.add('hidden'); more.classList.add('hidden');
    for (const c of n.choices) choices.appendChild(h('button.dl-choice', { text: c.label, onClick: () => { buzz(10); bus.emit('ui:click'); bus.emit('dialogue:choose', { i: c.i }); } }));
    type();
  });
  return { box };
}
