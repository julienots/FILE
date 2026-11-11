// Capture d'écran de contrôle : node tests/shot.mjs "<query>" out.png [steps] [evalJS]
import { createRequire } from 'module';
const { chromium } = createRequire(import.meta.url)(process.env.PW_PATH || 'playwright');
import http from 'http'; import fs from 'fs'; import path from 'path';
const root = path.resolve('www');
const srv = http.createServer((q, r) => { let f = path.join(root, decodeURIComponent(q.url.split('?')[0])); if (f.endsWith('/')) f += 'index.html'; fs.readFile(f, (e, d) => { if (e) { r.writeHead(404); r.end(); } else { r.writeHead(200, { 'content-type': f.endsWith('.js') ? 'text/javascript' : f.endsWith('.css') ? 'text/css' : f.endsWith('.json') ? 'application/json' : 'text/html' }); r.end(d); } }); }).listen(0);
const port = srv.address().port;
const [query = '', out = 'shot.png', steps = '30', js = '', post = '', vw = '1280', vh = '720'] = process.argv.slice(2);
const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const p = await b.newPage({ viewport: { width: +vw, height: +vh } });
const errs = [];
p.on('pageerror', (e) => errs.push('PAGEERROR ' + e.message)); p.on('console', (m) => { if (m.type() === 'error' || m.type() === 'warning') errs.push(m.type() + ' ' + m.text()); else if (m.text().startsWith('#')) console.log(m.text()); });
await p.goto(`http://localhost:${port}/index.html?${query}`);
await p.waitForFunction(() => window.__SL && window.__SL.ready, null, { timeout: 120000 });
if (js) console.log('eval:', JSON.stringify(await p.evaluate(js)));
await p.evaluate((n) => { const g = window.__SL.game; g.running = false; for (let i = 0; i < n; i++) g.tick(1 / 30); g.render(); }, +steps);
if (post) { console.log('post:', JSON.stringify(await p.evaluate(post))); }
await p.screenshot({ path: out });
console.log(errs.slice(0, 12).join('\n') || 'no errors');
await b.close(); srv.close();
