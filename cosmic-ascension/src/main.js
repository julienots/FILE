import { Game } from './core/Game.js';
import { bus } from './core/EventBus.js';
import { State } from './core/State.js';
import { Config } from './core/Config.js';
import { Assets } from './assets/AssetLibrary.js';
import { SpaceScene } from './scenes/SpaceScene.js';
import { PlanetScene } from './scenes/PlanetScene.js';
import { initFallback } from './core/Fallback.js';
import { initSystems } from './systems/index.js';
import { mountUI } from './ui/index.js';
import { initAudio } from './audio/Audio.js';

// Point d'entrée: State → systèmes (B) → assets → Game + scènes (A) → UI (B).
// Les systèmes de B (src/systems + src/ui) remplacent les systèmes de secours de A (core/Fallback.js).
Config.systemsOwner = 'B';
async function boot() {
  State.load(); State.startAutosave();
  initSystems(); initAudio();
  await Assets.init();
  const game = new Game(document.getElementById('game'));
  game.register('space', SpaceScene); game.register('planet', PlanetScene);
  mountUI(document.getElementById('ui'));
  initFallback(document.getElementById('ui'));   // no-op: systemsOwner === 'B'
  window.__game = game; window.__bus = bus;   // debug / tests
  game.goto('space', {}); game.start();
}
boot();
