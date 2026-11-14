// node tests/models.mjs <dir sous public/models> <sortie.png> [yaw]  → planche d'aperçu de tous les GLB du dossier
import { createRequire } from 'module'; import { spawn } from 'child_process'; import { readdirSync } from 'fs';
const { chromium } = createRequire(import.meta.url)('playwright');
const [,, dir = '', out = 'tests/out/models.png', yaw = '.6'] = process.argv, files = readdirSync('public/models/' + dir).filter(f => f.endsWith('.glb')).sort();
const srv = spawn('npx', ['vite', '--port', '5199', '--strictPort'], { stdio: 'ignore' }); await new Promise(r => setTimeout(r, 3000));
const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] }), p = await b.newPage({ viewport: { width: 1280, height: 1000 } });
p.on('console', m => console.log(m.text())); p.on('pageerror', e => console.log('ERR', e));
await p.goto(`http://localhost:5199/tests/models.html?dir=${dir}&files=${files.join(',')}&yaw=${yaw}`); await p.waitForFunction(() => document.title === 'done', null, { timeout: 120000 });
await p.locator('#c').screenshot({ path: out }); await b.close(); srv.kill(); console.log(files.join(' | '));
