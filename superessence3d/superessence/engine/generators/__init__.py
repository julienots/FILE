"""Moteur de génération procédurale (« SUPERESSENCE TRIPO », mode procédural).

IMPORTANT (transparence) : ces générateurs construisent la géométrie par règles et
algorithmes (graphes de squelette, révolutions, lofts, membranes, booléens, textures
procédurales). Ce n'est PAS une génération neuronale texte→3D ou image→3D. Les moteurs
neuronaux sont facultatifs et se branchent via ai_backends.py.
"""
from __future__ import annotations

import time

import bpy
from mathutils import Vector

from ..core import CommandError, Params, command
from ..util import get_json_prop, get_object, hierarchy, select_only, set_json_prop, world_bbox
from . import postprocess
from .builder import Model
from .prompt import CATEGORY_LABELS, parse_modification, parse_prompt

QUALITY = {
    "draft": {"_subdiv": 1, "_terrain_res": 48, "detail": "low"},
    "standard": {"_subdiv": 2, "_terrain_res": 96},
    "high": {"_subdiv": 2, "_terrain_res": 128, "detail": "high"},
    "ultra": {"_subdiv": 3, "_terrain_res": 192, "detail": "high"},
}

DEFAULT_HEIGHTS = {
    ("creature", "dragon"): 2.5, ("creature", "hydra"): 3.0, ("creature", "kraken"): 4.0, ("creature", "blob"): 1.2,
    ("creature", "demon"): 2.4, ("creature", "golem"): 3.0, ("creature", "griffin"): 2.0, ("creature", "alien"): 1.7,
    ("creature", "beast"): 1.8, ("character", "humanoid"): 1.8,
    ("animal", "wolf"): 0.85, ("animal", "cat"): 0.35, ("animal", "horse"): 1.7, ("animal", "bear"): 1.4,
    ("animal", "deer"): 1.6, ("animal", "cow"): 1.5, ("animal", "rabbit"): 0.3, ("animal", "bird"): 0.4,
    ("animal", "fish"): 0.4, ("animal", "snake"): 0.25, ("animal", "spider"): 0.15, ("animal", "lizard"): 0.25,
    ("vehicle", "car"): 1.45, ("vehicle", "truck"): 3.2, ("vehicle", "spaceship"): 4.0, ("vehicle", "boat"): 8.5,
    ("vehicle", "plane"): 4.0, ("vehicle", "tank"): 2.6, ("vehicle", "bike"): 1.1,
    ("weapon", "sword"): 1.2, ("weapon", "dagger"): 0.4, ("weapon", "axe"): 0.95, ("weapon", "spear"): 2.3,
    ("weapon", "hammer"): 0.95, ("weapon", "bow"): 1.4, ("weapon", "staff"): 1.9, ("weapon", "shield"): 1.0,
    ("weapon", "scythe"): 1.9, ("weapon", "gun"): 0.3,
    ("prop", "barrel"): 0.9, ("prop", "crate"): 0.8, ("prop", "chest"): 0.65, ("prop", "table"): 0.78,
    ("prop", "chair"): 0.95, ("prop", "lamp"): 0.74, ("prop", "bottle"): 0.3, ("prop", "rock"): 1.0,
    ("prop", "tree"): 7.0, ("prop", "bush"): 1.2, ("prop", "mushroom"): 0.6, ("prop", "crystal"): 0.9,
    ("prop", "well"): 2.4, ("prop", "fence"): 1.1, ("prop", "crown"): 0.15,
}

ACCEPTED_SPEC_KEYS = {"category", "subtype", "keyword", "height", "colors", "features", "scales", "material", "style",
                      "build", "detail", "glow", "glow_eyes", "wet", "seed", "floors", "count", "body_plan", "species",
                      "terrain_size", "prompt", "size_hint", "tags", "notes"}


def default_params() -> dict:
    return {"quality": "standard", "symmetry": True, "smoothing": True, "subdivision": None, "remesh": False,
            "resolution": None, "target_polycount": 0, "texture_resolution": 0, "texture_maps":
            ["base_color", "roughness", "metallic", "normal", "height", "ao"], "generate_materials": True,
            "generate_uv": True, "auto_optimize": True, "fix_normals": True, "cleanup": True, "fill_holes": False,
            "collision": False, "game_ready": False, "lods": 0, "apply_modifiers": False}


