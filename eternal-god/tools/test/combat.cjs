// Combat et animations : aura, vol divin lent, vol comète, combos contre le Titan et le Dragon,
// atterrissage de super-héros, musique. Captures + erreurs.
const { chromium } = require('playwright');
(async () => {
  const out = process.argv[2] || '/tmp/claude-0/combat';
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--autoplay-policy=no-user-gesture-required'] });
  const p = await (await b.newContext({ viewport: { width: 915, height: 412 }, hasTouch: true, isMobile: true })).newPage();
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message)); p.on('console', (m) => { if (m.type() === 'error' && !/GPU stall|GL Driver/.test(m.text())) errs.push(m.text()); });
  await p.addInitScript(() => { localStorage.setItem('eg.settings', JSON.stringify({ quality: 'medium', hero: 'void' })); localStorage.removeItem('eg.save.v1'); });
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForSelector('#title:not(.hidden)', { timeout: 1500000 });
  await p.mouse.click(450, 200); await p.waitForTimeout(1500);
  const shot = async (n, w = 2500) => { await p.waitForTimeout(w); await p.screenshot({ path: `${out}-${n}.jpg`, quality: 85, timeout: 600000 }); console.log('shot', n, await p.evaluate(() => [G.god.state, G.god.action && G.god.action.kind, G.aura.active, G.music.level, G.monster.state, G.dragon && G.dragon.state].join(' '))); };
  await p.evaluate(() => { const g = G.god; g.pos.set(30, G.terrain.groundAt(30, 60), 60); g.state = 'ground'; g.vel.set(0, 0, 0); G.cam.zoomMul = 0.55; G.cam.yaw = 0.6; G.cam.pitch = 0.1; G.cam.lastInput = G.time + 1e6; G.env.dayTime = 0.42; });
  await p.evaluate(() => G.aura.toggle()); await shot('1-transformation', 900); await p.evaluate(() => { G.aura.transforming = 0.2; });
  await shot('2-aura', 2000);
  await p.evaluate(() => { G.god.startFly(); G.ui.input.moveY = 0.3; }); await shot('3-vol-divin', 3500);
  await p.evaluate(() => { G.ui.input.moveY = 1; }); await shot('4-vol-comete', 3000);
  await p.evaluate(() => { G.ui.input.moveY = 0; G.ui.input.down = true; }); await shot('5-atterrissage', 4000);
  await p.evaluate(() => { G.ui.input.down = false; G.monster.spawn(true); G.monster.t = 3.9; }); await p.waitForTimeout(1500);
  await p.evaluate(() => { const m = G.monster; G.god.pos.set(m.pos.x + 30, G.terrain.groundAt(m.pos.x + 30, m.pos.z), m.pos.z); G.god.state = 'ground'; G.cam.zoomMul = 1.2; });
  for (let i = 0; i < 4; i++) { await p.evaluate(() => G.powers.punch(0)); await p.waitForTimeout(260); }
  await shot('6-uppercut-titan', 200);
  await shot('7-titan-projete', 1200);
  await p.evaluate(() => { G.dragon.spawn(true); }); await p.waitForTimeout(6000);
  await p.evaluate(() => { const d = G.dragon; G.god.pos.set(d.pos.x - 30, d.pos.y - 4, d.pos.z); G.god.state = 'fly'; G.cam.zoomMul = 1.6; });
  await shot('8-dragon', 300);
  for (let i = 0; i < 8; i++) { await p.evaluate(() => G.powers.punch(0)); await p.waitForTimeout(260); }
  await shot('9-dragon-frappe', 500);
  await p.evaluate(() => { G.dragon.damage(1e6); }); await shot('10-dragon-chute', 3500);
  console.log('hp titan', await p.evaluate(() => G.monster.hp), 'music', await p.evaluate(() => G.music.ready + ' ' + G.music.level));
  console.log('erreurs', errs.length, JSON.stringify(errs.slice(0, 6)), 'calls', await p.evaluate(() => G.renderer.info.render.calls));
  await b.close();
})();
