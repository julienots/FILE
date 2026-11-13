// Captures UI (B): titre, HUD espace, HUD planète + évolution, pause, réglages, mode gaucher. `npm run ui` (après build).
import { createRequire } from 'module'; import { spawn } from 'child_process';
const { chromium } = createRequire(import.meta.url)('playwright');
const srv = spawn('npx', ['vite', 'preview', '--port', '4174', '--strictPort'], { stdio: 'ignore' });
await new Promise(r => setTimeout(r, 2500));
const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const errs = [], shots = [];
for (const vp of [{ name: 'phone', width: 915, height: 412, deviceScaleFactor: 1 }, { name: 'tablet', width: 1180, height: 820, deviceScaleFactor: 1 }]) {
  const p = await b.newPage({ viewport: vp, hasTouch: true });
  p.on('pageerror', e => errs.push(String(e))); p.on('console', m => m.type() === 'error' && errs.push(m.text()));
  const shot = async n => { const f = `tests/out/ui_${vp.name}_${n}.png`; await p.screenshot({ path: f }); shots.push(f); };
  await p.goto('http://localhost:4174/'); await p.evaluate(() => localStorage.clear()); await p.reload(); await p.waitForTimeout(3000);
  await shot('1_title');
  await p.click('.cbtn'); await p.waitForTimeout(1200); await shot('2_space');
  await p.evaluate(() => window.__game.goto('planet', { id: 'ember' })); await p.waitForTimeout(1500); await shot('3_planet');
  await p.evaluate(() => { window.__ui.hud.toast('Test de toast', 'discover'); });
  await p.keyboard.press('Escape'); await p.waitForTimeout(600); await shot('4_pause');
  await p.click('.screen:not(.hidden) .cbtn.alt'); await p.waitForTimeout(500); await shot('5_settings');
  if (vp.name === 'phone') { const V = '.screen:not(.hidden) '; await p.click(V + '.row:nth-of-type(4) .tog'); await p.click(V + '.cbtn'); await p.click(V + '.cbtn'); await p.waitForTimeout(500); await shot('6_lefty'); }
  await p.close();
}
await b.close(); srv.kill();
console.log(shots.join('\n'));
if (errs.length) { console.error('ERREURS:', errs); process.exit(1); } console.log('UI OK');
