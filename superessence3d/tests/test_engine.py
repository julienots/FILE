"""Tests du moteur avec le vrai Blender (bpy) : chaque outil doit réellement agir."""
import os

import bpy
import pytest

from superessence.engine.core import CommandError, ConfirmationRequired


def test_scene_and_primitives(fresh):
    r = fresh.execute("create_mesh", {"primitive": "cube", "name": "Boîte", "location": [1, 2, 3]})
    assert r["name"] == "Boîte"
    o = bpy.data.objects["Boîte"]
    assert tuple(round(x, 3) for x in o.location) == (1, 2, 3)
    info = fresh.execute("get_scene_info")
    assert any(x["name"] == "Boîte" for x in info["objects"])
    d = fresh.execute("get_object_details", {"name": "Boîte"})
    assert d["mesh"]["vertices"] == 8


def test_custom_mesh_validation(fresh):
    r = fresh.execute("create_mesh", {"vertices": [[0, 0, 0], [1, 0, 0], [0, 1, 0]], "faces": [[0, 1, 2]], "name": "Tri"})
    assert len(bpy.data.objects[r["name"]].data.polygons) == 1
    with pytest.raises(CommandError):
        fresh.execute("create_mesh", {"vertices": [[0, 0, 0]], "faces": [[0, 1, 2]]})
    with pytest.raises(CommandError):
        fresh.execute("create_mesh", {"primitive": "inconnu"})


def test_undo_redo(fresh):
    fresh.execute("create_mesh", {"primitive": "sphere", "name": "S"})
    assert "S" in bpy.data.objects
    fresh.execute("undo")
    assert "S" not in bpy.data.objects
    fresh.execute("redo")
    assert "S" in bpy.data.objects


def test_destructive_requires_confirmation(fresh):
    fresh.execute("create_mesh", {"primitive": "cube", "name": "A"})
    with pytest.raises(ConfirmationRequired):
        fresh.execute("delete_object", {"name": "A"})
    assert "A" in bpy.data.objects
    fresh.execute("delete_object", {"name": "A", "confirm": True})
    assert "A" not in bpy.data.objects


def test_transform_duplicate_parent(fresh):
    fresh.execute("create_mesh", {"primitive": "cube", "name": "P"})
    fresh.execute("create_mesh", {"primitive": "cube", "name": "C", "location": [3, 0, 0]})
    fresh.execute("transform_object", {"name": "P", "rotation": [0, 0, 90], "degrees": True})
    fresh.execute("set_parent", {"name": "C", "parent": "P"})
    c = bpy.data.objects["C"]
    assert c.parent.name == "P"
    assert abs(c.matrix_world.translation.x - 3) < 1e-4
    r = fresh.execute("duplicate_object", {"name": "P", "count": 2, "offset": [0, 5, 0]})
    assert len(r["created"]) == 2


def test_polygon_modeling(fresh):
    fresh.execute("create_mesh", {"primitive": "cube", "name": "M"})
    top = fresh.execute("select_by_normal", {"name": "M", "direction": [0, 0, 1]})["faces"]
    assert len(top) == 1
    r = fresh.execute("modify_geometry", {"name": "M", "operation": "extrude", "faces": top, "distance": 1})
    assert r["faces"] == 10
    fresh.execute("modify_geometry", {"name": "M", "operation": "inset", "faces": r["selected_faces"], "thickness": 0.2})
    fresh.execute("modify_geometry", {"name": "M", "operation": "bevel", "all": True, "width": 0.05, "segments": 2})
    fresh.execute("modify_geometry", {"name": "M", "operation": "subdivide", "all": True, "cuts": 1})
    me = bpy.data.objects["M"].data
    assert len(me.polygons) > 50
    z = max(v.co.z for v in me.vertices)
    assert z > 1.9


def test_modifiers(fresh):
    fresh.execute("create_mesh", {"primitive": "cube", "name": "Base"})
    fresh.execute("create_mesh", {"primitive": "sphere", "name": "Outil", "location": [1, 0, 1], "size": 1.5})
    for t, s in [("subdivision", {"levels": 1}), ("mirror", {"use_axis": [True, False, False]}), ("array", {"count": 3}),
                 ("bevel", {"width": 0.05})]:
        fresh.execute("add_modifier", {"name": "Base", "type": t, "settings": s})
    mods = fresh.execute("get_modifiers", {"name": "Base"})["modifiers"]
    assert [m["type"] for m in mods] == ["SUBSURF", "MIRROR", "ARRAY", "BEVEL"]
    fresh.execute("apply_modifier", {"name": "Base", "all": True})
    assert not bpy.data.objects["Base"].modifiers
    n = len(bpy.data.objects["Base"].data.vertices)
    assert n > 100
    r = fresh.execute("boolean", {"name": "Base", "cutter": "Outil", "operation": "difference"})
    assert r["faces"] > 0


