// Captures rapprochées d'une pose du héros sous plusieurs angles (vol stationnaire par défaut).
const { chromium } = require('playwright');
(async () => {
  const out = process.argv[2] || '/tmp/claude-0/pose', mode = process.argv[3] || 'hover';
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await (await b.newContext({ viewport: { width: 915, height: 412 }, hasTouch: true, isMobile: true })).newPage();
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message)); p.on('console', (m) => { if (m.type() === 'error' && !/GPU stall|GL Driver/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript(() => { localStorage.setItem('eg.settings', JSON.stringify({ quality: 'medium', hero: 'void', music: 0 })); localStorage.removeItem('eg.save.v1'); });
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForSelector('#title:not(.hidden)', { timeout: 1500000 });
  await p.mouse.click(450, 200); await p.waitForTimeout(1000);
  await p.evaluate((mode) => { G.monster.respawnT = 1e9; if (G.dragon) G.dragon.respawnT = 1e9; const g = G.god; const x = 120, z = 140; g.pos.set(x, G.terrain.groundAt(x, z) + 8, z); g.state = 'fly'; g.vel.set(0, 0, 0); g.heading = 0; G.cam.zoomMul = 0.32; G.cam.pitch = 0.05; G.cam.lastInput = G.time + 1e6; G.env.dayTime = 0.42; if (mode === 'aura') { G.aura.transform(); } if (mode === 'cruise') G.ui.input.moveY = 0.6; }, mode);
  const views = [['face', Math.PI], ['profil', Math.PI / 2], ['3q', Math.PI * 0.75]];
  await p.waitForTimeout(mode === 'aura' ? 9000 : 4000);
  for (const [n, yaw] of views) {
    await p.evaluate((yaw) => { G.cam.yaw = G.god.heading + yaw; if (G.ui.input.moveY) {} }, yaw);
    await p.waitForTimeout(1500);
    await p.screenshot({ path: `${out}-${mode}-${n}.jpg`, quality: 85, timeout: 600000 });
  }
  console.log('erreurs', errs.length, JSON.stringify(errs.slice(0, 4)));
  await b.close();
})();
