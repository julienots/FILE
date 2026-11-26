// Vérifie un héros importé en jeu : repos, course, coup, vol (captures rapprochées).
const { chromium } = require('playwright');
(async () => {
  const hero = process.argv[2] || 'cosmic', out = process.argv[3] || '/tmp/claude-0/hero';
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await (await b.newContext({ viewport: { width: 915, height: 412 }, hasTouch: true, isMobile: true })).newPage();
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message)); p.on('console', (m) => { if (m.type() === 'error' && !/GPU stall|GL Driver/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript((h) => { localStorage.setItem('eg.settings', JSON.stringify({ quality: 'medium', hero: h })); localStorage.removeItem('eg.save.v1'); }, hero);
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForSelector('#title:not(.hidden)', { timeout: 1500000 });
  await p.mouse.click(450, 200); await p.waitForTimeout(1500);
  // posé au sol, caméra proche de trois quarts
  await p.evaluate(() => { const g = G.god; let sx = 0, sz = 0; for (let i = 0; i < 400; i++) { const x = -200 + Math.random() * 400, z = -100 + Math.random() * 300; if (G.villages.isFree(x, z, 30) && G.terrain.normalAt(x, z).y > 0.97 && G.terrain.heightAt(x, z) > 9 && !G.veg.trees.some(t => Math.hypot(t.x - x, t.z - z) < 30)) { sx = x; sz = z; break; } } g.pos.set(sx, G.terrain.groundAt(sx, sz), sz); g.state = 'ground'; g.vel.set(0, 0, 0); G.cam.zoomMul = 0.45; G.cam.yaw = 0.6; G.cam.pitch = 0.12; G.cam.lastInput = G.time + 1e6; G.env.dayTime = 0.4; });
  const shot = async (n, w = 2500) => { await p.waitForTimeout(w); await p.screenshot({ path: `${out}-${n}.jpg`, quality: 85, timeout: 600000 }); };
  await shot('1-repos');
  await p.evaluate(() => { G.ui.input.moveY = 1; G.ui.input.moveX = 0; }); await shot('2-course', 2500);
  await p.evaluate(() => { G.ui.input.moveY = 0; G.powers.punch(0); }); await shot('3-poing', 300);
  await p.evaluate(() => { G.god.startFly(); G.ui.input.moveY = 0.5; }); await shot('4-vol', 3000);
  console.log('erreurs', errs.length, JSON.stringify(errs.slice(0, 5)), 'calls', await p.evaluate(() => G.renderer.info.render.calls));
  await b.close();
})();
