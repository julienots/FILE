import { Game } from './game/game.js';
import { loadProfile } from './core/save.js';
const profile = loadProfile();
const game = new Game(document.getElementById('gl'), profile, { toast: () => {}, camName: (m) => m });
window.__SL = { game, profile };
(async () => {
  const q = new URLSearchParams(location.search);
  const x = +(q.get('x') ?? -75), z = +(q.get('z') ?? 60), yaw = +(q.get('yaw') ?? Math.PI / 2);
  if (q.get('car')) { profile.car = q.get('car'); profile.cars[profile.car] = profile.cars[profile.car] || { cfg: {}, upg: {} }; }
  if (q.get('hour')) game.sky.hour = +q.get('hour');
  if (q.get('weather')) game.sky.setWeather(q.get('weather'));
  if (q.get('q')) { profile.settings.quality = q.get('q'); game.applySettings(); }
  game.spawnPlayer(profile.car, x, z, yaw);
  await game.preload(x, z);
  game.paused = false; game.input.setEnabled(true);
  game.start();
  window.__SL.ready = true;
})();