def normalize_spec(spec: dict) -> dict:
    s = {k: v for k, v in spec.items() if k in ACCEPTED_SPEC_KEYS or k.startswith("_")}
    cat = s.get("category", "creature")
    if cat not in CATEGORY_LABELS:
        raise CommandError(f"Catégorie inconnue : « {cat} ». Choix : {', '.join(CATEGORY_LABELS)}")
    s["category"] = cat
    s["category_label"] = CATEGORY_LABELS[cat]
    s.setdefault("features", {})
    s.setdefault("colors", {})
    s.setdefault("scales", {})
    for k, v in list(s["colors"].items()):
        from ..colors import parse_color
        c = parse_color(v)
        if c is None:
            raise CommandError(f"Couleur non reconnue pour « {k} » : {v!r}")
        s["colors"][k] = list(c[:3])
    return s


def generate(engine, spec: dict, params: dict | None = None, name: str | None = None, location=None,
             rotation=None) -> dict:
    t0 = time.time()
    params = {**default_params(), **(params or {})}
    q = params.get("quality", "standard")
    if q not in QUALITY:
        raise CommandError("Qualité : draft, standard, high ou ultra.")
    spec = normalize_spec(spec)
    for k, v in QUALITY[q].items():
        if k == "detail":
            spec.setdefault("detail", v)
        else:
            spec[k] = v
    if params.get("subdivision") is not None:
        spec["_subdiv"] = max(0, min(4, int(params["subdivision"])))
    spec.setdefault("seed", int(time.time()) % 100000)
    cat = spec["category"]
    sub = spec.get("subtype")
    label = (spec.get("keyword") or sub or cat).replace("_", " ").capitalize()
    engine.progress(f"Construction : {label}…", 0.1)
    m = Model(name or label, spec)
    try:
        if cat in ("creature", "character", "animal"):
            from .organic import build_organic
            build_organic(m, spec)
        elif cat == "building":
            from .hard_surface import build_building
            build_building(m, spec)
        elif cat == "vehicle":
            from .hard_surface import build_vehicle
            build_vehicle(m, spec)
        elif cat == "weapon":
            from .hard_surface import build_weapon
            build_weapon(m, spec)
        elif cat == "prop":
            from .hard_surface import build_prop
            build_prop(m, spec)
        else:
            from .hard_surface import build_scenery
            build_scenery(m, spec)
    except CommandError:
        _delete_model(m.root)
        raise
    except Exception as e:
        _delete_model(m.root)
        raise CommandError(f"La génération a échoué : {type(e).__name__}: {e}") from e
    spec["_params"] = params
    m.finish()
    engine.progress("Mise à l'échelle et finitions…", 0.5)
    height = spec.get("height")
    if height is None:
        height = DEFAULT_HEIGHTS.get((cat, sub))
        if height and spec.get("size_hint") == "big":
            height *= 2.5
        elif height and spec.get("size_hint") == "small":
            height *= 0.5
    report = postprocess.run(engine, m, params, height)
    if location is not None:
        m.root.location = Vector(location)
    if rotation is not None:
        m.root.rotation_euler = rotation
    spec["height"] = report.get("height", height)
    set_json_prop(m.root, "se_spec", spec)
    select_only([m.root] + m.parts[:0], active=m.root)
    stats = _stats(m.root)
    try:
        from ..library import record_generation
        record_generation(engine, spec.get("prompt", ""), spec, [m.root.name])
    except Exception:
        pass
    engine.progress("Génération terminée", 1.0)
    return {"name": m.root.name, "parts": [o.name for o in m.parts], "spec": spec, "postprocess": report,
            "stats": stats, "seconds": round(time.time() - t0, 2),
            "method": "procédural (géométrie construite par algorithmes Blender — pas de réseau neuronal)",
            "_label": f"Générer {m.root.name}"}


def _stats(root) -> dict:
    objs = hierarchy(root)
    dg = bpy.context.evaluated_depsgraph_get()
    tri = 0
    for o in objs:
        if o.type == "MESH":
            em = o.evaluated_get(dg).to_mesh()
            em.calc_loop_triangles()
            tri += len(em.loop_triangles)
            o.evaluated_get(dg).to_mesh_clear()
    bb = world_bbox(objs)
    return {"objects": len(objs), "meshes": sum(1 for o in objs if o.type == "MESH"), "triangles_evaluated": tri,
            "dimensions_m": [round(x, 3) for x in (bb[1] - bb[0])] if bb else None}


def _delete_model(root):
    arm = root.get("se_armature") if root.name in bpy.data.objects else None
    for o in reversed(hierarchy(root)):
        bpy.data.objects.remove(o, do_unlink=True)
    if arm and arm in bpy.data.objects:
        bpy.data.objects.remove(bpy.data.objects[arm], do_unlink=True)


