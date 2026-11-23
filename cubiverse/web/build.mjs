// Construit la version web (dist/) : un seul script classique (compatible file:// dans la WebView Android).
import * as esbuild from 'esbuild';
import { cpSync, mkdirSync, rmSync } from 'node:fs';
const dev = process.argv.includes('--dev');
rmSync('dist', { recursive: true, force: true });
mkdirSync('dist', { recursive: true });
cpSync('public', 'dist', { recursive: true });
await esbuild.build({
  entryPoints: { app: 'src/main.js' },
  bundle: true, format: 'iife', target: ['chrome80'], minify: !dev, sourcemap: dev,
  outdir: 'dist', loader: { '.ttf': 'file' }, assetNames: 'fonts/[name]', legalComments: 'none',
});
console.log('dist/ prêt');