def test_sculpt_and_paint(fresh):
    fresh.execute("create_mesh", {"primitive": "grid", "name": "G", "size": 2})
    me = bpy.data.objects["G"].data
    before = max(v.co.z for v in me.vertices)
    r = fresh.execute("sculpt_object", {"name": "G", "points": [[0, 0, 0, 0, 0, 1]], "radius": 0.5, "strength": 2,
                                         "brush": "draw", "iterations": 3})
    assert r["affected_vertices"] > 5
    assert max(v.co.z for v in me.vertices) > before + 0.01
    fresh.execute("sculpt_object", {"name": "G", "points": [[0, 0, 0.1]], "radius": 0.6, "brush": "smooth"})
    r = fresh.execute("vertex_paint", {"name": "G", "points": [[0, 0, 0]], "radius": 0.5, "color": "rouge"})
    assert r["affected"] > 0 and me.color_attributes.get("Couleur") is not None


def test_uv_and_materials(fresh):
    fresh.execute("create_mesh", {"primitive": "monkey", "name": "Singe"})
    fresh.execute("create_uv_map", {"name": "Singe", "method": "smart"})
    assert len(bpy.data.objects["Singe"].data.uv_layers) == 1
    for preset in ("metal", "wood", "stone", "skin", "scales", "fabric", "organic", "glass", "lava", "emission", "brick"):
        r = fresh.execute("create_material", {"preset": preset, "objects": ["Singe"], "color": "#808080"})
        mat = bpy.data.materials[r["material"]]
        assert len(mat.node_tree.nodes) >= 2
    r = fresh.execute("create_texture", {"description": "peau de monstre noire, humide, veines violettes, reliefs organiques",
                                         "objects": ["Singe"]})
    assert r["interpretation"]["preset"] == "organic"
    fresh.execute("set_material", {"material": r["material"], "roughness": 0.2, "emission": "#ff0000",
                                   "emission_strength": 2})


def test_bake(fresh):
    fresh.execute("create_mesh", {"primitive": "sphere", "name": "B"})
    fresh.execute("create_material", {"preset": "scales", "objects": ["B"], "color": "vert"})
    r = fresh.execute("bake_textures", {"name": "B", "resolution": 256, "maps": ["base_color", "roughness", "normal",
                                                                                "metallic", "height"], "samples": 2})
    maps = r["results"][0]["maps"]
    for k in ("base_color", "roughness", "normal", "metallic", "height"):
        assert os.path.getsize(maps[k]["path"]) > 100
    assert bpy.data.objects["B"].data.materials[0].name.endswith("_PBR")


@pytest.mark.parametrize("prompt", [
    "un dragon noir avec quatre ailes et des pointes", "un chevalier en armure", "un orc vert", "un robot",
    "un loup gris", "un aigle", "un requin", "une araignée", "un serpent", "un kraken", "un slime vert",
    "une maison en briques", "un château", "une tour", "un temple", "une voiture rouge", "un vaisseau spatial",
    "un bateau", "un avion", "un tank", "une épée", "une hache", "un arc", "un bouclier", "un bâton de mage",
    "un tonneau", "un coffre", "une potion", "un arbre", "un sapin", "un rocher", "un cristal", "un champignon",
    "un terrain montagneux", "une forêt", "des ruines", "un donjon", "un village",
])
def test_generation_all_categories(fresh, prompt):
    r = fresh.execute("generate_from_prompt", {"prompt": prompt, "params": {"quality": "draft", "generate_uv": False}})
    assert r["stats"]["triangles_evaluated"] > 10
    root = bpy.data.objects[r["name"]]
    assert root.get("se_spec")


