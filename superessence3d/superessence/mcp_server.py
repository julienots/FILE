"""Serveur MCP local de SUPERESSENCE 3D STUDIO (transport stdio, pour Claude Desktop).

Claude Desktop lance ce processus. Chaque outil appelle l'API locale du studio
(127.0.0.1, jeton secret) : les opérations sont réellement exécutées par Blender.
Si le studio n'est pas ouvert, il est démarré automatiquement en arrière-plan.

Lancement manuel (test) : python -m superessence.mcp_server
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from typing import Any

from . import config

try:  # SDK MCP Python 2.x
    from mcp.server.mcpserver import Image, MCPServer as _Server
except ImportError:  # SDK MCP Python 1.x
    from mcp.server.fastmcp import FastMCP as _Server, Image  # type: ignore

from mcp.types import ToolAnnotations


def _ann(title: str, read_only=False, destructive=False, idempotent=False) -> ToolAnnotations:
    try:
        return ToolAnnotations(title=title, read_only_hint=read_only, destructive_hint=destructive,
                               idempotent_hint=idempotent, open_world_hint=False)
    except Exception:  # noms camelCase du SDK 1.x
        return ToolAnnotations(title=title, readOnlyHint=read_only, destructiveHint=destructive,  # type: ignore
                               idempotentHint=idempotent, openWorldHint=False)


INSTRUCTIONS = """SUPERESSENCE 3D STUDIO : studio 3D local basé sur Blender (source de vérité) avec un moteur de
génération PROCÉDURALE (pas de réseau neuronal). Unités : mètres, axe Z vers le haut, l'avant des créatures vers -Y.

