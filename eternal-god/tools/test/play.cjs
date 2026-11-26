// Session de jeu automatisée dans Chromium (écran de téléphone) : chargement, démarrage,
// joystick, vol, pouvoirs, menu ; relève les erreurs et prend des captures.
const { chromium } = require('playwright');
const out = process.argv[2] || '/tmp/claude-0/play';
const q = process.argv[3] || 'medium';
(async () => {
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const ctx = await b.newContext({ viewport: { width: 915, height: 412 }, deviceScaleFactor: 1, hasTouch: true, isMobile: true });
  const p = await ctx.newPage();
  const errors = [];
  p.on('pageerror', (e) => { errors.push('PAGEERROR ' + e.message); console.log('[pageerror]', e.message); });
  p.on('console', (m) => { if (m.type() === 'error' && !/GPU stall|GL Driver/.test(m.text())) { errors.push(m.text()); console.log('[console.error]', m.text().slice(0, 300)); } });
  await p.addInitScript((qq) => { localStorage.setItem('eg.settings', JSON.stringify({ quality: qq, showFps: true })); localStorage.removeItem('eg.save.v1'); }, q);
  const t0 = Date.now();
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForTimeout(2500);
  await p.screenshot({ path: `${out}-0-loading.jpg`, quality: 85, timeout: 600000 });
  await p.waitForSelector('#title:not(.hidden)', { timeout: 1500000 });
  console.log('load time', ((Date.now() - t0) / 1000).toFixed(1), 's');
  await p.waitForTimeout(1500);
  await p.screenshot({ path: `${out}-1-title.jpg`, quality: 85, timeout: 600000 });
  if (await p.$('#fatal:not(.hidden)')) { console.log('FATAL', await p.textContent('#fatalMsg')); }
  await p.mouse.click(450, 200);
  await p.waitForTimeout(4000);
  await p.screenshot({ path: `${out}-2-start.jpg`, quality: 85, timeout: 600000 });
  const step = async (label, fn, wait = 3000) => { await fn(); await p.waitForTimeout(wait); await p.screenshot({ path: `${out}-${label}.jpg`, quality: 85, timeout: 600000 }); console.log('step', label, await p.evaluate(() => JSON.stringify({ st: G.god.state, pos: G.god.pos.toArray().map(v => +v.toFixed(1)), fps: document.querySelector('#fps').textContent }))); };
  // atterrir : quitter le vol
  await step('3-land', () => p.evaluate(() => G.god.toggleFly()), 5000);
  // marcher puis courir avec le joystick (clavier = même logique d'entrée)
  await step('4-run', async () => { await p.keyboard.down('KeyW'); await p.keyboard.down('ShiftLeft'); await p.waitForTimeout(3500); await p.keyboard.up('KeyW'); await p.keyboard.up('ShiftLeft'); }, 500);
  await step('5-punch', () => p.evaluate(() => G.powers.punch(0)), 1500);
  await step('6-explosion', () => p.evaluate(() => { G.powers.cooldown = 0; G.powers.cast('explosion', G.cam.aim()); }), 2500);
  await step('7-shockwave', () => p.evaluate(() => G.powers.cast('shockwave', G.cam.aim())), 2500);
  await step('8-lightning', () => p.evaluate(() => G.powers.cast('lightning', G.cam.aim())), 1500);
  await step('9-meteor', () => p.evaluate(() => G.powers.cast('meteor', G.cam.aim())), 4500);
  await step('10-laser', async () => { await p.evaluate(() => { G.ui.input.powerHeld = true; G.powers.cast('laser', G.cam.aim()); }); await p.waitForTimeout(2500); await p.evaluate(() => { G.ui.input.powerHeld = false; }); }, 800);
  await step('11-fly', async () => { await p.evaluate(() => { G.god.jump(); G.god.startFly(); G.cam.pitch = 0.1; }); await p.keyboard.down('KeyW'); await p.waitForTimeout(3000); await p.keyboard.up('KeyW'); }, 500);
  await step('12-monster', () => p.evaluate(() => { G.monster.respawnT = 0; }), 7000);
  await step('13-menu', () => p.evaluate(() => G.ui.openMenu('map')), 1500);
  await p.evaluate(() => G.ui.closeMenu());
  console.log('calls/tris', await p.evaluate(() => JSON.stringify(G.renderer.info.render)));
  console.log('ERRORS', errors.length, JSON.stringify(errors.slice(0, 10)));
  await b.close();
})();
