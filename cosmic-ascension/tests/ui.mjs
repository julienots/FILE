// Captures UI (B): titre, dialogue, HUD espace/planète, combat (boss, K.O.), pause, quêtes, inventaire, réglages, gaucher.
// `npm run build && npm run ui` → tests/out/ui_<appareil>_<n>.png
import { createRequire } from 'module'; import { spawn } from 'child_process';
const { chromium } = createRequire(import.meta.url)('playwright');
const srv = spawn('npx', ['vite', 'preview', '--port', '4174', '--strictPort'], { stdio: 'ignore' });
await new Promise(r => setTimeout(r, 2500));
const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const errs = [], shots = [], V = '.screen:not(.hidden) ';
const only = process.argv[2];
for (const vp of [{ name: 'phone', width: 915, height: 412 }, { name: 'tablet', width: 1180, height: 820 }]) {
  if (only && only !== vp.name) continue;
  const p = await b.newPage({ viewport: { ...vp, deviceScaleFactor: 1 }, hasTouch: true });
  p.on('pageerror', e => errs.push(String(e))); p.on('console', m => m.type() === 'error' && errs.push(m.text()));
  const shot = async n => { const f = `tests/out/ui_${vp.name}_${n}.png`; await p.screenshot({ path: f }); shots.push(f); };
  const emit = (ev, data) => p.evaluate(([e, d]) => window.__bus.emit(e, d), [ev, data]);
  const wait = ms => p.waitForTimeout(ms);
  await p.goto('http://localhost:4174/'); await p.evaluate(() => localStorage.clear()); await p.reload(); await wait(3000);
  await shot('01_title');
  await p.click('.cbtn'); await wait(3200); await shot('02_dialogue');
  for (let i = 0; i < 12 && await p.isVisible('.dlg'); i++) { const c = await p.$('.dl-choice'); if (c && await c.isVisible()) await c.click(); else await p.click('.dl-card'); await wait(250); }
  await wait(600); await shot('03_space');
  await p.evaluate(() => window.__game.goto('planet', { id: 'home' })); await wait(2000);
  for (let i = 0; i < 12 && await p.isVisible('.dlg'); i++) { await p.click('.dl-card'); await wait(250); }
  await emit('combat:combo', { n: 4 }); await emit('hero:damaged', { amount: 30, hp: 20, maxHp: 100 }); await wait(250); await shot('04_planet_combat');
  await emit('boss:spawn', { id: 'lab_warden', name: 'Le Gardien Prime', title: 'Geôlier du Laboratoire', hp: 900, maxHp: 900 });
  await emit('boss:damaged', { hp: 400, maxHp: 900 }); await emit('boss:phase', { phase: 1 }); await wait(400); await shot('05_boss');
  await emit('boss:despawn', {}); await emit('quest:start', { id: 'side_lost_pipion' }); await emit('inventory:changed', {});
  await p.evaluate(() => { const i = JSON.parse(localStorage.getItem('cosmic-ascension-save-v1') || '{}'); });
  await p.keyboard.press('Escape'); await wait(500); await shot('06_pause');
  await p.click(V + '.menu2 .cbtn:first-child'); await wait(400); await shot('07_quests');
  await p.click(V + '.cbtn'); await p.click(V + '.menu2 .cbtn:last-child'); await wait(400); await shot('08_inventory');
  await p.click(V + '.cbtn'); await p.click(V + '.menu > .cbtn.alt'); await wait(400); await shot('09_settings');
  if (vp.name === 'phone') { await p.click(V + '.row:nth-of-type(4) .tog'); await p.click(V + '.cbtn'); await p.click(V + '.cbtn'); await wait(400); await shot('10_lefty'); }
  for (let i = 0; i < 4 && await p.isVisible('.screen:not(.hidden)'); i++) { await p.keyboard.press('Escape'); await wait(200); }
  await emit('npc:interact', { npcId: 'forga' }); await wait(1500); await p.click('.dl-card'); await wait(300); await p.click('.dl-card'); await wait(1200); await shot('11_npc_dialogue');
  await p.close();
}
await b.close(); srv.kill();
console.log(shots.length + ' captures');
if (errs.length) { console.error('ERREURS:', errs); process.exit(1); } console.log('UI OK');