def _params_from(p: Params) -> dict:
    out = {}
    src = p.dict("params", {})
    merged = {**src, **{k: v for k, v in p.data.items() if k in default_params()}}
    d = Params(merged)
    if d.has("quality"):
        out["quality"] = d.str("quality", choices=list(QUALITY))
    for b in ("symmetry", "smoothing", "remesh", "generate_materials", "generate_uv", "auto_optimize", "fix_normals",
              "cleanup", "fill_holes", "collision", "game_ready", "apply_modifiers"):
        if d.has(b):
            out[b] = d.bool(b)
    if d.has("subdivision"):
        out["subdivision"] = d.int("subdivision", lo=0, hi=4)
    if d.has("resolution"):
        out["resolution"] = d.float("resolution", lo=0.002, hi=1.0)
    if d.has("target_polycount"):
        out["target_polycount"] = d.int("target_polycount", lo=0, hi=20_000_000)
    if d.has("texture_resolution"):
        tr = d.int("texture_resolution", lo=0, hi=4096)
        if tr not in (0, 512, 1024, 2048, 4096):
            raise CommandError("texture_resolution : 0 (aucune), 512, 1024, 2048 ou 4096.")
        out["texture_resolution"] = tr
    if d.has("texture_maps"):
        out["texture_maps"] = [str(x) for x in d.list("texture_maps")]
    if d.has("lods"):
        out["lods"] = d.int("lods", lo=0, hi=4)
    return out


def _spec_from(p: Params, category: str | None = None) -> dict:
    if p.has("prompt") and not p.has("spec"):
        spec = parse_prompt(p.str("prompt"))
    else:
        spec = dict(p.dict("spec", {}))
    if category:
        if spec.get("category") != category:
            spec["category"] = category
            if category == "character":
                spec.setdefault("subtype", "humanoid")
    for key in ("subtype", "material", "style", "build", "keyword", "body_plan", "species"):
        if p.has(key):
            spec[key] = p.str(key)
    if p.has("height"):
        spec["height"] = p.float("height", lo=0.01, hi=1000)
    if p.has("colors"):
        spec.setdefault("colors", {}).update(p.dict("colors"))
    for ck in ("color", "primary_color"):
        if p.has(ck):
            spec.setdefault("colors", {})["primary"] = p.raw(ck)
    if p.has("secondary_color"):
        spec.setdefault("colors", {})["secondary"] = p.raw("secondary_color")
    if p.has("eye_color"):
        spec.setdefault("colors", {})["eyes"] = p.raw("eye_color")
    if p.has("features"):
        spec.setdefault("features", {}).update(p.dict("features"))
    for fk in ("wings", "horns", "tails", "tentacles", "heads", "eyes", "legs", "arms"):
        if p.has(fk):
            spec.setdefault("features", {})[fk] = p.int(fk, lo=0, hi=64)
    for bk in ("spikes", "scales", "claws", "fangs", "fur", "feathers", "shell"):
        if p.has(bk):
            spec.setdefault("features", {})[bk] = p.bool(bk)
    for gk in ("glow", "glow_eyes", "wet"):
        if p.has(gk):
            spec[gk] = p.bool(gk)
    if p.has("seed"):
        spec["seed"] = p.int("seed", lo=0)
    if p.has("floors"):
        spec["floors"] = p.int("floors", lo=1, hi=60)
    if p.has("count"):
        spec["count"] = p.int("count", lo=1, hi=200)
    if p.has("part_scales"):
        spec.setdefault("scales", {}).update({k: float(v) for k, v in p.dict("part_scales").items()})
    spec.setdefault("category", category or "creature")
    return spec


def _gen_cmd(engine, p: Params, category: str | None):
    spec = _spec_from(p, category)
    return generate(engine, spec, _params_from(p), p.str("name", None),
                    p.vec("location") if p.has("location") else None)


@command("parse_prompt", history=False)
def cmd_parse_prompt(engine, p: Params):
    """Analyse une description (FR/EN) et renvoie la spécification de génération, sans rien créer."""
    return {"spec": parse_prompt(p.str("prompt")), "categories": CATEGORY_LABELS}


@command("generate_from_prompt", label="Générer depuis une description")
def cmd_generate(engine, p: Params):
    """Génère un modèle 3D procédural depuis une description texte (ou une `spec` structurée), avec les
    paramètres de qualité/géométrie/textures/options."""
    return _gen_cmd(engine, p, None)


@command("create_creature", label="Créer une créature")
def cmd_create_creature(engine, p: Params):
    """Crée une créature/monstre : subtype (dragon, hydra, kraken, demon, golem, griffin, blob, alien, beast),
    wings, horns, tails, tentacles, heads, eyes, legs, spikes, scales, claws, colors {primary, secondary, eyes,
    horns, wings}, glow_eyes, material, height, part_scales {wings: 1.5...}. Ou `prompt` texte."""
    return _gen_cmd(engine, p, "creature")


