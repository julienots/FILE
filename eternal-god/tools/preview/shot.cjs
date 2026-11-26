// Rend tools/preview/index.html dans Chromium et enregistre une capture.
const { chromium } = require('playwright');
(async () => {
  const [,, url, out, w = '1920', h = '1080', wait = '4000'] = process.argv;
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const p = await b.newPage({ viewport: { width: +w, height: +h }, deviceScaleFactor: 1 });
  p.on('console', m => console.log('[console]', m.type(), m.text()));
  p.on('pageerror', e => console.log('[pageerror]', e.message));
  await p.goto(url);
  await p.waitForFunction('window.__ready === true', null, { timeout: 240000 });
  await p.waitForTimeout(+wait);
  await p.screenshot({ path: out, type: out.endsWith('.jpg') ? 'jpeg' : 'png', quality: out.endsWith('.jpg') ? 92 : undefined });
  await b.close();
})();
