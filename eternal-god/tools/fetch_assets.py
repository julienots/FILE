#!/usr/bin/env python3
"""Télécharge les ressources CC0 de Poly Haven (https://polyhaven.com, licence CC0) dans tools/.cache/.
Les fichiers bruts ne sont pas versionnés : tools/bake_assets.* les convertit en ressources mobiles.
"""
import json
import os
import sys
import urllib.request

ROOT = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(ROOT, ".cache", "polyhaven")

TEXTURES = [
    # terrain
    "rocky_terrain_02", "forrest_ground_01", "mossy_rock", "snow_02", "coast_sand_01", "grass_path_2",
    "burned_ground_01", "dark_rock", "raked_dirt", "cobblestone_floor_01", "aerial_grass_rock",
    # bâtiments
    "plastered_wall_02", "weathered_brown_planks", "clay_roof_tiles_02", "roof_slates_02",
    "thatch_roof_angled", "castle_wall_slates", "marble_01",
]
HDRIS = [
    "qwantani_dawn_puresky", "qwantani_sunrise_puresky", "qwantani_morning_puresky", "qwantani_noon_puresky",
    "qwantani_late_afternoon_puresky", "qwantani_sunset_puresky", "qwantani_dusk_2_puresky", "qwantani_night_puresky",
]
MODELS = ["island_tree_02", "island_tree_01", "fir_sapling_medium", "shrub_02", "fern_02", "rock_moss_set_01", "boulder_01", "grass_medium_01"]


UA = {"User-Agent": "EternalGod-asset-fetcher/1.0"}


def get(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    tmp = dest + ".part"
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as r, open(tmp, "wb") as f:
        while True:
            b = r.read(1 << 20)
            if not b:
                break
            f.write(b)
    os.replace(tmp, dest)


def files(asset):
    with urllib.request.urlopen(urllib.request.Request(f"https://api.polyhaven.com/files/{asset}", headers=UA), timeout=60) as r:
        return json.load(r)


def main():
    for t in TEXTURES:
        f = files(t)
        for key in ("Diffuse", "nor_gl"):
            u = f[key]["1k"]["jpg"]["url"]
            get(u, os.path.join(CACHE, "tex", f"{t}_{key.lower()}.jpg"))
        print("texture", t, flush=True)
    for h in HDRIS:
        f = files(h)
        get(f["hdri"]["2k"]["hdr"]["url"], os.path.join(CACHE, "hdri", f"{h}.hdr"))
        print("hdri", h, flush=True)
    for m in MODELS:
        f = files(m)["gltf"]["1k"]["gltf"]
        d = os.path.join(CACHE, "models", m)
        get(f["url"], os.path.join(d, f"{m}.gltf"))
        for rel, info in f.get("include", {}).items():
            get(info["url"], os.path.join(d, rel))
        print("model", m, flush=True)


if __name__ == "__main__":
    sys.exit(main())
