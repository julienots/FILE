import { bus } from '../core/EventBus.js';
import { State } from '../core/State.js';
import { CSS } from './styles.js';
import { setLang } from './i18n.js';
import { mountControls } from './Controls.js';
import { mountHud } from './Hud.js';
import { mountScreens } from './Screens.js';
// Point d'entrée de l'UI (B). main.js appelle mountUI(#ui) une fois.
export function mountUI(root) {
  document.head.appendChild(Object.assign(document.createElement('style'), { textContent: CSS }));
  setLang(State.data.settings.lang);
  const controls = mountControls(root);
  let screens;
  const hud = mountHud(root, { onPause: () => screens.pause() });
  const playing = (v) => { hud.show(v); controls.show(v); };
  screens = mountScreens(root, { onPlay: () => { playing(true); bus.emit('game:start'); }, onQuit: () => playing(false) });
  playing(false); screens.title();
  // bouton retour Android / Échap = pause
  addEventListener('keydown', e => { if (e.code === 'Escape' && !screens.open) screens.pause(); });
  globalThis.Capacitor?.Plugins?.App?.addListener?.('backButton', () => { if (!screens.open) screens.pause(); });
  // mise en arrière-plan = pause automatique
  document.addEventListener('visibilitychange', () => { if (document.hidden && !screens.open && !hud.layer.classList.contains('hidden')) screens.pause(); });
  const api = { hud, controls, screens };
  globalThis.__ui = api; // debug / tests
  return api;
}
