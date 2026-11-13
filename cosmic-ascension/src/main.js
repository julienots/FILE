import { Game } from './core/Game.js';
import { bus } from './core/EventBus.js';
import { State } from './core/State.js';
import { Assets } from './assets/AssetLibrary.js';
import { SpaceScene } from './scenes/SpaceScene.js';
import { PlanetScene } from './scenes/PlanetScene.js';
import { mountControls } from './ui/Controls.js';
import { initFallback } from './core/Fallback.js';

// Point d'entrée. OPUS: ajouter écran titre, HUD, menus, audio, systèmes (quêtes, dialogues…) via bus d'événements.
async function boot() {
  State.load();
  await Assets.init();
  const game = new Game(document.getElementById('game'));
  game.register('space', SpaceScene); game.register('planet', PlanetScene);
  mountControls(document.getElementById('ui'));
  initFallback(document.getElementById('ui'));   // systèmes de secours tant que B n'a pas livré (voir core/Fallback.js)
  window.__game = game; window.__bus = bus;               // debug / tests
  game.goto('space', {}); game.start();
}
boot();