def test_modify_rig_animate(fresh):
    r = fresh.execute("generate_from_prompt", {"prompt": "un dragon avec deux ailes et des yeux rouges",
                                               "params": {"quality": "draft", "generate_uv": False}})
    name = r["name"]
    m = fresh.execute("modify_model", {"name": name, "instruction": "agrandis ses ailes, ajoute six tentacules et "
                                                                     "remplace les yeux par des yeux blancs lumineux"})
    assert any("tentacles" in c for c in m["changes"])
    assert sum(1 for p in m["parts"] if "Tentacule" in p) == 6
    rig = fresh.execute("create_rig", {"name": name})
    assert rig["bones"] > 20 and rig["source"].startswith("structure")
    anim = fresh.execute("create_animation", {"name": name, "motions": ["wing_flap", "tail_wave", "breathe",
                                                                         "tentacle_wave"]})
    assert anim["animated_channels"] > 10
    arm = bpy.data.objects[rig["armature"]]
    sc = bpy.context.scene
    sc.frame_set(1)
    a = [tuple(pb.matrix.translation) for pb in arm.pose.bones]
    sc.frame_set(7)
    b = [tuple(pb.matrix.translation) for pb in arm.pose.bones]
    assert a != b, "la pose doit changer entre deux images"


def test_humanoid_walk(fresh):
    r = fresh.execute("create_character", {"keyword": "chevalier", "params": {"quality": "draft", "generate_uv": False}})
    fresh.execute("create_rig", {"name": r["name"]})
    anim = fresh.execute("create_animation", {"name": r["name"], "motions": ["walk"]})
    assert anim["animated_channels"] >= 6


def test_organic_custom(fresh):
    bones = [{"name": "corps", "head": [0, 0, 1], "tail": [0, -1, 1.1], "radius": [0.4, 0.3], "role": "spine", "segments": 2},
             {"name": "tête", "head": [0, -1, 1.1], "tail": [0, -1.5, 1.4], "radius": [0.3, 0.15], "parent": "corps", "role": "head"}]
    r = fresh.execute("create_organic_model", {"bones": bones, "name": "Bestiole", "color": "violet",
                                               "eyes": [{"head_center": [0, -1.2, 1.25], "head_radius": 0.28, "count": 3}]})
    assert len(r["parts"]) >= 4
    rig = fresh.execute("create_rig", {"name": r["name"]})
    assert rig["bones"] == 2


def test_generation_params(fresh):
    r = fresh.execute("generate_from_prompt", {"prompt": "un tonneau", "params": {
        "quality": "standard", "target_polycount": 300, "collision": True, "game_ready": True, "lods": 1,
        "texture_resolution": 512, "texture_maps": ["base_color", "normal"]}})
    pp = r["postprocess"]
    assert pp.get("triangles", 0) <= 400
    assert pp["collision"] and all(c.startswith("UCX_") for c in pp["collision"])
    assert pp["lods"]
    assert all("error" not in b for b in pp["baked"])
    assert abs(r["spec"]["height"] - 0.9) < 0.05


@pytest.mark.parametrize("fmt", ["glb", "gltf", "obj", "fbx", "stl", "ply", "blend"])
def test_export_formats(fresh, fmt):
    fresh.execute("generate_from_prompt", {"prompt": "un coffre", "name": "Coffre",
                                           "params": {"quality": "draft", "generate_uv": True}})
    r = fresh.execute("export_model", {"format": fmt, "name": "Coffre", "filename": f"test_{fmt}"})
    assert r["verification"]["valid"] and os.path.getsize(r["path"]) > 0
    if fmt in ("glb", "obj", "fbx", "stl", "ply"):
        imp = fresh.execute("import_model", {"path": r["path"]})
        assert imp["imported"]


@pytest.mark.parametrize("profile", ["unity", "unreal", "godot", "mobile", "print3d", "high_quality"])
def test_export_profiles(fresh, profile):
    fresh.execute("generate_from_prompt", {"prompt": "un loup", "name": "Loup", "params": {"quality": "draft",
                                                                                         "generate_uv": False}})
    fresh.execute("create_rig", {"name": "Loup"})
    fresh.execute("create_animation", {"name": "Loup", "motions": ["walk"]})
    r = fresh.execute("export_model", {"profile": profile, "name": "Loup"})
    assert r["verification"]["valid"]
    if profile in ("godot", "high_quality", "mobile"):
        assert r["verification"]["animations"] >= 1
        assert r["verification"]["skins"] >= 1


def test_render(fresh):
    fresh.execute("create_mesh", {"primitive": "monkey"})
    fresh.execute("create_lighting", {"preset": "studio"})
    fresh.execute("set_camera", {"frame_objects": True})
    r = fresh.execute("render_scene", {"samples": 4, "resolution": [160, 90]})
    assert os.path.getsize(r["path"]) > 1000


def test_project_save_load(fresh):
    fresh.execute("create_mesh", {"primitive": "torus", "name": "Anneau"})
    r = fresh.execute("save_project", {"name": "test_projet"})
    assert os.path.exists(r["path"])
    fresh.execute("new_project", {"confirm": True})
    assert "Anneau" not in bpy.data.objects
    fresh.execute("load_project", {"path": r["path"], "confirm": True})
    assert "Anneau" in bpy.data.objects


