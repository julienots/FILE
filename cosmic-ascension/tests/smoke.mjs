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
await p.evaluate(() => { const g = window.__game.current, h = g.serpent.headPos; g.ship.root.position.set(h.x + 260, h.y + 120, h.z + 520); g.yaw = .45; g.pitch = -.2; g.camera.position.copy(g.ship.root.position); }); await p.waitForTimeout(2500); await p.screenshot({ path: 'tests/out/serpent.png' });
await p.evaluate(() => { const g = window.__game.current, c = g.serpent.cores[2].pos; g.ship.root.position.set(c.x + 60, c.y + 160, c.z + 200); g.yaw = .29; g.pitch = -.6; g.vel.set(0, 0, 0); g.camera.position.copy(g.ship.root.position); }); await p.keyboard.down('KeyJ'); await p.waitForTimeout(1500); await p.screenshot({ path: 'tests/out/bossfight.png' }); await p.keyboard.up('KeyJ');
console.log('cores hp', await p.evaluate(() => window.__game.current.serpent.cores.map(c => c.hp).join()));
await p.evaluate(() => { const g = window.__game.current; for (const c of g.serpent.cores) g.serpent.damageAt(c.pos, 99); }); await p.waitForTimeout(2500); await p.screenshot({ path: 'tests/out/bossdead.png' });
console.log('stage after boss', await p.evaluate(() => JSON.parse(JSON.stringify(window.__game && 1)) && (document.querySelector('#fb-name') ?? document.querySelector('.hud .nm'))?.textContent));
await p.evaluate(() => { const g = window.__game.current, d = g.devourer.root.position; g.ship.root.position.set(d.x - 250, d.y + 150, d.z + 800); g.yaw = -.3; g.pitch = -.18; g.vel.set(0, 0, 0); g.camera.position.copy(g.ship.root.position); }); await p.waitForTimeout(3000); await p.screenshot({ path: 'tests/out/devourer.png' });
await p.evaluate(() => { const g = window.__game.current; g.ship.root.position.set(0, 0, 60); g.yaw = 0; g.pitch = 0; g.vel.set(0, 0, 0); }); await p.waitForTimeout(500);
await p.keyboard.down('ShiftLeft'); await p.waitForTimeout(1500); await p.screenshot({ path: 'tests/out/boost.png' });
await p.waitForTimeout(2500); await p.keyboard.up('ShiftLeft'); await p.keyboard.press('KeyE'); await p.waitForTimeout(1900); await p.screenshot({ path: 'tests/out/arrive.png' });
console.log('scene:', await p.evaluate(() => window.__game.currentName)); await p.waitForTimeout(2500);
await p.keyboard.press('KeyE'); await p.waitForTimeout(2200); await p.screenshot({ path: 'tests/out/takeoff.png' }); console.log('scene:', await p.evaluate(() => window.__game.currentName));
await p.evaluate(() => window.__game.goto('planet', { id: 'home' })); await p.waitForTimeout(2000);
await p.evaluate(async () => { const g = window.__game.current; for (const s of g.base.slots.slice(0, 5)) g.base.slots && window.__bus.emit('build:placed', { planetId: 'home', slot: s.i, buildingId: ['house', 'greenhouse', 'lab', 'forge', 'temple'][g.base.count() % 5] }); g.camPitch = .5; }); await p.waitForTimeout(1500); await p.screenshot({ path: 'tests/out/homebase.png' });
await p.waitForTimeout(500);
await p.screenshot({ path: 'tests/out/planet.png' });
for (const id of ['ember', 'glacio', 'sylva']) { await p.evaluate((id) => window.__game.goto('planet', { id }), id); await p.waitForTimeout(1500); await p.screenshot({ path: `tests/out/planet-${id}.png` }); }
await p.evaluate(() => { const g = window.__game.current, n = g.structs.npcs[0], t = n.dir.clone().cross({ x: 0, y: 1, z: 0 }).normalize(); g.dir.copy(n.dir).addScaledVector(t, .035).normalize(); g.fwd.copy(t).negate(); g.face.copy(g.fwd); g.camPitch = .28; }); await p.waitForTimeout(1500);
if (await p.evaluate(() => !!window.__game.current.nearNpc)) await p.keyboard.press('KeyE'); else console.log('WARN: pas de PNJ proche'); await p.waitForTimeout(700); await p.screenshot({ path: 'tests/out/village.png' });
await p.evaluate(() => { const g = window.__game.current, e = g.enemies.list[0]; e.dir.copy(g.dir).applyAxisAngle(g.fwd.clone().cross(g.dir).normalize(), -.05); e.home.copy(e.dir); }); await p.keyboard.down('KeyJ'); await p.waitForTimeout(900); await p.screenshot({ path: 'tests/out/combat.png' }); await p.keyboard.up('KeyJ');
await p.keyboard.press('KeyK'); await p.waitForTimeout(500); await p.screenshot({ path: 'tests/out/shockwave.png' }); await p.waitForTimeout(4500); await p.screenshot({ path: 'tests/out/evolved4.png' });
console.log('fps~', await p.evaluate(() => new Promise(r => { let n = 0; const t0 = performance.now(); const f = () => { n++; performance.now() - t0 > 1500 ? r(Math.round(n / 1.5)) : requestAnimationFrame(f); }; f(); })));
for (const st of [0, 1, 2, 3, 4, 5]) { await p.evaluate((st) => { const g = window.__game.current; g.hero.setStage(st); g.fwd.copy(g.face).negate(); g.camPitch = .22; }, st); await p.waitForTimeout(900); await p.screenshot({ path: `tests/out/hero${st}.png`, clip: { x: 440, y: 120, width: 400, height: 360 } }); }
await b.close(); srv.kill();
if (errs.length) { console.error('ERREURS:', errs); process.exit(1); } console.log('SMOKE OK');
