// Usage : node bake.cjs <modèle> <dossier sortie> [tile] [élévations]
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const [,, m, out, tile = '256', el = '0,25,50,80', extra = ''] = process.argv;
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const p = await b.newPage({ viewport: { width: 800, height: 600 } });
  p.on('pageerror', e => console.log('[pageerror]', e.message));
  p.on('console', msg => { if (msg.type() === 'error') console.log('[err]', msg.text()); });
  await p.goto(`http://127.0.0.1:8765/tools/bake/impostor.html?m=${m}&tile=${tile}&el=${el}${extra}`);
  await p.waitForFunction('window.__ready === true', null, { timeout: 1800000 });
  const o = await p.evaluate(() => window.__out);
  for (const k of ['albedo', 'normal']) fs.writeFileSync(`${out}/${m}_${k}.png`, Buffer.from(o[k].split(',')[1], 'base64'));
  fs.writeFileSync(`${out}/${m}.json`, JSON.stringify(o.meta));
  console.log(m, JSON.stringify(o.meta));
  await b.close();
})();