@command("create_character", label="Créer un personnage")
def cmd_create_character(engine, p: Params):
    """Crée un personnage humanoïde : keyword (chevalier, mage, orc, elfe, nain, robot, zombie, squelette, viking...),
    build (thin/normal/heavy), style (realistic/cartoon), colors, height, features (horns, wings, tails...)."""
    p.data.setdefault("subtype", "humanoid")
    return _gen_cmd(engine, p, "character")


@command("create_animal", label="Créer un animal")
def cmd_create_animal(engine, p: Params):
    """Crée un animal : subtype wolf, cat, horse, bear, deer, cow, rabbit, lizard, bird, fish, snake, spider."""
    return _gen_cmd(engine, p, "animal")


@command("create_building", label="Créer un bâtiment")
def cmd_create_building(engine, p: Params):
    """Crée un bâtiment : subtype house, tower, castle, temple, modern, shop ; floors ; colors ; material."""
    return _gen_cmd(engine, p, "building")


@command("create_vehicle", label="Créer un véhicule")
def cmd_create_vehicle(engine, p: Params):
    """Crée un véhicule : subtype car, truck, spaceship, boat, plane, tank, bike ; colors."""
    return _gen_cmd(engine, p, "vehicle")


@command("create_weapon", label="Créer une arme fictive")
def cmd_create_weapon(engine, p: Params):
    """Crée une arme fictive (accessoire de jeu) : subtype sword, dagger, axe, spear, hammer, bow, staff, shield,
    scythe, gun ; material ; glow."""
    return _gen_cmd(engine, p, "weapon")


@command("create_prop", label="Créer un objet")
def cmd_create_prop(engine, p: Params):
    """Crée un objet : subtype barrel, crate, chest, table, chair, lamp, bottle, rock, tree, bush, mushroom,
    crystal, well, fence, crown."""
    return _gen_cmd(engine, p, "prop")


@command("create_scenery", label="Créer un décor")
def cmd_create_scenery(engine, p: Params):
    """Crée un décor : subtype terrain, forest, ruins, dungeon, village ; terrain_size ; count."""
    return _gen_cmd(engine, p, "scenery")


@command("create_organic_model", label="Créer un modèle organique")
def cmd_create_organic(engine, p: Params):
    """Modèle organique libre défini par un squelette : `bones` [{name, head, tail, radius (ou [r_head, r_tail]),
    parent, role}] en mètres. Le corps est construit par Skin + Subdivision (surface lisse continue),
    avec matériau et squelette d'animation correspondant. Permet à Claude de construire n'importe quelle
    anatomie (créatures inédites, plantes, formes organiques)."""
    from .builder import SkinBody
    bones = p.list("bones")
    if not bones:
        raise CommandError("« bones » est vide.")
    if len(bones) > 2000:
        raise CommandError("Trop d'os (max 2000).")
    spec = {"category": "creature", "subtype": "custom", "body_plan": "custom", "colors": {}, "features": {},
            "prompt": p.str("description", "modèle organique personnalisé")}
    if p.has("color"):
        spec["colors"]["primary"] = p.raw("color")
    spec = normalize_spec(spec)
    params = _params_from(p)
    spec["_subdiv"] = QUALITY[params.get("quality", "standard")]["_subdiv"] if params.get("subdivision") is None \
        else params["subdivision"]
    m = Model(p.str("name", "Organique"), spec)
    sb = SkinBody()
    index = {}
    for b in bones:
        if not isinstance(b, dict) or not {"name", "head", "tail"} <= set(b):
            _delete_model(m.root)
            raise CommandError("Chaque os doit avoir name, head, tail (et radius).")
        r = b.get("radius", 0.1)
        r0, r1 = (r if isinstance(r, (list, tuple)) else (r, r))[:2]
        par = b.get("parent")
        if par and par in index:
            head_i = index[par]
            # si la tête de l'os ne coïncide pas avec la queue du parent, on insère un sommet
            if (Vector(sb.verts[head_i]) - Vector(b["head"])).length > 1e-4:
                hi = sb.v(b["head"], float(r0))
                sb.e(head_i, hi)
                head_i = hi
        else:
            head_i = sb.v(b["head"], float(r0))
            sb.roots.add(head_i)
        segs = int(b.get("segments", 1))
        prev = head_i
        for k in range(1, segs + 1):
            t = k / segs
            pos = Vector(b["head"]).lerp(Vector(b["tail"]), t)
            rr = float(r0) + (float(r1) - float(r0)) * t
            vi = sb.v(pos, rr)
            sb.e(prev, vi)
            prev = vi
        index[b["name"]] = prev
        m.bone(b["name"], b["head"], b["tail"], par if par in index else None, b.get("role", ""))
    body = sb.build(f"{m.root.name}_Corps", subdiv=spec["_subdiv"])
    from ..materials import new_material
    mat_preset = p.str("material", "organic")
    m.add(body, m.material("corps", mat_preset, color=p.color("color", (0.5, 0.3, 0.3, 1))), part="corps")
    if p.has("eyes"):
        from .organic import add_eyes
        for e in p.list("eyes"):
            add_eyes(m, spec, Vector(e["head_center"]), float(e.get("head_radius", 0.2)),
                     Vector(e.get("forward", (0, -1, 0))), int(e.get("count", 2)), e.get("bone", ""))
    spec["_params"] = params
    m.finish()
    report = postprocess.run(engine, m, params, None)
    select_only([m.root])
    return {"name": m.root.name, "parts": [o.name for o in m.parts], "postprocess": report, "stats": _stats(m.root),
            "_label": f"Organique {m.root.name}"}


