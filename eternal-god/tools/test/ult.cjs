// Lance chaque pouvoir ultime en jeu et capture l'effet.
const { chromium } = require('playwright');
(async () => {
  const out = process.argv[2] || '/tmp/claude-0/ult';
  const only = process.argv[3] ? process.argv[3].split(',') : null;
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await (await b.newContext({ viewport: { width: 915, height: 412 }, hasTouch: true, isMobile: true })).newPage();
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message)); p.on('console', (m) => { if (m.type() === 'error' && !/GPU stall|GL Driver/.test(m.text())) errs.push(m.text().slice(0, 200)); });
  await p.addInitScript(() => { localStorage.setItem('eg.settings', JSON.stringify({ quality: 'low', hero: 'void' })); localStorage.removeItem('eg.save.v1'); });
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForSelector('#title:not(.hidden)', { timeout: 1500000 });
  await p.mouse.click(450, 200); await p.waitForTimeout(1500);
  const ids = only || ['singularity', 'comet', 'leap', 'meteorshower', 'swords', 'tornado', 'gravity', 'timestop', 'fissure', 'supernova'];
  for (const id of ids) {
    // près du bourg, face aux maisons
    await p.evaluate(() => { const g = G.god; g.pos.set(-20, G.terrain.groundAt(-20, 150), 150); g.state = 'ground'; g.vel.set(0, 0, 0); g.heading = Math.PI; G.cam.yaw = 0.25; G.cam.pitch = 0.32; G.cam.zoomMul = 2.0; G.cam.lastInput = G.time + 1e6; for (const k in G.powers.cool) G.powers.cool[k] = 0; G.env.dayTime = 0.45; });
    await p.waitForTimeout(400);
    await p.evaluate((id) => { const aim = { point: new THREE_V(G.god.pos.x, 0, G.god.pos.z - 70) }; }, id).catch(() => {});
    await p.evaluate((id) => { const pt = G.god.pos.clone(); pt.z -= 70; pt.y = G.terrain.heightAt(pt.x, pt.z); G.powers.cast(id, { point: pt, dir: null, ground: true }); }, id);
    const wait = { singularity: 3500, comet: 500, leap: 3200, meteorshower: 3500, swords: 2800, tornado: 3500, gravity: 1800, timestop: 1500, fissure: 1200, supernova: 2700 }[id] || 2000;
    await p.waitForTimeout(wait);
    await p.screenshot({ path: `${out}-${id}.jpg`, quality: 80, timeout: 600000 });
    console.log(id, 'ok', 'effets actifs', await p.evaluate(() => JSON.stringify({ n: G.ultimates.effects.length, god: G.god.pos.toArray().map(v => +v.toFixed(1)), cam: G.camera.position.toArray().map(v => +v.toFixed(1)), exp: G.renderer.toneMappingExposure, st: G.god.state, fov: G.camera.fov })));
    await p.waitForTimeout(3000);
  }
  console.log('erreurs', errs.length, JSON.stringify(errs.slice(0, 8)));
  await b.close();
})();
