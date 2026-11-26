// Forme humaine (marche, course, combo, vol, comète, aura), nouveaux pouvoirs et frimeurs.
const { chromium } = require('playwright');
(async () => {
  const out = process.argv[2] || '/tmp/claude-0/human';
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await (await b.newContext({ viewport: { width: 915, height: 412 }, hasTouch: true, isMobile: true })).newPage();
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message)); p.on('console', (m) => { if (m.type() === 'error' && !/GPU stall|GL Driver/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript(() => { localStorage.setItem('eg.settings', JSON.stringify({ quality: 'medium', hero: 'void', music: 0 })); localStorage.removeItem('eg.save.v1'); });
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForSelector('#title:not(.hidden)', { timeout: 1500000 });
  await p.mouse.click(450, 200); await p.waitForTimeout(1000);
  const shot = async (n, w = 2000) => { await p.waitForTimeout(w); await p.screenshot({ path: `${out}-${n}.jpg`, quality: 85, timeout: 600000 }); console.log('shot', n, await p.evaluate(() => [G.god.state, G.god.human, G.god.action && G.god.action.kind, G.god.rig ? '' : '', G.people.list.filter((x) => x.state === 'taunt').length + ' frimeurs', G.people.list.length + ' habitants', G.renderer.info.render.calls + ' appels'].join(' | '))); };
  // près de la place du village principal
  await p.evaluate(() => { G.monster.respawnT = 1e9; if (G.dragon) G.dragon.respawnT = 1e9; const v = G.villages.villages[0]; const x = v.spots.plaza.x + 3, z = v.spots.plaza.z + 3; const g = G.god; g.pos.set(x, G.terrain.groundAt(x, z), z); g.state = 'ground'; g.vel.set(0, 0, 0); g.setHuman(true); G.cam.yaw = 0.8; G.cam.pitch = 0.18; G.slowmo = 2; G.cam.lastInput = G.time + 1e6; G.env.dayTime = 0.42; });
  await shot('1-humain', 3000);
  await shot('2-frimeurs', 6000);
  await p.evaluate(() => { G.ui.input.moveY = 0.5; }); await shot('3-marche', 2000);
  await p.evaluate(() => { G.ui.input.moveY = 1; }); await shot('4-course', 2000);
  await p.evaluate(() => { G.ui.input.moveY = 0; G.powers.punch(0); }); await shot('5-poing', 250);
  await p.evaluate(() => { G.god.startFly(); G.ui.input.moveY = 0; }); await shot('6-vol', 3500);
  await p.evaluate(() => { G.ui.input.moveY = 1; }); await shot('7-comete', 3000);
  await p.evaluate(() => { G.ui.input.moveY = 0; G.aura.transform(); G.aura.transforming = 0.2; }); await shot('8-aura-humain', 3000);
  await p.evaluate(() => { G.powers.cast('astral', G.cam.aim()); }); await shot('9-deferlante', 5000);
  await p.evaluate(() => { G.cam.pitch = 0.25; G.powers.cast('moon', G.cam.aim()); }); await shot('10-lune', 4000);
  await shot('11-lune-impact', 5000);
  await p.evaluate(() => { G.powers.cast('quake', G.cam.aim()); G.powers.cast('lance', G.cam.aim()); }); await shot('12-seisme-lance', 3000);
  await p.evaluate(() => { G.god.setHuman(false); }); await shot('13-divin', 1500);
  console.log('erreurs', errs.length, JSON.stringify(errs.slice(0, 6)), 'calls', await p.evaluate(() => G.renderer.info.render.calls));
  await b.close();
})();