def find_model_root(obj):
    o = obj
    while o is not None:
        if o.get("se_spec"):
            return o
        o = o.parent
    return None


@command("modify_model", label="Modifier le modèle")
def cmd_modify_model(engine, p: Params):
    """Modifie un modèle généré : `instruction` en langage naturel (« agrandis ses ailes, ajoute six tentacules,
    remplace les yeux par des yeux blancs lumineux ») et/ou `changes` structurés (features, colors, part_scales,
    height, material...). Le modèle est reconstruit à la même place avec la spec modifiée."""
    obj = get_object(p.str("name"))
    root = find_model_root(obj)
    if root is None:
        raise CommandError("Cet objet n'a pas été généré par SUPERESSENCE (pas de spécification). Utilisez les "
                           "outils de modélisation (modify_geometry, sculpt_object...).")
    spec = get_json_prop(root, "se_spec")
    changes = []
    if p.has("instruction"):
        spec, changes = parse_modification(p.str("instruction"), spec)
        if not changes:
            raise CommandError("Aucune modification reconnue dans l'instruction. Précisez (ex. « ajoute 2 cornes », "
                               "« agrandis les ailes », « yeux rouges lumineux ») ou utilisez `changes`.")
    if p.has("changes"):
        ch = p.dict("changes")
        for k in ("features", "colors", "scales"):
            if k in ch:
                spec.setdefault(k, {}).update(ch[k])
                changes.append(f"{k} : {ch[k]}")
        if "part_scales" in ch:
            spec.setdefault("scales", {}).update(ch["part_scales"])
            changes.append(f"échelles : {ch['part_scales']}")
        for k in ("height", "material", "style", "build", "glow", "glow_eyes", "wet", "subtype", "floors", "seed"):
            if k in ch:
                spec[k] = ch[k]
                changes.append(f"{k} = {ch[k]}")
    if not changes:
        raise CommandError("Indiquez « instruction » ou « changes ».")
    params = spec.pop("_params", None) or default_params()
    if p.has("params"):
        params.update(_params_from(p))
    loc, rot = root.location.copy(), root.rotation_euler.copy()
    name = root.name
    had_rig = bool(root.get("se_armature"))
    _delete_model(root)
    for k in [k for k in spec if k.startswith("_")]:
        spec.pop(k)
    result = generate(engine, spec, params, name, location=loc, rotation=rot)
    if had_rig:
        from ..rigging import cmd_create_rig
        result["rig"] = cmd_create_rig(engine, Params({"name": result["name"]}))
    result["changes"] = changes
    result["_label"] = f"Modifier {result['name']}"
    return result


@command("get_generation_options", history=False)
def cmd_options(engine, p: Params):
    """Catégories, sous-types, paramètres de génération et leurs valeurs possibles."""
    from .prompt import CATEGORIES
    return {"categories": {k: {"label": CATEGORY_LABELS[k], "subtypes": list(v)} for k, v in CATEGORIES.items()},
            "quality": list(QUALITY), "texture_resolutions": [0, 512, 1024, 2048, 4096],
            "texture_maps": ["base_color", "roughness", "metallic", "normal", "height", "ao", "emission"],
            "defaults": default_params(),
            "honesty": "Génération procédurale par algorithmes Blender. Aucun réseau neuronal n'est utilisé sauf si "
                       "un moteur IA local facultatif est configuré (voir ai_backends)."}
