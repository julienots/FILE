const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch(); const p = await b.newPage();
  const errs = []; p.on('pageerror', (e) => errs.push(e.message)); p.on('console', (m) => { if (m.type() === 'error') errs.push(m.text()); });
  await p.goto('http://127.0.0.1:8765/tools/test/music.html'); await p.waitForFunction(() => window.ready, null, { timeout: 30000 });
  console.log(JSON.stringify(await p.evaluate(() => window.run())), 'erreurs', errs);
  await b.close();
})();
