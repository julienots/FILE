// Assemble le jeu dans dist/ : game.js (code + moteur 3D), index.html, décodeur Draco, modèles.
import { build } from 'esbuild';
import { readFileSync, writeFileSync, mkdirSync, cpSync, existsSync } from 'node:fs';
mkdirSync('dist/draco', { recursive: true });
await build({ entryPoints: ['src/main.js'], bundle: true, format: 'esm', minify: true, target: 'es2020', outfile: 'dist/game.js', logLevel: 'warning' });
const page = readFileSync('src/page.html', 'utf8');
writeFileSync('dist/index.html', `<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><style>body{margin:0}[hidden]{display:none!important}</style></head><body>${page}</body></html>`);
for (const f of ['draco_wasm_wrapper.js', 'draco_decoder.wasm', 'draco_decoder.js']) cpSync(`node_modules/three/examples/jsm/libs/draco/gltf/${f}`, `dist/draco/${f}`);
if (existsSync('models')) cpSync('models', 'dist/models', { recursive: true });
console.log('ok');
