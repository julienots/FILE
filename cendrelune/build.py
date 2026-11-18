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