Méthode recommandée :
1. get_scene_info pour voir l'état ; inspect_model / render_scene (renvoie une image) pour vérifier visuellement.
2. Pour un modèle complet : generate_from_prompt (description libre) ou les outils create_creature / create_character /
   create_animal / create_building / create_vehicle / create_weapon / create_prop / create_scenery avec des paramètres
   précis (features, colors, part_scales, height, material...). Pour une anatomie inédite : create_organic_model (squelette
   d'os avec rayons → surface organique lisse + armature).
3. Pour modifier un modèle généré : modify_model (instruction en langage naturel ou changes structurés).
4. Détails : modify_geometry (extrusion, bevel, inset...), sculpt_object, apply_modifier/add_modifier, create_material,
   create_texture (description texte), bake_textures (cartes PBR), create_uv_map.
5. Animation : create_rig puis create_animation (motions combinables : wing_flap, tail_wave, breathe, walk, fly...).
6. Finition : create_lighting, create_camera, render_scene, export_model (profils unity/unreal/godot/mobile/print3d).
Les opérations destructrices renvoient confirmation_required : demandez l'accord de l'utilisateur, puis relancez
avec confirm=true. undo annule la dernière opération."""

mcp = _Server("superessence", instructions=INSTRUCTIONS)


# ---------------------------------------------------------------------------
# Liaison avec le studio
# ---------------------------------------------------------------------------
class StudioError(Exception):
    pass


def _base() -> str:
    port = int(os.environ.get("SUPERESSENCE_PORT") or config.load_settings().get("port", config.DEFAULT_PORT))
    return f"http://127.0.0.1:{port}"


def _request(method: str, path: str, body: dict | None = None, timeout: float = 3600) -> dict:
    data = json.dumps(body or {}).encode("utf-8") if method == "POST" else None
    req = urllib.request.Request(_base() + path, data=data, method=method,
                                 headers={"Content-Type": "application/json", "X-SE-Token": config.token(),
                                          "Host": "127.0.0.1"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        try:
            return json.loads(e.read().decode("utf-8"))
        except ValueError:
            raise StudioError(f"Erreur HTTP {e.code} du studio.") from None


_started_proc = None


def ensure_studio() -> None:
    global _started_proc
    try:
        r = _request("GET", "/api/status", timeout=3)
        if r.get("ok"):
            return
    except (urllib.error.URLError, ConnectionError, OSError):
        pass
    # démarrage automatique du studio (sans ouvrir de navigateur)
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    log = open(os.path.join(config.home(), "studio_auto.log"), "a", encoding="utf-8")
    kwargs = {}
    if os.name == "nt":
        kwargs["creationflags"] = 0x00000008 | 0x00000200  # DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP
    else:
        kwargs["start_new_session"] = True
    env = dict(os.environ, SUPERESSENCE_NO_BROWSER="1")
    args = [sys.executable, "-m", "superessence", "--no-browser"] + (["--lan"] if os.environ.get("SUPERESSENCE_LAN") else [])
    _started_proc = subprocess.Popen(args, cwd=root, stdout=log,
                                     stderr=log, stdin=subprocess.DEVNULL, env=env, **kwargs)
    for _ in range(180):
        time.sleep(0.5)
        try:
            if _request("GET", "/api/status", timeout=2).get("ok"):
                return
        except (urllib.error.URLError, ConnectionError, OSError):
            if _started_proc.poll() is not None:
                break
    raise StudioError("Impossible de démarrer SUPERESSENCE 3D STUDIO. Lancez-le manuellement (start) et consultez "
                      f"{os.path.join(config.home(), 'studio_auto.log')}.")


def call(command: str, params: dict | None = None) -> dict:
    """Exécute une commande du moteur et renvoie un résultat structuré."""
    clean = {k: v for k, v in (params or {}).items() if v is not None}
    ensure_studio()
    r = _request("POST", f"/api/cmd/{command}", clean)
    if r.get("ok"):
        return r["result"]
    if r.get("confirmation_required"):
        return {"confirmation_required": True, "message": r.get("error"),
                "action": "Demandez l'accord explicite de l'utilisateur, puis relancez le même outil avec confirm=true."}
    raise StudioError(r.get("error") or "Erreur inconnue du studio.")


# ---------------------------------------------------------------------------
# Scène et inspection
# ---------------------------------------------------------------------------
@mcp.tool(annotations=_ann("Informations de la scène", read_only=True))
def get_scene_info() -> dict:
    """Résumé de la scène : objets (type, transformations, polygones, matériaux, modificateurs), caméra, rendu,
    collections, totaux, état d'annulation."""
    return call("get_scene_info")


@mcp.tool(annotations=_ann("Lister les objets", read_only=True))
def list_objects(type: str | None = None) -> dict:
    """Liste les objets de la scène. type facultatif : MESH, EMPTY, LIGHT, CAMERA, ARMATURE."""
    return call("list_objects", {"type": type})


@mcp.tool(annotations=_ann("Détails d'un objet", read_only=True))
def get_object_details(name: str) -> dict:
    """Détails d'un objet : transformations, maillage, UV, groupes, matériaux, modificateurs, spécification de génération."""
    return call("get_object_details", {"name": name})


@mcp.tool(annotations=_ann("Inspecter un modèle", read_only=True))
def inspect_model(name: str, evaluated: bool = True) -> dict:
    """Diagnostic : polygones, quads/tris/n-gones, non-manifold, trous, doublons, normales inversées, îlots, volume,
    UV, échelle. Renvoie la liste des problèmes à corriger."""
    return call("inspect_model", {"name": name, "evaluated": evaluated})


# ---------------------------------------------------------------------------
# Création de géométrie
# ---------------------------------------------------------------------------
@mcp.tool(annotations=_ann("Créer un maillage"))
def create_mesh(primitive: str | None = "cube", name: str | None = None, size: float = 2.0,
                location: list[float] | None = None, rotation: list[float] | None = None,
                scale: list[float] | None = None, vertices: list[list[float]] | None = None,
                faces: list[list[int]] | None = None, color: str | None = None, smooth: bool = False) -> dict:
    """Crée un maillage. primitive : cube, plane, sphere, icosphere, cylinder, cone, torus, monkey, grid, circle.
    Ou géométrie libre : vertices [[x,y,z],...] et faces [[i,j,k,...],...] (mètres). color : '#rrggbb' ou nom."""
    params: dict[str, Any] = {"name": name, "size": size, "location": location, "rotation": rotation, "scale": scale,
                              "color": color, "smooth": smooth}
    if vertices:
        params.update(vertices=vertices, faces=faces or [])
    else:
        params["primitive"] = primitive
    return call("create_mesh", params)


@mcp.tool(annotations=_ann("Générer depuis une description"))
def generate_from_prompt(prompt: str, quality: str = "standard", height: float | None = None,
                         texture_resolution: int = 0, target_polycount: int = 0, game_ready: bool = False,
                         collision: bool = False, name: str | None = None,
                         location: list[float] | None = None) -> dict:
    """Génère un modèle 3D procédural complet depuis une description (FR/EN) : personnages, monstres, animaux,
    bâtiments, objets, décors, véhicules, armes fictives. quality : draft|standard|high|ultra. texture_resolution :
    0|512|1024|2048|4096 (cuisson PBR). Renvoie la spec interprétée (vérifiez-la) et les statistiques."""
    spec = call("parse_prompt", {"prompt": prompt})["spec"]
    if height:
        spec["height"] = height
    return call("generate_from_prompt", {"spec": spec, "name": name, "location": location, "params": {
        "quality": quality, "texture_resolution": texture_resolution, "target_polycount": target_polycount,
        "game_ready": game_ready, "collision": collision}})


def _gen(command: str, args: dict) -> dict:
    params = {k: v for k, v in args.items() if v is not None and k not in ("quality", "texture_resolution")}
    params["params"] = {"quality": args.get("quality") or "standard",
                        "texture_resolution": args.get("texture_resolution") or 0}
    return call(command, params)


@mcp.tool(annotations=_ann("Créer une créature / un monstre"))
def create_creature(subtype: str = "dragon", prompt: str | None = None, height: float | None = None,
                    wings: int | None = None, horns: int | None = None, tails: int | None = None,
                    tentacles: int | None = None, heads: int | None = None, eyes: int | None = None,
                    legs: int | None = None, spikes: bool | None = None, scales: bool | None = None,
                    claws: bool | None = None, fangs: bool | None = None, colors: dict | None = None,
                    glow_eyes: bool | None = None, glow: bool | None = None, material: str | None = None,
                    part_scales: dict | None = None, build: str | None = None, name: str | None = None,
                    location: list[float] | None = None, quality: str = "standard",
                    texture_resolution: int = 0) -> dict:
    """Crée une créature. subtype : dragon, hydra, kraken, demon, golem, griffin, blob, alien, beast.
    colors : {"primary": "noir", "secondary": "#aa0000", "eyes": "rouge", "horns": "ivoire", "wings": "violet"}.
    part_scales : {"wings": 1.5, "horns": 1.2, "tails": 1.3, "body": 1.1, "legs": 0.9, "neck": 1.2, "heads": 1,
    "eyes": 1.5, "tentacles": 1, "spikes": 1.3}. material : scales, skin, organic, fur, metal, stone, lava...
    Le modèle garde son squelette (create_rig) et sa spec (modify_model)."""
    return _gen("create_creature", locals())


@mcp.tool(annotations=_ann("Créer un personnage"))
def create_character(keyword: str = "personnage", prompt: str | None = None, height: float | None = None,
                     build: str | None = None, style: str | None = None, colors: dict | None = None,
                     horns: int | None = None, wings: int | None = None, tails: int | None = None,
                     arms: int | None = None, eyes: int | None = None, material: str | None = None,
                     glow_eyes: bool | None = None, part_scales: dict | None = None, name: str | None = None,
                     location: list[float] | None = None, quality: str = "standard",
                     texture_resolution: int = 0) -> dict:
    """Crée un personnage humanoïde. keyword : chevalier, guerrier, mage, sorcier, orc, troll, ogre, gobelin, elfe, nain,
    zombie, squelette, viking, robot, demon, golem, alien, enfant... build : thin|normal|heavy. style :
    realistic|cartoon. colors : {"primary": peau/corps, "secondary": vêtements/cheveux, "eyes": ...}."""
    return _gen("create_character", locals())


@mcp.tool(annotations=_ann("Créer un animal"))
def create_animal(subtype: str = "wolf", keyword: str | None = None, height: float | None = None,
                  colors: dict | None = None, eyes: int | None = None, legs: int | None = None,
                  material: str | None = None, part_scales: dict | None = None, name: str | None = None,
                  location: list[float] | None = None, quality: str = "standard", texture_resolution: int = 0) -> dict:
    """Crée un animal. subtype : wolf, cat, horse, bear, deer, cow, rabbit, lizard, bird, fish, snake, spider.
    keyword affine l'espèce (ex. requin, aigle, tortue, scorpion, lion)."""
    return _gen("create_animal", locals())


@mcp.tool(annotations=_ann("Créer un bâtiment"))
def create_building(subtype: str = "house", floors: int | None = None, colors: dict | None = None,
                    material: str | None = None, height: float | None = None, name: str | None = None,
                    location: list[float] | None = None, quality: str = "standard", texture_resolution: int = 0) -> dict:
    """Crée un bâtiment avec de vraies ouvertures : subtype house, tower, castle, temple, modern, shop. material :
    brick, stone, wood, plain."""
    return _gen("create_building", locals())


@mcp.tool(annotations=_ann("Créer un véhicule"))
def create_vehicle(subtype: str = "car", colors: dict | None = None, material: str | None = None,
                   height: float | None = None, name: str | None = None, location: list[float] | None = None,
                   quality: str = "standard", texture_resolution: int = 0) -> dict:
    """Crée un véhicule : car, truck, spaceship, boat, plane, tank, bike."""
    return _gen("create_vehicle", locals())


@mcp.tool(annotations=_ann("Créer une arme fictive"))
def create_weapon(subtype: str = "sword", colors: dict | None = None, material: str | None = None,
                  glow: bool | None = None, height: float | None = None, name: str | None = None,
                  location: list[float] | None = None, quality: str = "standard", texture_resolution: int = 0) -> dict:
    """Crée une arme fictive (accessoire de jeu) : sword, dagger, axe, spear, hammer, bow, staff, shield, scythe, gun."""
    return _gen("create_weapon", locals())


@mcp.tool(annotations=_ann("Créer un objet"))
def create_prop(subtype: str = "barrel", keyword: str | None = None, colors: dict | None = None,
                glow: bool | None = None, height: float | None = None, name: str | None = None,
                location: list[float] | None = None, quality: str = "standard", texture_resolution: int = 0) -> dict:
    """Crée un objet / ressource de jeu : barrel, crate, chest, table, chair, lamp, bottle, rock, tree, bush, mushroom,
    crystal, well, fence, crown. keyword affine (ex. sapin, vase, potion)."""
    return _gen("create_prop", locals())


@mcp.tool(annotations=_ann("Créer un décor"))
def create_scenery(subtype: str = "terrain", keyword: str | None = None, terrain_size: float | None = None,
                   count: int | None = None, colors: dict | None = None, name: str | None = None,
                   quality: str = "standard") -> dict:
    """Crée un décor : terrain (keyword montagne, desert...), forest, ruins, dungeon, village."""
    return _gen("create_scenery", locals())


@mcp.tool(annotations=_ann("Créer un modèle organique libre"))
def create_organic_model(bones: list[dict], name: str = "Organique", color: str | None = None,
                         material: str = "organic", eyes: list[dict] | None = None, quality: str = "standard",
                         description: str | None = None) -> dict:
    """Construit une forme organique lisse à partir d'un squelette, pour toute anatomie inédite.
    bones : [{"name": "corps", "head": [0,0,1], "tail": [0,-1,1.2], "radius": [0.4, 0.3], "parent": null,
    "role": "spine", "segments": 2}, {"name": "cou", "head": [0,-1,1.2], "tail": [0,-1.5,1.8], "radius": [0.25, 0.15],
    "parent": "corps", "role": "neck"}, ...]. Coordonnées en mètres (Z haut, avant -Y). Rôles utiles pour l'animation :
    root, spine, chest, neck, head, tail_1.., leg_L_front_upper, wing_L_1, tentacle_1_1...
    eyes : [{"head_center": [x,y,z], "head_radius": 0.2, "count": 2, "forward": [0,-1,0], "bone": "tête"}]."""
    return call("create_organic_model", {"bones": bones, "name": name, "color": color, "material": material,
                                         "eyes": eyes, "params": {"quality": quality}, "description": description})


@mcp.tool(annotations=_ann("Modifier un modèle généré"))
def modify_model(name: str, instruction: str | None = None, changes: dict | None = None) -> dict:
    """Modifie un modèle généré et le reconstruit sur place. instruction : « agrandis ses ailes, ajoute six tentacules
    et remplace les yeux par des yeux blancs lumineux ». changes : {"features": {"tentacles": 6}, "colors": {"eyes":
    "blanc"}, "part_scales": {"wings": 1.5}, "glow_eyes": true, "height": 3}. Le squelette est recréé s'il existait."""
    return call("modify_model", {"name": name, "instruction": instruction, "changes": changes})


@mcp.tool(annotations=_ann("Image(s) vers 3D"))
def image_to_model(image: str | None = None, images: list[str] | None = None, height: float = 1.0,
                   depth: float = 0.5, resolution: int = 96, backend: str | None = None,
                   name: str | None = None) -> dict:
    """Crée un modèle depuis une image (chemin local) par méthode GÉOMÉTRIQUE : silhouette gonflée et texturée. Avec
    images [face, profil, dessus] : enveloppe visuelle. Ce n'est pas une IA neuronale ; backend = moteur neuronal local
    facultatif s'il est configuré (list via execute_command list_ai_backends)."""
    return call("image_to_model", {"image": image, "images": images, "height": height, "depth": depth,
                                   "resolution": resolution, "backend": backend, "name": name})


# ---------------------------------------------------------------------------
# Édition
# ---------------------------------------------------------------------------
@mcp.tool(annotations=_ann("Modifier la géométrie"))
def modify_geometry(name: str, operation: str, faces: list[int] | None = None, vertices: list[int] | None = None,
                    edges: list[int] | None = None, all: bool | None = None, distance: float | None = None,
                    thickness: float | None = None, depth: float | None = None, width: float | None = None,
                    segments: int | None = None, cuts: int | None = None, smooth: float | None = None,
                    offset: list[float] | None = None, factor: float | None = None, iterations: int | None = None,
                    axis: str | None = None, angle: float | None = None, individual: bool | None = None,
                    direction: list[float] | None = None) -> dict:
    """Édition polygonale (indices Blender). operation : extrude (faces, distance, individual), inset (thickness, depth),
    bevel (edges|faces, width, segments), subdivide (cuts, smooth), delete, translate (offset), scale (factor), push
    (distance), smooth (factor, iterations), merge, merge_by_distance, triangulate, fill_holes, recalc_normals,
    flip_normals, solidify (thickness), mirror (axis), spin, bridge, randomize, loop_cut (edge). all=true pour tout le
    maillage. Astuce : execute_command select_by_normal {name, direction:[0,0,1]} pour obtenir les faces du dessus."""
    args = {k: v for k, v in locals().items() if v is not None}
    return call("modify_geometry", args)


@mcp.tool(annotations=_ann("Sculpter"))
def sculpt_object(name: str, points: list[list[float]], brush: str = "draw", radius: float = 0.2,
                  strength: float = 0.5, invert: bool = False, symmetry_x: bool = False, iterations: int = 1,
                  grab_offset: list[float] | None = None) -> dict:
    """Sculpture réelle des sommets. points : [[x,y,z, nx,ny,nz], ...] en coordonnées monde (normale facultative).
    brush : draw, clay, inflate, smooth, flatten, scrape, pinch, crease, noise, grab (avec grab_offset). Pour plus de
    détail, densifier d'abord : execute_command remesh_for_sculpt {name, voxel_size}."""
    return call("sculpt_object", {k: v for k, v in locals().items() if v is not None})


@mcp.tool(annotations=_ann("Transformer des objets"))
def transform_object(name: str, location: list[float] | None = None, rotation_degrees: list[float] | None = None,
                     scale: list[float] | None = None, relative: bool = False) -> dict:
    """Déplace / tourne (degrés) / met à l'échelle un objet. relative=true pour un décalage."""
    return call("transform_object", {"name": name, "location": location, "rotation": rotation_degrees,
                                     "degrees": True, "scale": scale, "relative": relative})


@mcp.tool(annotations=_ann("Ajouter un modificateur"))
def add_modifier(name: str, type: str, settings: dict | None = None, apply: bool = False) -> dict:
    """Ajoute un modificateur Blender : subdivision, boolean, mirror, array, bevel, solidify, decimate, remesh, displace,
    smooth, wireframe, screw, simple_deform, cast, wave, shrinkwrap... settings = propriétés Blender, ex. BOOLEAN
    {"operation": "DIFFERENCE", "object": "Cube"}, ARRAY {"count": 5}, MIRROR {"use_axis": [true,false,false]}."""
    return call("add_modifier", {"name": name, "type": type, "settings": settings, "apply": apply})


@mcp.tool(annotations=_ann("Appliquer un modificateur"))
def apply_modifier(name: str, modifier: str | None = None, type: str | None = None, settings: dict | None = None,
                   all: bool = False) -> dict:
    """Applique un modificateur existant (modifier), tous (all=true), ou ajoute et applique directement un `type`
    (ex. type="subdivision", settings={"levels": 2})."""
    return call("apply_modifier", {"name": name, "modifier": modifier, "type": type, "settings": settings,
                                   "all": all or None})


@mcp.tool(annotations=_ann("Optimiser un maillage"))
def optimize_mesh(name: str, target_triangles: int = 0, fill_holes: bool = False, apply_modifiers: bool = False) -> dict:
    """Nettoie (doublons, sommets isolés, faces dégénérées, normales), ferme les trous (option) et décime vers un nombre
    de triangles cible."""
    return call("optimize_mesh", {"name": name, "target_triangles": target_triangles, "fill_holes": fill_holes,
                                  "apply_modifiers": apply_modifiers})


@mcp.tool(annotations=_ann("Supprimer des objets", destructive=True))
def delete_object(name: str | None = None, names: list[str] | None = None, children: bool = True,
                  confirm: bool = False) -> dict:
    """Supprime des objets (et leurs enfants). Exige confirm=true après accord de l'utilisateur."""
    return call("delete_object", {"name": name, "names": names, "children": children, "confirm": confirm})


@mcp.tool(annotations=_ann("Dupliquer des objets"))
def duplicate_object(name: str, count: int = 1, offset: list[float] | None = None, linked: bool = False) -> dict:
    """Duplique un objet (avec ses enfants), éventuellement plusieurs fois avec un décalage [x,y,z]."""
    return call("duplicate_object", {"name": name, "count": count, "offset": offset or [1.5, 0, 0], "linked": linked})


@mcp.tool(annotations=_ann("Parenter"))
def set_parent(name: str, parent: str = "") -> dict:
    """Définit le parent d'un objet (vide pour détacher)."""
    return call("set_parent", {"name": name, "parent": parent})


# ---------------------------------------------------------------------------
# Matériaux, textures, UV
# ---------------------------------------------------------------------------
@mcp.tool(annotations=_ann("Créer un matériau"))
def create_material(objects: list[str] | None = None, preset: str | None = None, description: str | None = None,
                    name: str | None = None, color: str | None = None, color2: str | None = None,
                    roughness: float | None = None, metallic: float | None = None, bump: float | None = None,
                    emission: str | None = None, emission_strength: float | None = None, wet: float | None = None,
                    veins: str | None = None, scale: float | None = None) -> dict:
    """Crée un matériau PBR procédural (vrais nœuds Blender) et l'affecte aux objets. preset : plain, metal, gold, rust,
    wood, bark, stone, brick, skin, scales, fabric, leather, organic, plastic, glass, ice, crystal, lava, emission,
    ground, fur, eye. Ou description : « métal rouillé sombre »."""
    return call("create_material", {k: v for k, v in locals().items() if v is not None})


@mcp.tool(annotations=_ann("Régler un matériau"))
def set_material(material: str, base_color: str | None = None, metallic: float | None = None,
                 roughness: float | None = None, emission: str | None = None, emission_strength: float | None = None,
                 alpha: float | None = None, transmission: float | None = None, subsurface: float | None = None,
                 node: str | None = None, input: str | None = None, value: Any = None) -> dict:
    """Modifie un matériau (valeurs Principled) ou une entrée de nœud précise (node, input, value)."""
    return call("set_material", {k: v for k, v in locals().items() if v is not None})


@mcp.tool(annotations=_ann("Créer une texture par description"))
def create_texture(description: str, objects: list[str] | None = None, bake_resolution: int = 0,
                   maps: list[str] | None = None, name: str | None = None) -> dict:
    """Texture procédurale décrite par texte (« peau de monstre noire, humide, avec des veines violettes et des reliefs
    organiques ») appliquée aux objets. bake_resolution 512..4096 cuit aussi les cartes PBR en PNG."""
    return call("create_texture", {"description": description, "objects": objects, "bake_resolution": bake_resolution,
                                   "maps": maps, "name": name})


@mcp.tool(annotations=_ann("Cuire les cartes PBR"))
def bake_textures(name: str, resolution: int = 1024, maps: list[str] | None = None, apply: bool = True) -> dict:
    """Cuit base_color, roughness, metallic, normal, height, ao, emission en PNG (Cycles) et remplace le matériau par
    une version à textures images exportable."""
    return call("bake_textures", {"name": name, "resolution": resolution, "maps": maps, "apply": apply})


@mcp.tool(annotations=_ann("Créer les UV"))
def create_uv_map(name: str, method: str = "smart") -> dict:
    """Dépliage UV : smart, unwrap, cube, cylinder, sphere, lightmap."""
    return call("create_uv_map", {"name": name, "method": method})


# ---------------------------------------------------------------------------
# Rigging et animation
# ---------------------------------------------------------------------------
@mcp.tool(annotations=_ann("Créer un squelette"))
def create_rig(name: str, template: str = "auto", bones: list[dict] | None = None) -> dict:
    """Crée l'armature et lie le modèle (poids automatiques). template : auto (structure exacte du modèle généré),
    humanoid, quadruped, winged_quadruped, serpent, chain ; ou bones personnalisés."""
    return call("create_rig", {"name": name, "template": template, "bones": bones})


@mcp.tool(annotations=_ann("Créer une animation"))
def create_animation(name: str, motions: list[str] | None = None, object_motion: str | None = None,
                     amplitude: float = 1.0, frames: int | None = None, cycles: int = 2, speed: float = 1.0,
                     keyframes: list[dict] | None = None, replace: bool = True) -> dict:
    """Animation en images clés Blender. motions combinables : walk, run, jump, attack, fly, wing_flap, tail_wave,
    breathe, idle, head_look, tentacle_wave, swim, slither, roar (ex. ["wing_flap","tail_wave","breathe"] pour « fais battre
    les ailes, bouger la queue et respirer le torse »). object_motion sans squelette : spin, bounce, float, pulse, sway.
    keyframes personnalisées : [{"frame": 1, "bone": "tête", "rotation": [0,0,20]}, ...]."""
    return call("create_animation", {k: v for k, v in locals().items() if v is not None})


# ---------------------------------------------------------------------------
# Caméra, lumière, rendu
# ---------------------------------------------------------------------------
@mcp.tool(annotations=_ann("Créer une caméra"))
def create_camera(name: str | None = None, lens: float = 50.0, location: list[float] | None = None,
                  target: list[float] | None = None, frame_objects: bool = True, active: bool = True) -> dict:
    """Ajoute une caméra (cadrage automatique de la scène par défaut)."""
    return call("create_camera", {"name": name, "lens": lens, "location": location, "target": target,
                                  "frame_objects": frame_objects, "active": active})


@mcp.tool(annotations=_ann("Créer l'éclairage"))
def create_lighting(preset: str = "studio", replace: bool = True) -> dict:
    """Éclairage complet adapté à la taille de la scène : studio, outdoor, dramatic, night, product."""
    return call("create_lighting", {"preset": preset, "replace": replace})


@mcp.tool(annotations=_ann("Rendu de la scène", read_only=True))
def render_scene(engine: str = "cycles", samples: int = 32, width: int = 960, height: int = 540,
                 transparent: bool = False, frame_scene: bool = False):
    """Rendu réel par Blender (Cycles par défaut) et renvoie l'IMAGE pour vérification visuelle. frame_scene=true
    recadre la caméra active sur tous les objets avant le rendu."""
    if frame_scene:
        call("set_camera", {"frame_objects": True})
    r = call("render_scene", {"engine": engine, "samples": samples, "resolution": [width, height],
                              "transparent": transparent})
    return [Image(path=r["path"]), json.dumps({k: v for k, v in r.items()}, ensure_ascii=False)]


# ---------------------------------------------------------------------------
# Fichiers et projets
# ---------------------------------------------------------------------------
@mcp.tool(annotations=_ann("Exporter"))
def export_model(format: str = "glb", profile: str | None = None, name: str | None = None,
                 filename: str | None = None, folder: str | None = None, apply_modifiers: bool = True,
                 animations: bool = True, target_triangles: int | None = None) -> dict:
    """Exporte et VÉRIFIE le fichier : glb, gltf, obj, fbx, stl, ply, blend ; ou profil unity, unreal, godot, mobile,
    print3d, high_quality. name = un modèle (avec enfants et squelette) ; sinon toute la scène visible."""
    return call("export_model", {k: v for k, v in locals().items() if v is not None})


@mcp.tool(annotations=_ann("Importer"))
def import_model(path: str, normalize: bool = False, height: float | None = None) -> dict:
    """Importe un fichier local glb/gltf/obj/fbx/stl/ply/blend. normalize=true met à l'échelle (height en m)."""
    return call("import_model", {"path": path, "normalize": normalize, "height": height})


@mcp.tool(annotations=_ann("Enregistrer le projet"))
def save_project(name: str | None = None, path: str | None = None) -> dict:
    """Enregistre le projet en .blend (ouvrable dans Blender). name = dans le dossier projets ; sinon fichier courant."""
    return call("save_project", {"name": name, "path": path})


@mcp.tool(annotations=_ann("Ouvrir un projet", destructive=True))
def load_project(name: str | None = None, path: str | None = None, confirm: bool = False) -> dict:
    """Ouvre un projet .blend (remplace la scène). Si des modifications ne sont pas enregistrées, exige confirm=true."""
    return call("load_project", {"name": name, "path": path, "confirm": confirm})


@mcp.tool(annotations=_ann("Annuler"))
def undo() -> dict:
    """Annule la dernière opération (historique par instantanés)."""
    return call("undo")


@mcp.tool(annotations=_ann("Rétablir"))
def redo() -> dict:
    """Rétablit l'opération annulée."""
    return call("redo")


@mcp.tool(annotations=_ann("Enregistrer dans la bibliothèque"))
def library_save(name: str, category: str | None = None, tags: list[str] | None = None,
                 model_id: int | None = None, note: str | None = None) -> dict:
    """Enregistre un modèle dans la bibliothèque (nouvelle entrée, ou nouvelle version si model_id)."""
    return call("library_save", {"name": name, "category": category, "tags": tags, "model_id": model_id, "note": note})


@mcp.tool(annotations=_ann("Exécuter une commande du moteur"))
def execute_command(command: str, params: dict | None = None) -> dict:
    """Accès direct à toute commande du moteur (plus de 90). Liste : command="list_commands". Exemples :
    select_by_normal, select_linked_faces, boolean, shade, set_light, set_world, set_frame, pose_bone, vertex_paint,
    weight_paint, library_list, library_load, list_ai_backends, scene_manage, move_to_collection, run_python (si activé)."""
    if command == "list_commands":
        ensure_studio()
        return _request("GET", "/api/commands")
    return call(command, params or {})


@mcp.tool(annotations=_ann("Exécuter du Python Blender"))
def run_python(code: str) -> dict:
    """Exécute du code Python bpy dans le studio (variable `result` renvoyée). Désactivé par défaut : l'utilisateur doit
    activer « Autoriser le code Python » dans Paramètres › Préférences."""
    return call("run_python", {"code": code})


# ---------------------------------------------------------------------------
# Ressources
# ---------------------------------------------------------------------------
@mcp.resource("superessence://project", name="Projet", mime_type="application/json")
def res_project() -> str:
    """Projet courant, dossiers de travail et réglages."""
    return json.dumps(call("get_project_info"), ensure_ascii=False, indent=1)


@mcp.resource("superessence://scene", name="Scène", mime_type="application/json")
def res_scene() -> str:
    """Scène actuelle complète."""
    return json.dumps(call("get_scene_info"), ensure_ascii=False, indent=1)


@mcp.resource("superessence://objects", name="Objets", mime_type="application/json")
def res_objects() -> str:
    """Liste des objets."""
    return json.dumps(call("list_objects"), ensure_ascii=False, indent=1)


@mcp.resource("superessence://object/{name}", name="Objet", mime_type="application/json")
def res_object(name: str) -> str:
    """Détails d'un objet."""
    return json.dumps(call("get_object_details", {"name": name}), ensure_ascii=False, indent=1)


@mcp.resource("superessence://files/{kind}", name="Fichiers", mime_type="application/json")
def res_files(kind: str) -> str:
    """Fichiers du dossier de travail : exports, renders, projects, imports, textures."""
    return json.dumps(call("list_files", {"kind": kind}), ensure_ascii=False, indent=1)


@mcp.resource("superessence://library", name="Bibliothèque", mime_type="application/json")
def res_library() -> str:
    """Modèles de la bibliothèque."""
    return json.dumps(call("library_list"), ensure_ascii=False, indent=1)


@mcp.resource("superessence://generation-options", name="Options de génération", mime_type="application/json")
def res_gen_options() -> str:
    """Catégories, sous-types et paramètres de génération disponibles."""
    return json.dumps(call("get_generation_options"), ensure_ascii=False, indent=1)


@mcp.prompt(name="creer_creature")
def prompt_creature(description: str) -> str:
    """Guide pour créer une créature détaillée avec SUPERESSENCE."""
    return (f"Crée dans SUPERESSENCE 3D STUDIO : {description}\n"
            "1) Utilise create_creature (ou generate_from_prompt) avec des paramètres précis. 2) Vérifie avec "
            "render_scene(frame_scene=true). 3) Corrige avec modify_model ou les outils d'édition. 4) Crée le squelette "
            "(create_rig) et une animation adaptée. 5) Propose l'export.")


def main():
    mcp.run("stdio")


if __name__ == "__main__":
    main()
