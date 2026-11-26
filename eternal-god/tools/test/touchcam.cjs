// Vérifie la rotation caméra au doigt (événements tactiles réels via le protocole Chrome).
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const ctx = await b.newContext({ viewport: { width: 915, height: 412 }, hasTouch: true, isMobile: true });
  const p = await ctx.newPage();
  await p.addInitScript(() => { localStorage.setItem('eg.settings', JSON.stringify({ quality: 'low' })); localStorage.removeItem('eg.save.v1'); });
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForSelector('#title:not(.hidden)', { timeout: 900000 });
  await p.mouse.click(450, 200); await p.waitForTimeout(3000);
  const cdp = await ctx.newCDPSession(p);
  const swipe = async (x0, y0, dx, dy, steps = 12, id = 1) => {
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: x0, y: y0, id }] });
    for (let i = 1; i <= steps; i++) { await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x: x0 + dx * i / steps, y: y0 + dy * i / steps, id }] }); await p.waitForTimeout(16); }
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  };
  const yaw = () => p.evaluate(() => G.cam.yaw);
  for (const [label, x, y] of [['centre-droit', 600, 200], ['entre les boutons (bas droite)', 760, 330], ['haut gauche', 200, 60]]) {
    const y0 = await yaw(); await swipe(x, y, -150, 0); await p.waitForTimeout(300);
    console.log(label, 'rotation', ((await yaw()) - y0) * 180 / Math.PI | 0, '°');
  }
  // doigt « fantôme » : un toucher annulé ne doit pas bloquer la rotation suivante
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: 600, y: 200, id: 5 }] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] });
  const y1 = await yaw(); await swipe(600, 200, -150, 0); await p.waitForTimeout(300);
  console.log('après toucher annulé', ((await yaw()) - y1) * 180 / Math.PI | 0, '°');
  await b.close();
})();
