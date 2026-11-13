import { Input } from '../core/Input.js';
import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { ABILITIES } from '../data/abilities.js';
import { h } from './dom.js';
import { t } from './i18n.js';
import { buzz } from './haptics.js';
// Contrôles tactiles paysage (B). Écrit UNIQUEMENT dans Input (contrat ARCHITECTURE §Entrées).
// Gauche: joystick flottant (apparaît sous le doigt). Droite: Attaque (gros), Saut, Boost, Capacité ✦, Action contextuelle. Glisser à droite = caméra.
export function mountControls(root) {
  const layer = h('div.layer.controls'); root.appendChild(layer);
  Input._touch = {};
  // caméra (sous les boutons)
  const cam = h('div.camzone'); layer.appendChild(cam);
  let lp = null, lx = 0, ly = 0;
  cam.addEventListener('pointerdown', e => { lp = e.pointerId; lx = e.clientX; ly = e.clientY; cam.setPointerCapture(lp); });
  cam.addEventListener('pointermove', e => { if (e.pointerId !== lp) return; Input.look.x += e.clientX - lx; Input.look.y += e.clientY - ly; lx = e.clientX; ly = e.clientY; });
  const camEnd = e => { if (e.pointerId === lp) lp = null; }; cam.addEventListener('pointerup', camEnd); cam.addEventListener('pointercancel', camEnd);

  // joystick flottant: zone gauche entière
  const zone = h('div.jzone');
  const knob = h('i'), joy = h('div.joy', {}, knob); zone.appendChild(joy); layer.appendChild(zone);
  let jid = null, cx = 0, cy = 0;
  const R = () => joy.offsetWidth / 2;
  const home = () => { joy.style.left = joy.style.top = joy.style.right = joy.style.bottom = ''; joy.style.transform = ''; };
  const upd = e => {
    let dx = (e.clientX - cx) / R(), dy = (e.clientY - cy) / R(); const l = Math.hypot(dx, dy); if (l > 1) { dx /= l; dy /= l; }
    const dz = l < .12 ? 0 : 1; Input.move.x = dx * dz; Input.move.y = -dy * dz; knob.style.transform = `translate(${dx * R() * .62}px,${dy * R() * .62}px)`;
  };
  zone.addEventListener('pointerdown', e => {
    if (jid != null) return; jid = e.pointerId; zone.setPointerCapture(jid); joy.classList.add('act');
    const zr = zone.getBoundingClientRect(), r = R();
    const x = Math.max(zr.left + r, Math.min(zr.right - r, e.clientX)), y = Math.max(zr.top + r, Math.min(zr.bottom - r, e.clientY));
    joy.style.left = (x - zr.left - r) + 'px'; joy.style.top = (y - zr.top - r) + 'px'; joy.style.right = joy.style.bottom = 'auto';
    cx = x; cy = y; upd(e);
  });
  zone.addEventListener('pointermove', e => { if (e.pointerId === jid) upd(e); });
  const jend = e => { if (e.pointerId !== jid) return; jid = null; Input.move.x = Input.move.y = 0; knob.style.transform = ''; joy.classList.remove('act'); home(); };
  zone.addEventListener('pointerup', jend); zone.addEventListener('pointercancel', jend);

  // boutons
  const pad = h('div.pad'); layer.appendChild(pad);
  const mk = (key, cls, label, ms = 10) => {
    const lab = h('span', { text: label }), b = h('div.btn.' + cls, {}, lab); pad.appendChild(b);
    const set = v => { Input.buttons[key] = v; Input._touch[key] = v; b.classList.toggle('on', v); };
    b.addEventListener('pointerdown', e => { e.preventDefault(); e.stopPropagation(); b.setPointerCapture(e.pointerId); set(true); buzz(ms); });
    b.addEventListener('pointerup', () => set(false)); b.addEventListener('pointercancel', () => set(false)); b.addEventListener('lostpointercapture', () => set(false));
    b.lab = lab; return b;
  };
  mk('attack', 'b-attack', '⚔', 18); mk('jump', 'b-jump', '⤒'); mk('boost', 'b-boost', '≫');
  const special = mk('special', 'b-special', '✦', 25), action = mk('action', 'b-action', '', 15);
  action.classList.add('off');

  // icône de capacité = capacité la plus récente du stade actuel
  const refreshSpecial = () => {
    const ab = [...(State.data.hero.abilities || [])].reverse().find(a => a !== 'hop');
    special.lab.textContent = ABILITIES[ab]?.icon ?? '✦';
  };
  refreshSpecial(); bus.on('hero:evolved', refreshSpecial); bus.on('state:loaded', refreshSpecial);
  // bouton contextuel (Atterrir / Décoller / Parler…). B5/B3 émettront `ui:context {label|null}` pour PNJ/objets.
  const setAction = (label) => { action.classList.toggle('off', !label); action.lab.textContent = label || ''; };
  bus.on('space:near', ({ canLand }) => setAction(canLand ? t('act.land') : null));
  bus.on('scene:entered', ({ id }) => setAction(id === 'planet' ? t('act.takeoff') : null));
  bus.on('ui:context', ({ label } = {}) => setAction(label));

  // gaucher
  const applyHand = () => root.classList.toggle('lefty', !!State.data.settings.leftHanded);
  applyHand(); bus.on('settings:changed', applyHand);
  // relâche tout quand l'UI recouvre (pause, dialogue)
  const releaseAll = () => { for (const k in Input.buttons) Input.buttons[k] = Input._touch[k] = false; Input.move.x = Input.move.y = 0; pad.querySelectorAll('.on').forEach(b => b.classList.remove('on')); };
  bus.on('game:pause', releaseAll); bus.on('dialogue:start', releaseAll);
  return { layer, show(v) { layer.classList.toggle('hidden', !v); if (!v) releaseAll(); } };
}
