import { Game } from './core/Game.js';
import { State } from './core/State.js';
import { Assets } from './assets/AssetLibrary.js';
import { SpaceScene } from './scenes/SpaceScene.js';
import { PlanetScene } from './scenes/PlanetScene.js';
import { initSystems } from './systems/index.js';
import { mountUI } from './ui/index.js';

// Point d'entrée: State → systèmes (B) → assets → Game + scènes (A) → UI (B).
async function boot() {
  State.load(); State.startAutosave();
  initSystems();
  await Assets.init();
  const game = new Game(document.getElementById('game'));
  game.register('space', SpaceScene); game.register('planet', PlanetScene);
  mountUI(document.getElementById('ui'));
  window.__game = game;               // debug / tests
  game.goto('space', {}); game.start();
}
boot();
