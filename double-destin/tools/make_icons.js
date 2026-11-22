// Rend le logo du jeu en icônes Android (mipmap) et en image de présentation.
const { chromium } = require("playwright");
const fs = require("fs"), path = require("path");
const core = fs.readFileSync(path.join(__dirname, "../www/js/core.js"), "utf8");
(async () => {
  const b = await chromium.launch(); const p = await b.newPage();
  await p.setContent(`<html><body style="margin:0;background:transparent"><script>${core}</script>
    <div id="ic" style="width:512px;height:512px;border-radius:110px;background:radial-gradient(circle at 50% 35%,#2b2140,#09090f 75%);display:grid;place-items:center"></div></body></html>`);
  await p.evaluate(() => { document.getElementById("ic").innerHTML = logoSVG(430); });
  const el = await p.$("#ic");
  const big = path.join(__dirname, "../branding"); fs.mkdirSync(big, { recursive: true });
  await el.screenshot({ path: path.join(big, "icon-512.png"), omitBackground: true });
  const sizes = { mdpi: 48, hdpi: 72, xhdpi: 96, xxhdpi: 144, xxxhdpi: 192 };
  for (const [d, s] of Object.entries(sizes)) {
    const dir = path.join(__dirname, "../android/res/mipmap-" + d); fs.mkdirSync(dir, { recursive: true });
    await p.setViewportSize({ width: 600, height: 600 });
    await el.screenshot({ path: path.join(dir, "ic_launcher.png"), omitBackground: true, scale: "css" });
  }
  await b.close();
})();
