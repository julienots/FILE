// Intègre les fichiers de public/ dans le Worker (src/assets.gen.js).
// Lancé automatiquement par `wrangler deploy` / `wrangler dev` (voir [build] dans wrangler.toml).
import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join, relative, extname } from "node:path";
const root = new URL("../public/", import.meta.url).pathname;
const TYPES = { ".html": "text/html; charset=utf-8", ".css": "text/css; charset=utf-8", ".js": "text/javascript; charset=utf-8",
  ".svg": "image/svg+xml", ".png": "image/png", ".webmanifest": "application/manifest+json", ".json": "application/json" };
const out = {};
(function walk(dir) {
  for (const f of readdirSync(dir).sort()) {
    const p = join(dir, f);
    if (statSync(p).isDirectory()) walk(p);
    else out["/" + relative(root, p)] = { type: TYPES[extname(f)] || "application/octet-stream", b64: readFileSync(p).toString("base64") };
  }
})(root);
writeFileSync(new URL("../src/assets.gen.js", import.meta.url), "// Généré par scripts/embed-assets.mjs : ne pas modifier.\nexport default " + JSON.stringify(out) + ";\n");
console.log(`AXON : ${Object.keys(out).length} fichiers intégrés.`);