def test_library(fresh):
    g = fresh.execute("generate_from_prompt", {"prompt": "une épée", "name": "Lame",
                                               "params": {"quality": "draft", "generate_uv": False}})
    s = fresh.execute("library_save", {"name": g["name"], "tags": ["arme", "test"]})
    assert os.path.exists(s["blend"]) and os.path.exists(s["glb"])
    lst = fresh.execute("library_list", {"query": "arme"})
    assert any(m["id"] == s["model_id"] for m in lst["models"])
    s2 = fresh.execute("library_save", {"name": g["name"], "model_id": s["model_id"], "note": "v2"})
    assert s2["version"] == 2
    fresh.execute("library_update", {"model_id": s["model_id"], "model_name": "Épée test", "category": "Armes"})
    d = fresh.execute("library_duplicate", {"model_id": s["model_id"]})
    loaded = fresh.execute("library_load", {"model_id": s["model_id"], "location": [3, 0, 0]})
    assert loaded["objects"]
    with pytest.raises(ConfirmationRequired):
        fresh.execute("library_delete", {"model_id": d["model_id"]})
    fresh.execute("library_delete", {"model_id": d["model_id"], "confirm": True})


def _write_test_image(path, kind="disc"):
    import numpy as np
    n = 96
    img = bpy.data.images.new("t", n, n, alpha=True)
    px = np.zeros((n, n, 4), dtype=np.float32)
    yy, xx = np.mgrid[0:n, 0:n]
    if kind == "disc":
        mask = (xx - 48) ** 2 + (yy - 48) ** 2 < 30 ** 2
    else:
        mask = (abs(xx - 48) < 20) & (abs(yy - 48) < 36)
    px[mask] = (0.8, 0.2, 0.1, 1.0)
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def test_image_to_model(fresh, tmp_path):
    a = str(tmp_path / "face.png")
    b = str(tmp_path / "profil.png")
    _write_test_image(a, "disc")
    _write_test_image(b, "rect")
    r = fresh.execute("image_to_model", {"image": a, "height": 1, "resolution": 48})
    o = bpy.data.objects[r["name"]]
    assert len(o.data.vertices) > 100 and "non neuronal" in r["method"]
    r2 = fresh.execute("image_to_model", {"images": [a, b], "height": 1, "resolution": 40})
    assert len(bpy.data.objects[r2["name"]].data.vertices) > 100


def test_inspect_and_optimize(fresh):
    fresh.execute("create_mesh", {"primitive": "sphere", "name": "S"})
    r = fresh.execute("inspect_model", {"name": "S"})
    assert r["watertight"] and r["triangles"] > 0
    o = fresh.execute("optimize_mesh", {"name": "S", "target_triangles": 200})
    assert o["report"][0]["triangles_after"] <= 220


def test_viewport_payload(fresh):
    fresh.execute("create_mesh", {"primitive": "cube", "name": "V"})
    fresh.execute("add_modifier", {"name": "V", "type": "subdivision"})
    d = fresh.execute("get_mesh", {"name": "V"})
    assert d["triangles"] > 12 and d["positions"]
    again = fresh.execute("get_mesh", {"name": "V", "signature": d["signature"]})
    assert again.get("unchanged")
    e = fresh.execute("get_mesh", {"name": "V", "evaluated": False})
    assert e["edit"]["co"] and e["faces"] == 6


def test_collections_scenes_lights_cameras(fresh):
    fresh.execute("create_mesh", {"primitive": "cube", "name": "X"})
    fresh.execute("move_to_collection", {"name": "X", "collection": "Décor"})
    assert "X" in bpy.data.collections["Décor"].objects
    fresh.execute("create_light", {"type": "SPOT", "energy": 300})
    fresh.execute("create_camera", {})
    fresh.execute("scene_manage", {"action": "create", "name": "Niveau 2"})
    assert bpy.context.scene.name == "Niveau 2"


def test_python_disabled_by_default(fresh):
    with pytest.raises(CommandError):
        fresh.execute("run_python", {"code": "result = 1"})


def test_param_validation(fresh):
    with pytest.raises(CommandError):
        fresh.execute("create_mesh", {"primitive": "cube", "size": -1})
    with pytest.raises(CommandError):
        fresh.execute("get_object_details", {"name": "nexistepas"})
    with pytest.raises(CommandError):
        fresh.execute("commande_inconnue")
