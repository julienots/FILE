#!/usr/bin/env python3
"""Télécharge les assets CC0 de Poly Haven utilisés par le jeu (non versionnés dans Git).
Usage : python3 tools/fetch_assets.py   (depuis le dossier the-perfect-life)"""
import json, os, sys, urllib.request, concurrent.futures as cf

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "polyhaven")
API = "https://api.polyhaven.com/files/"

MODELS = """sofa_02 sofa_03 modern_arm_chair_01 mid_century_lounge_chair modern_coffee_table_01
modern_coffee_table_02 side_table_01 throw_pillows_01 potted_plant_01 potted_plant_02 potted_plant_04
modern_ceiling_lamp_01 modern_wooden_cabinet hanging_picture_frame_01 hanging_picture_frame_02
book_encyclopedia_set_01 ceramic_vase_01 ceramic_vase_03 standing_picture_frame_01
dining_chair_02 wooden_table_02 bar_chair_round_01 electric_stove wooden_cutting_board food_apple_01
bananas lemon croissant jug_01 tea_set_01 wooden_bowl_01 wine_bottles_01 ClassicNightstand_01
painted_wooden_nightstand drawer_cabinet desk_lamp_arm_01 alarm_clock_01 ornate_mirror_01 wall_clock
outdoor_table_chair_set_01 street_lamp_01 fire_hydrant planter_box_01 security_camera_01 shrub_01
shrub_02 shrub_03 shrub_04 island_tree_01 jacaranda_tree tree_small_02 grass_medium_01 grass_bermuda_01
flower_gazania utility_box_01 metal_trash_can gaming_console ceiling_fan vintage_electric_kettle""".split()

TEXTURES = """laminate_floor_02 marble_01 white_plaster_02 painted_plaster_wall kitchen_wood white_oak_veneer
long_white_tiles large_grey_tiles white_planks_clean grey_roof_tiles roof_tiles_14 clean_asphalt
concrete_pavement grooved_concrete_driveway leafy_grass fabric_pattern_07 leather_white
quatrefoil_jacquard_fabric dark_wood wood_floor_deck granite_tile""".split()
TEX_MAPS = {"Diffuse": "diff", "nor_gl": "nor", "arm": "arm"}

HDRIS = ["kloofendal_48d_partly_cloudy_puresky", "suburban_garden"]

def get(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "game-asset-fetch"})
    with urllib.request.urlopen(req, timeout=300) as r, open(dest + ".part", "wb") as f:
        f.write(r.read())
    os.replace(dest + ".part", dest)

def info(name):
    req = urllib.request.Request(API + name, headers={"User-Agent": "game-asset-fetch"})
    return json.load(urllib.request.urlopen(req, timeout=60))

def jobs():
    for m in MODELS:
        g = info(m)["gltf"]["2k"]["gltf"]
        base = os.path.join(ROOT, "models", m)
        yield g["url"], os.path.join(base, m + ".gltf")
        for rel, f in g["include"].items():
            yield f["url"], os.path.join(base, rel)
    for t in TEXTURES:
        d = info(t)
        for key, short in TEX_MAPS.items():
            if key == "Diffuse" and key not in d:
                key = next(k for k in d if k.startswith("col"))
            yield d[key]["2k"]["jpg"]["url"], os.path.join(ROOT, "textures", t, f"{t}_{short}.jpg")
    for h in HDRIS:
        yield info(h)["hdri"]["2k"]["hdr"]["url"], os.path.join(ROOT, "hdri", h + ".hdr")

if __name__ == "__main__":
    todo = list(jobs())
    print(len(todo), "fichiers")
    with cf.ThreadPoolExecutor(8) as ex:
        for fut in cf.as_completed([ex.submit(get, u, d) for u, d in todo]):
            fut.result()
    print("OK")
