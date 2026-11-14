import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { CSS, CSS2, CSS_DARK } from './styles.js';
import { setLang } from './i18n.js';
import { mountControls } from './Controls.js';
import { mountHud } from './Hud.js';
import { mountScreens } from './Screens.js';
import { mountDialogueBox } from './DialogueBox.js';
import { mountCombatFx } from './CombatFx.js';
import { initNative } from './native.js';
// Point d'entrée de l'UI (B). main.js appelle mountUI(#ui) une fois.
export function mountUI(root) {
  document.head.appendChild(Object.assign(document.createElement('style'), { textContent: CSS + CSS2 + CSS_DARK }));
  setLang(State.data.settings.lang);
  const controls = mountControls(root);
  let screens;
  const hud = mountHud(root, { onPause: () => screens.pause() });
  let cfx = null;
  const playing = (v) => { hud.show(v); controls.show(v); cfx?.show(v); };
  screens = mountScreens(root, { onPlay: () => { playing(true); bus.emit('game:start'); }, onQuit: () => playing(false) });
  cfx = mountCombatFx(root, hud);
  mountDialogueBox(root, { onOpen: (open) => controls.show(!open) });
  playing(false); screens.title();
  // bouton retour Android / Échap = pause
  const back = () => { if (!hud.layer.classList.contains('hidden') || screens.open) screens.back(); };
  addEventListener('keydown', e => { if (e.code === 'Escape') back(); });
  initNative({ onBack: back });
  bus.on('app:background', () => { if (!screens.open && !hud.layer.classList.contains('hidden')) screens.pause(); });
  // mise en arrière-plan = pause automatique
  document.addEventListener('visibilitychange', () => { if (document.hidden && !screens.open && !hud.layer.classList.contains('hidden')) screens.pause(); });
  const api = { hud, controls, screens };
  globalThis.__ui = api; // debug / tests
  return api;
}
