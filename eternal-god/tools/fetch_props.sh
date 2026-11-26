#!/bin/sh
# Accessoires de village scannés (Poly Haven, CC0) : téléchargés puis simplifiés pour mobile.
set -e
GT=${GT:-gltf-transform}
OUT=app/src/main/assets/game/models/props
CACHE=tools/.cache/polyhaven/props
mkdir -p $OUT $CACHE
for m in wooden_barrels_01 wooden_crate_02 barrel_03 wooden_bucket_01 stone_fire_pit wooden_picnic_table wooden_lantern_01 wine_barrel_01; do
  python3 - "$m" "$CACHE" <<'PY'
import json, os, sys, urllib.request
m, cache = sys.argv[1], sys.argv[2]
UA = {"User-Agent": "EternalGod-asset-fetcher/1.0"}
f = json.load(urllib.request.urlopen(urllib.request.Request(f"https://api.polyhaven.com/files/{m}", headers=UA)))["gltf"]["1k"]["gltf"]
d = os.path.join(cache, m)
for url, rel in [(f["url"], f"{m}.gltf")] + [(v["url"], k) for k, v in f.get("include", {}).items()]:
    p = os.path.join(d, rel); os.makedirs(os.path.dirname(p), exist_ok=True)
    if not os.path.exists(p):
        open(p, "wb").write(urllib.request.urlopen(urllib.request.Request(url, headers=UA)).read())
PY
  $GT weld $CACHE/$m/$m.gltf /tmp/claude-0/rk/p-$m-w.glb >/dev/null
  $GT simplify /tmp/claude-0/rk/p-$m-w.glb /tmp/claude-0/rk/p-$m-s.glb --ratio 0.15 --error 0.002 >/dev/null
  $GT resize /tmp/claude-0/rk/p-$m-s.glb /tmp/claude-0/rk/p-$m-r.glb --width 512 --height 512 >/dev/null
  $GT webp /tmp/claude-0/rk/p-$m-r.glb $OUT/$m.glb --quality 82 >/dev/null
  echo "$m $(du -k $OUT/$m.glb | cut -f1)KB"
done
