// Test de fumée: build servi par vite preview, charge le jeu en paysage, vérifie 0 erreur console, capture d'écran des scènes.
import { createRequire } from 'module'; import { spawn } from 'child_process';
const { chromium } = createRequire(import.meta.url)('playwright');
const srv = spawn('npx', ['vite', 'preview', '--port', '4173', '--strictPort'], { stdio: 'ignore' });
await new Promise(r => setTimeout(r, 2500));
const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const p = await b.newPage({ viewport: { width: 1280, height: 600 } }); const errs = [];
p.on('pageerror', e => errs.push(String(e))); p.on('console', m => m.type() === 'error' && errs.push(m.text()));
await p.goto('http://localhost:4173/'); await p.waitForTimeout(4000);
await p.screenshot({ path: 'tests/out/space.png' });
await p.keyboard.down('ShiftLeft'); await p.waitForTimeout(1500); await p.screenshot({ path: 'tests/out/boost.png' });
await p.waitForTimeout(2500); await p.keyboard.up('ShiftLeft'); await p.keyboard.press('KeyE'); await p.waitForTimeout(1900); await p.screenshot({ path: 'tests/out/arrive.png' });
console.log('scene:', await p.evaluate(() => window.__game.currentName)); await p.waitForTimeout(2500);
await p.keyboard.press('KeyE'); await p.waitForTimeout(2200); await p.screenshot({ path: 'tests/out/takeoff.png' }); console.log('scene:', await p.evaluate(() => window.__game.currentName));
await p.evaluate(() => window.__game.goto('planet', { id: 'home' })); await p.waitForTimeout(3000);
await p.screenshot({ path: 'tests/out/planet.png' });
for (const id of ['ember', 'glacio', 'sylva']) { await p.evaluate((id) => window.__game.goto('planet', { id }), id); await p.waitForTimeout(1500); await p.screenshot({ path: `tests/out/planet-${id}.png` }); }
await p.keyboard.press('KeyK'); await p.waitForTimeout(1900); await p.screenshot({ path: 'tests/out/evolve.png' });
for (let i = 0; i < 3; i++) { await p.waitForTimeout(3600); await p.keyboard.press('KeyK'); } await p.waitForTimeout(3400); await p.screenshot({ path: 'tests/out/evolved4.png' });
console.log('fps~', await p.evaluate(() => new Promise(r => { let n = 0; const t0 = performance.now(); const f = () => { n++; performance.now() - t0 > 1500 ? r(Math.round(n / 1.5)) : requestAnimationFrame(f); }; f(); })));
await b.close(); srv.kill();
if (errs.length) { console.error('ERREURS:', errs); process.exit(1); } console.log('SMOKE OK');
