// Mesure rapide : appels de dessin, triangles et erreurs après quelques secondes de jeu.
const { chromium } = require('playwright');
(async () => {
  const q = process.argv[2] || 'medium';
  const b = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const p = await (await b.newContext({ viewport: { width: 915, height: 412 }, hasTouch: true, isMobile: true })).newPage();
  const errors = [];
  p.on('pageerror', (e) => errors.push(e.message));
  p.on('console', (m) => { if (m.type() === 'error' && !/GPU stall|GL Driver/.test(m.text())) errors.push(m.text()); });
  await p.addInitScript((qq) => { localStorage.setItem('eg.settings', JSON.stringify({ quality: qq })); localStorage.removeItem('eg.save.v1'); }, q);
  const t0 = Date.now();
  await p.goto('http://127.0.0.1:8765/app/src/main/assets/game/index.html');
  await p.waitForSelector('#title:not(.hidden)', { timeout: 1500000 });
  const lt = (Date.now() - t0) / 1000;
  await p.mouse.click(450, 200);
  await p.waitForTimeout(5000);
  await p.evaluate(() => { G.god.toggleFly(); });
  await p.waitForTimeout(5000);
  const info = await p.evaluate(() => ({ r: G.renderer.info.render, m: G.renderer.info.memory, prog: G.renderer.info.programs.length, people: G.people.list.length }));
  const top = await p.evaluate(() => {
    const rows = [];
    G.scene.traverse((o) => {
      if (!o.isMesh || !o.visible) return;
      let vis = true; let a = o; while (a) { if (!a.visible) { vis = false; break; } a = a.parent; } if (!vis) return;
      const g = o.geometry; const tri = (g.index ? g.index.count : g.attributes.position.count) / 3;
      const n = o.isInstancedMesh ? o.count : (g.isInstancedBufferGeometry ? g.instanceCount : 1);
      rows.push([(o.isInstancedMesh ? 'inst:' + Math.round(tri) + 'tri' : o.material.type + (o.isSkinnedMesh ? ':skin' : '')), Math.round(tri * n), n]);
    });
    const agg = {}; for (const [k, t, n] of rows) { agg[k] = agg[k] || [0, 0, 0]; agg[k][0] += t; agg[k][1] += n; agg[k][2]++; }
    return Object.entries(agg).sort((a, b) => b[1][0] - a[1][0]).slice(0, 14).map(([k, v]) => `${k}: ${v[0]} tris, ${v[2]} objets, ${v[1]} inst`);
  });
  console.log(top.join('\n'));
  console.log(q, 'load', lt.toFixed(1) + 's', JSON.stringify(info), 'errors', errors.length, JSON.stringify(errors.slice(0, 5)));
  await b.close();
})();
