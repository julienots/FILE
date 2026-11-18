#!/usr/bin/env python3
"""Assemble dist/index.html (page + game.js inline) for publishing; models/ is served alongside."""
import os, shutil
here = os.path.dirname(os.path.abspath(__file__))
html = open(os.path.join(here, 'index.html'), encoding='utf-8').read()
js = open(os.path.join(here, 'game.js'), encoding='utf-8').read()
html = html.replace('<script src="game.js"></script>', '<script>\n' + js.replace('</script', '<\\/script') + '\n</script>')
os.makedirs(os.path.join(here, 'dist'), exist_ok=True)
open(os.path.join(here, 'dist', 'index.html'), 'w', encoding='utf-8').write(html)
print('dist/index.html', len(html))
# modèles : .glb -> .txt base64 (types servis par l'hébergeur d'artifacts)
import json, base64
man = json.load(open(os.path.join(here, 'models', 'manifest.json'), encoding='utf-8'))
os.makedirs(os.path.join(here, 'dist', 'models'), exist_ok=True)
for m in man['models']:
    src = os.path.join(here, 'models', m['file'])
    if m['file'].endswith('.glb') and os.path.exists(src):
        out = m['file'][:-4] + '.glb.txt'
        open(os.path.join(here, 'dist', 'models', out), 'w').write(base64.b64encode(open(src, 'rb').read()).decode())
        m['file'] = out
json.dump(man, open(os.path.join(here, 'dist', 'models', 'manifest.json'), 'w', encoding='utf-8'), ensure_ascii=False)
print('models:', [m['file'] for m in man['models']])
