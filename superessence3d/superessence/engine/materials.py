"""Matériaux et textures : Principled BSDF, préréglages procéduraux, matériaux décrits par texte,
édition des nœuds, et cuisson (bake) des cartes PBR en images.
"""
from __future__ import annotations

import os
import time

import bpy

from .colors import linear_to_srgb, parse_color, to_hex, to_linear
from .core import CommandError, Params, command
from .util import get_mesh_object, get_object, override, select_only, target_objects, rna_properties, rna_set

PRESETS = ["plain", "metal", "gold", "rust", "wood", "bark", "stone", "brick", "skin", "scales", "fabric",
           "leather", "organic", "plastic", "glass", "ice", "crystal", "lava", "emission", "ground", "fur", "eye"]

PRESET_LABELS = {
    "plain": "Uni", "metal": "Métal", "gold": "Or", "rust": "Rouille", "wood": "Bois", "bark": "Écorce",
    "stone": "Pierre", "brick": "Briques", "skin": "Peau", "scales": "Écailles", "fabric": "Tissu",
    "leather": "Cuir", "organic": "Organique", "plastic": "Plastique", "glass": "Verre", "ice": "Glace",
    "crystal": "Cristal", "lava": "Lave", "emission": "Lumineux", "ground": "Sol / herbe", "fur": "Fourrure",
    "eye": "Œil",
}


class NodeBuilder:
    def __init__(self, mat):
        self.mat = mat
        if mat.node_tree is None:
            if mat.node_tree is None:
                mat.use_nodes = True
        self.nt = mat.node_tree
        self.nt.nodes.clear()
        self.x = -1400
        self.out = self.new("ShaderNodeOutputMaterial", 400, 0)
        self.bsdf = self.new("ShaderNodeBsdfPrincipled", 100, 0)
        self.link(self.bsdf.outputs[0], self.out.inputs["Surface"])
        self._coords = None

    def new(self, kind, x=None, y=0, **props):
        n = self.nt.nodes.new(kind)
        if x is None:
            self.x += 220
            x = self.x
        n.location = (x, y)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.nt.links.new(a, b)

    def coords(self, scale=1.0):
        """Coordonnées objet (aucune UV nécessaire) avec mise à l'échelle."""
        if self._coords is None:
            tc = self.new("ShaderNodeTexCoord", -1500, 300)
            mp = self.new("ShaderNodeMapping", -1300, 300)
            self.link(tc.outputs["Object"], mp.inputs["Vector"])
            mp.inputs["Scale"].default_value = (scale, scale, scale)
            self._coords = mp
        return self._coords.outputs["Vector"]

    def ramp(self, fac_socket, c1, c2, p1=0.0, p2=1.0, y=0):
        r = self.new("ShaderNodeValToRGB", y=y)
        r.color_ramp.elements[0].position = p1
        r.color_ramp.elements[0].color = c1
        r.color_ramp.elements[1].position = p2
        r.color_ramp.elements[1].color = c2
        self.link(fac_socket, r.inputs["Fac"])
        return r

    def mix_color(self, a, b, fac, blend="MIX", y=0):
        m = self.new("ShaderNodeMix", y=y)
        m.data_type = "RGBA"
        m.blend_type = blend
        if isinstance(fac, float):
            m.inputs[0].default_value = fac
        else:
            self.link(fac, m.inputs[0])
        for sock, val in ((m.inputs[6], a), (m.inputs[7], b)):
            if isinstance(val, tuple):
                sock.default_value = val
            else:
                self.link(val, sock)
        return m.outputs[2]

    def height(self, socket, strength=0.3, distance=0.05):
        """Relief : nœud SE_Height (cuisable en carte de hauteur) + Bump vers la normale."""
        h = self.new("ShaderNodeMath", y=-400)
        h.name = h.label = "SE_Height"
        h.operation = "MULTIPLY"
        h.inputs[1].default_value = 1.0
        self.link(socket, h.inputs[0])
        b = self.new("ShaderNodeBump", -100, -300)
        b.inputs["Strength"].default_value = strength
        b.inputs["Distance"].default_value = distance
        self.link(h.outputs[0], b.inputs["Height"])
        self.link(b.outputs["Normal"], self.bsdf.inputs["Normal"])
        return h

    def set(self, name, value):
        self.bsdf.inputs[name].default_value = value


def build_preset(mat, preset: str, color=(0.8, 0.8, 0.8, 1), color2=None, roughness=None, metallic=None,
                 scale=1.0, bump=1.0, emission=None, emission_strength=0.0, wet=0.0, veins=None,
                 subsurface=None) -> None:
    """Construit un vrai arbre de nœuds procédural Blender. Couleurs en sRGB 0..1."""
    if preset not in PRESETS:
        raise CommandError(f"Préréglage inconnu : « {preset} ». Disponibles : {', '.join(PRESETS)}")
    c1 = to_linear(color)
    if color2 is None:
        color2 = tuple(max(0.0, x * 0.45) for x in color[:3]) + (1.0,)
    c2 = to_linear(color2)
    nb = NodeBuilder(mat)
    s = max(0.01, scale)
    rough = roughness
    metal = metallic
    preview_color = list(color[:3])

    if preset == "plain" or preset == "plastic":
        nb.set("Base Color", c1)
        rough = 0.35 if rough is None and preset == "plastic" else (0.5 if rough is None else rough)
        if preset == "plastic":
            nb.set("Coat Weight", 0.3)
        n = nb.new("ShaderNodeTexNoise", y=-400)
        n.inputs["Scale"].default_value = 40 * s
        nb.link(nb.coords(), n.inputs["Vector"])
        nb.height(n.outputs["Fac"], 0.05 * bump, 0.01)

    elif preset in ("metal", "gold", "rust"):
        metal = 1.0 if metal is None else metal
        n = nb.new("ShaderNodeTexNoise", y=200)
        n.inputs["Scale"].default_value = 8 * s
        n.inputs["Detail"].default_value = 10
        nb.link(nb.coords(), n.inputs["Vector"])
        rr = nb.new("ShaderNodeMapRange", y=0)
        nb.link(n.outputs["Fac"], rr.inputs["Value"])
        base_r = 0.25 if rough is None else rough
        rr.inputs["To Min"].default_value = max(0, base_r - 0.12)
        rr.inputs["To Max"].default_value = min(1, base_r + 0.15)
        nb.link(rr.outputs["Result"], nb.bsdf.inputs["Roughness"])
        if preset == "rust":
            rust_c = to_linear(color2 if color2 else (0.45, 0.18, 0.06, 1))
            n2 = nb.new("ShaderNodeTexNoise", y=500)
            n2.inputs["Scale"].default_value = 3 * s
            n2.inputs["Detail"].default_value = 12
            nb.link(nb.coords(), n2.inputs["Vector"])
            mask = nb.ramp(n2.outputs["Fac"], (0, 0, 0, 1), (1, 1, 1, 1), 0.45, 0.6, y=500)
            nb.link(nb.mix_color(c1, to_linear((0.42, 0.16, 0.05, 1)), mask.outputs["Color"]), nb.bsdf.inputs["Base Color"])
            inv = nb.new("ShaderNodeMath", y=600, operation="SUBTRACT")
            inv.inputs[0].default_value = 1.0
            nb.link(mask.outputs["Color"], inv.inputs[1])
            nb.link(inv.outputs[0], nb.bsdf.inputs["Metallic"])
            nb.height(n2.outputs["Fac"], 0.3 * bump, 0.02)
            metal = None
        else:
            nb.set("Base Color", c1)
            sc = nb.new("ShaderNodeTexNoise", y=-400)
            sc.inputs["Scale"].default_value = 60 * s
            nb.link(nb.coords(), sc.inputs["Vector"])
            nb.height(sc.outputs["Fac"], 0.04 * bump, 0.005)
        rough = None

    elif preset in ("wood", "bark"):
        w = nb.new("ShaderNodeTexWave", y=200)
        w.wave_type = "RINGS" if preset == "wood" else "BANDS"
        w.inputs["Scale"].default_value = (14 if preset == "wood" else 6) * s
        w.inputs["Distortion"].default_value = 3 if preset == "wood" else 14
        w.inputs["Detail"].default_value = 4
        # fil du bois : anneaux étirés le long de Z (veinage longitudinal)
        stretch = nb.new("ShaderNodeVectorMath", y=300)
        stretch.operation = "MULTIPLY"
        stretch.inputs[1].default_value = (1.0, 1.0, 0.12 if preset == "wood" else 1.0)
        nb.link(nb.coords(), stretch.inputs[0])
        nb.link(stretch.outputs[0], w.inputs["Vector"])
        r = nb.ramp(w.outputs["Fac"], c2, c1, 0.2, 0.8, y=200)
        nb.link(r.outputs["Color"], nb.bsdf.inputs["Base Color"])
        rough = 0.6 if rough is None else rough
        if preset == "bark":
            vor = nb.new("ShaderNodeTexVoronoi", y=-400)
            vor.inputs["Scale"].default_value = 10 * s
            nb.link(nb.coords(), vor.inputs["Vector"])
            add = nb.new("ShaderNodeMath", y=-400, operation="ADD")
            nb.link(w.outputs["Fac"], add.inputs[0])
            nb.link(vor.outputs["Distance"], add.inputs[1])
            nb.height(add.outputs[0], 0.8 * bump, 0.05)
            rough = 0.85
        else:
            nb.height(w.outputs["Fac"], 0.1 * bump, 0.01)

    elif preset in ("stone", "ground"):
        n = nb.new("ShaderNodeTexNoise", y=200)
        n.inputs["Scale"].default_value = 4 * s
        n.inputs["Detail"].default_value = 12
        n.inputs["Roughness"].default_value = 0.65
        nb.link(nb.coords(), n.inputs["Vector"])
        r = nb.ramp(n.outputs["Fac"], c2, c1, 0.3, 0.7, y=200)
        v = nb.new("ShaderNodeTexVoronoi", y=-200)
        v.feature = "DISTANCE_TO_EDGE"
        v.inputs["Scale"].default_value = 3 * s
        nb.link(nb.coords(), v.inputs["Vector"])
        cracks = nb.ramp(v.outputs["Distance"], (0, 0, 0, 1), (1, 1, 1, 1), 0.0, 0.08, y=-200)
        col = nb.mix_color(r.outputs["Color"], tuple(x * 0.4 for x in c2[:3]) + (1,), 0.0)
        mul = nb.new("ShaderNodeMix", y=0)
        mul.data_type = "RGBA"
        mul.blend_type = "MULTIPLY"
        mul.inputs[0].default_value = 0.6 if preset == "stone" else 0.0
        nb.link(col, mul.inputs[6])
        nb.link(cracks.outputs["Color"], mul.inputs[7])
        nb.link(mul.outputs[2], nb.bsdf.inputs["Base Color"])
        h = nb.new("ShaderNodeMath", y=-400, operation="MULTIPLY")
        nb.link(n.outputs["Fac"], h.inputs[0])
        nb.link(cracks.outputs["Color"], h.inputs[1])
        nb.height(h.outputs[0], 0.7 * bump, 0.05)
        rough = 0.85 if rough is None else rough

    elif preset == "brick":
        b = nb.new("ShaderNodeTexBrick", y=200)
        b.inputs["Color1"].default_value = c1
        b.inputs["Color2"].default_value = c2
        b.inputs["Mortar"].default_value = to_linear((0.6, 0.58, 0.55, 1))
        b.inputs["Scale"].default_value = 1.6 * s
        # projection murale : (x + y, z) pour que les briques restent droites sur toutes les façades
        sep = nb.new("ShaderNodeSeparateXYZ", y=300)
        nb.link(nb.coords(), sep.inputs[0])
        addxy = nb.new("ShaderNodeMath", y=300, operation="ADD")
        nb.link(sep.outputs[0], addxy.inputs[0])
        nb.link(sep.outputs[1], addxy.inputs[1])
        comb = nb.new("ShaderNodeCombineXYZ", y=300)
        nb.link(addxy.outputs[0], comb.inputs[0])
        nb.link(sep.outputs[2], comb.inputs[1])
        nb.link(comb.outputs[0], b.inputs["Vector"])
        nb.link(b.outputs["Color"], nb.bsdf.inputs["Base Color"])
        inv = nb.new("ShaderNodeMath", y=-400, operation="SUBTRACT")
        inv.inputs[0].default_value = 1.0
        nb.link(b.outputs["Fac"], inv.inputs[1])
        nb.height(inv.outputs[0], 0.6 * bump, 0.03)
        rough = 0.85 if rough is None else rough

    elif preset in ("skin", "organic", "leather", "fur"):
        n = nb.new("ShaderNodeTexNoise", y=200)
        n.inputs["Scale"].default_value = (6 if preset != "fur" else 2) * s
        n.inputs["Detail"].default_value = 8
        n.inputs["Distortion"].default_value = 0.6 if preset == "organic" else 0.1
        nb.link(nb.coords(), n.inputs["Vector"])
        r = nb.ramp(n.outputs["Fac"], c2, c1, 0.25, 0.75, y=200)
        base = r.outputs["Color"]
        height_src = n.outputs["Fac"]
        if veins is not None or preset == "organic":
            vc = to_linear(veins if veins is not None else tuple(min(1, x * 1.6) for x in color2[:3]) + (1,))
            v = nb.new("ShaderNodeTexVoronoi", y=500)
            v.feature = "DISTANCE_TO_EDGE"
            v.inputs["Scale"].default_value = 2.5 * s
            nb.link(n.outputs["Color"], v.inputs["Vector"])
            vm = nb.ramp(v.outputs["Distance"], (1, 1, 1, 1), (0, 0, 0, 1), 0.0, 0.035, y=500)
            base = nb.mix_color(base, vc, vm.outputs["Color"], y=400)
            sub = nb.new("ShaderNodeMath", y=-300, operation="SUBTRACT")
            nb.link(n.outputs["Fac"], sub.inputs[0])
            nb.link(vm.outputs["Color"], sub.inputs[1])
            height_src = sub.outputs[0]
        if preset == "leather":
            vv = nb.new("ShaderNodeTexVoronoi", y=-500)
            vv.inputs["Scale"].default_value = 60 * s
            nb.link(nb.coords(), vv.inputs["Vector"])
            height_src = vv.outputs["Distance"]
        if preset == "fur":
            nb.set("Sheen Weight", 0.8)
            nb.set("Sheen Roughness", 0.4)
        nb.link(base, nb.bsdf.inputs["Base Color"])
        if preset == "skin" or subsurface:
            nb.set("Subsurface Weight", 0.25 if subsurface is None else subsurface)
            nb.set("Subsurface Radius", (0.6, 0.25, 0.15))
            nb.set("Subsurface Scale", 0.05)
        nb.height(height_src, (0.25 if preset != "organic" else 0.6) * bump, 0.02)
        rough = {"skin": 0.45, "organic": 0.4, "leather": 0.55, "fur": 0.8}[preset] if rough is None else rough

    elif preset == "scales":
        v = nb.new("ShaderNodeTexVoronoi", y=200)
        v.inputs["Scale"].default_value = 18 * s
        v.inputs["Randomness"].default_value = 0.6
        nb.link(nb.coords(), v.inputs["Vector"])
        r = nb.ramp(v.outputs["Distance"], c1, c2, 0.15, 0.55, y=200)
        n = nb.new("ShaderNodeTexNoise", y=500)
        n.inputs["Scale"].default_value = 2 * s
        nb.link(nb.coords(), n.inputs["Vector"])
        col = nb.mix_color(r.outputs["Color"], tuple(min(1, x * 1.3) for x in c1[:3]) + (1,), n.outputs["Fac"],
                           blend="OVERLAY", y=400)
        nb.link(col, nb.bsdf.inputs["Base Color"])
        inv = nb.new("ShaderNodeMath", y=-400, operation="SUBTRACT")
        inv.inputs[0].default_value = 1.0
        nb.link(v.outputs["Distance"], inv.inputs[1])
        nb.height(inv.outputs[0], 0.6 * bump, 0.03)
        rr = nb.new("ShaderNodeMapRange", y=0)
        nb.link(v.outputs["Distance"], rr.inputs["Value"])
        base_r = 0.35 if rough is None else rough
        rr.inputs["To Min"].default_value = max(0, base_r - 0.15)
        rr.inputs["To Max"].default_value = min(1, base_r + 0.25)
        nb.link(rr.outputs["Result"], nb.bsdf.inputs["Roughness"])
        rough = None
        if veins is not None:
            nb.set("Coat Weight", 0.4)

    elif preset == "fabric":
        wx = nb.new("ShaderNodeTexWave", y=300)
        wy = nb.new("ShaderNodeTexWave", y=100)
        wy.bands_direction = "Y"
        for w in (wx, wy):
            w.inputs["Scale"].default_value = 60 * s
            nb.link(nb.coords(), w.inputs["Vector"])
        mx = nb.new("ShaderNodeMath", y=200, operation="MULTIPLY")
        nb.link(wx.outputs["Fac"], mx.inputs[0])
        nb.link(wy.outputs["Fac"], mx.inputs[1])
        r = nb.ramp(mx.outputs[0], c2, c1, 0.0, 0.6, y=200)
        nb.link(r.outputs["Color"], nb.bsdf.inputs["Base Color"])
        nb.set("Sheen Weight", 0.6)
        nb.height(mx.outputs[0], 0.2 * bump, 0.005)
        rough = 0.85 if rough is None else rough

    elif preset in ("glass", "ice", "crystal"):
        nb.set("Base Color", c1)
        nb.set("Transmission Weight", 1.0)
        nb.set("IOR", {"glass": 1.45, "ice": 1.31, "crystal": 1.6}[preset])
        rough = {"glass": 0.02, "ice": 0.15, "crystal": 0.05}[preset] if rough is None else rough
        n = nb.new("ShaderNodeTexNoise", y=-400)
        n.inputs["Scale"].default_value = 10 * s
        nb.link(nb.coords(), n.inputs["Vector"])
        nb.height(n.outputs["Fac"], (0.3 if preset == "ice" else 0.05) * bump, 0.01)
        if preset == "crystal":
            nb.set("Emission Color", c1)
            nb.set("Emission Strength", 0.5 if not emission_strength else emission_strength)

    elif preset == "lava":
        v = nb.new("ShaderNodeTexVoronoi", y=200)
        v.feature = "DISTANCE_TO_EDGE"
        v.inputs["Scale"].default_value = 4 * s
        n = nb.new("ShaderNodeTexNoise", y=400)
        n.inputs["Scale"].default_value = 3 * s
        nb.link(nb.coords(), n.inputs["Vector"])
        nb.link(n.outputs["Color"], v.inputs["Vector"])
        glow = nb.ramp(v.outputs["Distance"], (1, 1, 1, 1), (0, 0, 0, 1), 0.0, 0.12, y=200)
        hot = to_linear(emission if emission else (1.0, 0.35, 0.03, 1))
        rock = to_linear(color if color else (0.05, 0.03, 0.03, 1))
        nb.link(nb.mix_color(rock, hot, glow.outputs["Color"]), nb.bsdf.inputs["Base Color"])
        em = nb.new("ShaderNodeMix", y=0)
        em.data_type = "RGBA"
        em.inputs[6].default_value = (0, 0, 0, 1)
        em.inputs[7].default_value = hot
        nb.link(glow.outputs["Color"], em.inputs[0])
        nb.link(em.outputs[2], nb.bsdf.inputs["Emission Color"])
        nb.set("Emission Strength", emission_strength or 8.0)
        inv = nb.new("ShaderNodeMath", y=-400, operation="SUBTRACT")
        inv.inputs[0].default_value = 1.0
        nb.link(glow.outputs["Color"], inv.inputs[1])
        nb.height(inv.outputs[0], 0.8 * bump, 0.05)
        rough = 0.8 if rough is None else rough
        emission_strength = emission_strength or 8.0
        emission = emission or (1.0, 0.35, 0.03, 1)

    elif preset == "emission":
        nb.set("Base Color", c1)
        nb.set("Emission Color", to_linear(emission or color))
        nb.set("Emission Strength", emission_strength or 6.0)
        emission = emission or color
        emission_strength = emission_strength or 6.0
        rough = 0.3 if rough is None else rough

    elif preset == "eye":
        nb.set("Base Color", c1)
        nb.set("Coat Weight", 1.0)
        nb.set("Coat Roughness", 0.02)
        rough = 0.15 if rough is None else rough
        if emission_strength:
            nb.set("Emission Color", to_linear(emission or color))
            nb.set("Emission Strength", emission_strength)
            emission = emission or color

    # réglages communs
    if rough is not None:
        nb.set("Roughness", max(0.0, min(1.0, rough)))
    if metal is not None and not nb.bsdf.inputs["Metallic"].is_linked:
        nb.set("Metallic", max(0.0, min(1.0, metal)))
    if wet > 0 and not nb.bsdf.inputs["Roughness"].is_linked:
        nb.set("Roughness", nb.bsdf.inputs["Roughness"].default_value * (1 - 0.8 * wet))
    if wet > 0:
        nb.set("Coat Weight", min(1.0, 0.3 + wet))
        nb.set("Coat Roughness", 0.05)
    if emission is not None and emission_strength and preset not in ("lava", "crystal"):
        nb.set("Emission Color", to_linear(emission))
        nb.set("Emission Strength", emission_strength)
    mat["se_preset"] = preset
    # aperçu viewport (le rendu exact reste celui de Blender)
    mat["se_preview"] = {
        "color": [float(x) for x in preview_color],
        "metallic": float(1.0 if preset in ("metal", "gold") else (metal or 0.0)),
        "roughness": float(nb.bsdf.inputs["Roughness"].default_value),
        "emission": [float(x) for x in (emission or (0, 0, 0))[:3]],
        "emission_strength": float(emission_strength or 0.0),
    }
    mat.diffuse_color = (*color[:3], 1.0)


def new_material(name: str, preset: str = "plain", **kw) -> bpy.types.Material:
    mat = bpy.data.materials.new(name)
    if mat.node_tree is None:
        mat.use_nodes = True
    build_preset(mat, preset, **kw)
    return mat


def assign_material(obj, mat, slot: int | None = None, replace_all=True) -> None:
    me = obj.data
    if replace_all or not me.materials:
        me.materials.clear()
        me.materials.append(mat)
    elif slot is not None and slot < len(me.materials):
        me.materials[slot] = mat
    else:
        me.materials.append(mat)


def assign_simple_material(obj, preset, color) -> bpy.types.Material:
    mat = new_material(f"{obj.name}_mat", preset or "plain", color=color)
    assign_material(obj, mat)
    return mat


def describe_material(mat) -> dict:
    d = {"name": mat.name, "preset": mat.get("se_preset"), "users": mat.users}
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None) if mat.node_tree else None
    if bsdf is not None:
        def val(name):
            s = bsdf.inputs.get(name)
            if s is None:
                return None
            if s.is_linked:
                return "(texture)"
            v = s.default_value
            if hasattr(v, "__len__"):
                if len(v) == 4 and "Color" in name:
                    return to_hex([linear_to_srgb(c) for c in v[:3]])
                return [round(x, 4) for x in v]
            return round(float(v), 4)
        d["principled"] = {k: val(k) for k in ("Base Color", "Metallic", "Roughness", "IOR", "Alpha",
                                                "Subsurface Weight", "Transmission Weight", "Coat Weight",
                                                "Sheen Weight", "Emission Color", "Emission Strength")}
    nodes = []
    if mat.node_tree:
        for n in mat.node_tree.nodes:
            ins = []
            for s in n.inputs:
                if s.is_linked or not hasattr(s, "default_value") or not s.enabled:
                    continue
                v = s.default_value
                if hasattr(v, "__len__") and not isinstance(v, str):
                    v = [round(float(x), 4) for x in v]
                elif isinstance(v, float):
                    v = round(v, 4)
                ins.append({"name": s.name, "identifier": s.identifier, "type": s.type, "value": v})
            props = []
            for pid in ("wave_type", "bands_direction", "feature", "blend_type", "operation", "data_type",
                        "noise_dimensions", "distance", "voronoi_dimensions"):
                if hasattr(n, pid):
                    prop = n.bl_rna.properties[pid]
                    props.append({"id": pid, "value": getattr(n, pid),
                                  "items": [[i.identifier, i.name] for i in prop.enum_items]})
            nodes.append({"name": n.name, "label": n.label or n.bl_label, "type": n.bl_idname, "inputs": ins,
                          "properties": props,
                          "links_in": [f"{l.from_node.name}.{l.from_socket.name}" for s in n.inputs for l in s.links]})
        d["nodes"] = nodes
    d["images"] = sorted({n.image.name for n in mat.node_tree.nodes if n.type == "TEX_IMAGE" and n.image}) \
        if mat.node_tree else []
    return d


@command("list_materials", history=False)
def cmd_list_materials(engine, p: Params):
    """Liste des matériaux du projet et préréglages disponibles."""
    return {"materials": [{"name": m.name, "preset": m.get("se_preset"), "users": m.users}
                          for m in bpy.data.materials],
            "presets": [{"id": k, "label": PRESET_LABELS[k]} for k in PRESETS]}


@command("get_material", history=False)
def cmd_get_material(engine, p: Params):
    """Détails d'un matériau : paramètres Principled et nœuds avec leurs entrées modifiables."""
    mat = bpy.data.materials.get(p.str("material"))
    if mat is None:
        raise CommandError("Matériau introuvable.")
    return describe_material(mat)


def _preset_kwargs(p: Params) -> dict:
    kw = {}
    if p.has("color"):
        kw["color"] = p.color("color")
    if p.has("color2"):
        kw["color2"] = p.color("color2")
    if p.has("roughness"):
        kw["roughness"] = p.float("roughness", lo=0, hi=1)
    if p.has("metallic"):
        kw["metallic"] = p.float("metallic", lo=0, hi=1)
    if p.has("scale"):
        kw["scale"] = p.float("scale", lo=0.01, hi=1000)
    if p.has("bump"):
        kw["bump"] = p.float("bump", lo=0, hi=10)
    if p.has("emission"):
        kw["emission"] = p.color("emission")
    if p.has("emission_strength"):
        kw["emission_strength"] = p.float("emission_strength", lo=0, hi=1000)
    if p.has("wet"):
        kw["wet"] = p.float("wet", lo=0, hi=1)
    if p.has("veins"):
        kw["veins"] = p.color("veins")
    if p.has("subsurface"):
        kw["subsurface"] = p.float("subsurface", lo=0, hi=1)
    return kw


@command("create_material", label="Créer un matériau")
def cmd_create_material(engine, p: Params):
    """Crée un matériau PBR procédural : preset (metal, wood, stone, skin, scales, fabric, organic, glass, lava,
    emission...), couleurs, rugosité, métal, relief, émission, aspect humide, veines. Ou depuis une description
    textuelle (`description`). Affecté aux objets indiqués (`objects`) si fournis."""
    if p.has("description"):
        from .generators.prompt import parse_material_text
        spec = parse_material_text(p.str("description"))
        kw = dict(spec["params"])
        kw.update(_preset_kwargs(p))
        preset = p.str("preset", spec["preset"], choices=PRESETS)
        interp = spec
    else:
        kw = _preset_kwargs(p)
        preset = p.str("preset", "plain", choices=PRESETS)
        interp = None
    name = p.str("material", None) or p.str("name", None) or PRESET_LABELS[preset]
    mat = bpy.data.materials.get(name) if p.bool("overwrite", False) else None
    if mat is None:
        mat = bpy.data.materials.new(name)
        if mat.node_tree is None:
            mat.use_nodes = True
    build_preset(mat, preset, **kw)
    assigned = []
    for oname in p.list("objects", []):
        o = get_mesh_object(str(oname))
        assign_material(o, mat, replace_all=not p.bool("append_slot", False))
        assigned.append(o.name)
    out = {"material": mat.name, "preset": preset, "assigned_to": assigned, "_label": f"Matériau {mat.name}"}
    if interp:
        out["interpretation"] = interp
    return out


@command("assign_material", label="Affecter un matériau")
def cmd_assign_material(engine, p: Params):
    """Affecte un matériau existant à des objets (remplace, ou slot / faces précises)."""
    mat = bpy.data.materials.get(p.str("material"))
    if mat is None:
        raise CommandError("Matériau introuvable.")
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    faces = p.list("faces", [])
    for o in objs:
        if faces:
            me = o.data
            if mat.name not in [m.name for m in me.materials if m]:
                me.materials.append(mat)
            idx = [m.name if m else None for m in me.materials].index(mat.name)
            n = len(me.polygons)
            for f in faces:
                fi = int(f)
                if 0 <= fi < n:
                    me.polygons[fi].material_index = idx
        else:
            assign_material(o, mat)
    return {"material": mat.name, "objects": [o.name for o in objs]}


@command("set_material", label="Régler un matériau")
def cmd_set_material(engine, p: Params):
    """Modifie un matériau : valeurs Principled (base_color, metallic, roughness, emission, emission_strength,
    alpha, ior, transmission, subsurface, coat), ou entrée d'un nœud précis (node, input, value), ou propriété
    d'un nœud (node, property, value). `rebuild_preset` recrée le préréglage avec de nouveaux paramètres."""
    mat = bpy.data.materials.get(p.str("material"))
    if mat is None:
        raise CommandError("Matériau introuvable.")
    if p.has("rebuild_preset"):
        build_preset(mat, p.str("rebuild_preset", choices=PRESETS), **_preset_kwargs(p))
        return {"material": describe_material(mat)}
    nt = mat.node_tree
    bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
    mapping = {"base_color": "Base Color", "metallic": "Metallic", "roughness": "Roughness", "alpha": "Alpha",
               "ior": "IOR", "transmission": "Transmission Weight", "subsurface": "Subsurface Weight",
               "coat": "Coat Weight", "sheen": "Sheen Weight", "emission": "Emission Color",
               "emission_strength": "Emission Strength", "specular": "Specular IOR Level"}
    changed = []
    for key, sock in mapping.items():
        if not p.has(key):
            continue
        if bsdf is None:
            raise CommandError("Ce matériau n'a pas de nœud Principled BSDF.")
        s = bsdf.inputs[sock]
        if s.is_linked:
            for l in list(s.links):
                nt.links.remove(l)
        if key in ("base_color", "emission"):
            s.default_value = to_linear(p.color(key))
        else:
            s.default_value = p.float(key, lo=0, hi=1000)
        changed.append(key)
        prev = mat.get("se_preview")
        if prev is not None:
            pv = dict(prev.to_dict() if hasattr(prev, "to_dict") else prev)
            if key == "base_color":
                pv["color"] = list(p.color(key)[:3])
            elif key == "emission":
                pv["emission"] = list(p.color(key)[:3])
            elif key in ("metallic", "roughness", "emission_strength"):
                pv[key] = p.float(key)
            mat["se_preview"] = pv
    if p.has("alpha") and p.float("alpha") < 1:
        try:
            mat.surface_render_method = "BLENDED"
        except AttributeError:
            pass
    if p.has("node"):
        node = nt.nodes.get(p.str("node"))
        if node is None:
            raise CommandError(f"Nœud introuvable : « {p.str('node')} ».")
        if p.has("input"):
            key = p.str("input")
            s = node.inputs.get(key) or next((x for x in node.inputs if x.identifier == key), None)
            if s is None or not hasattr(s, "default_value"):
                raise CommandError(f"Entrée introuvable : « {key} ».")
            v = p.raw("value")
            try:
                if s.type == "RGBA":
                    s.default_value = to_linear(p.color("value"))
                elif hasattr(s.default_value, "__len__"):
                    s.default_value = [float(x) for x in v]
                elif s.type in ("INT",):
                    s.default_value = int(v)
                elif s.type == "BOOLEAN":
                    s.default_value = bool(v)
                else:
                    s.default_value = float(v)
            except (TypeError, ValueError) as e:
                raise CommandError(f"Valeur invalide : {e}") from e
            changed.append(f"{node.name}.{s.name}")
        elif p.has("property"):
            rna_set(node, p.str("property"), p.raw("value"))
            changed.append(f"{node.name}.{p.str('property')}")
    if p.has("new_name"):
        mat.name = p.str("new_name", max_len=60)
    return {"material": mat.name, "changed": changed, "_label": f"Régler {mat.name}"}


@command("create_texture", label="Créer une texture")
def cmd_create_texture(engine, p: Params):
    """Crée une texture procédurale décrite par texte (ex. « peau de monstre noire, humide, veines violettes »)
    et l'applique comme matériau aux objets. Avec `bake_resolution` (512..4096), cuit aussi les cartes PBR en images."""
    desc = p.str("description")
    from .generators.prompt import parse_material_text
    spec = parse_material_text(desc)
    kw = dict(spec["params"])
    kw.update(_preset_kwargs(p))
    name = p.str("name", desc[:40].strip().capitalize() or "Texture")
    mat = bpy.data.materials.new(name)
    if mat.node_tree is None:
        mat.use_nodes = True
    build_preset(mat, spec["preset"], **kw)
    objs = [get_mesh_object(str(n)) for n in p.list("objects", [])]
    for o in objs:
        assign_material(o, mat)
    out = {"material": mat.name, "interpretation": spec, "objects": [o.name for o in objs],
           "_label": f"Texture « {name} »"}
    res = p.int("bake_resolution", 0, lo=0, hi=8192)
    if res:
        if not objs:
            raise CommandError("La cuisson nécessite au moins un objet (`objects`).")
        out["baked"] = [bake_pbr(engine, o, res, p.list("maps", None)) for o in objs]
    return out


# ---------------------------------------------------------------------------
# Cuisson (bake) des cartes PBR
# ---------------------------------------------------------------------------
BAKE_MAPS = ["base_color", "roughness", "metallic", "normal", "height", "ao", "emission"]


def ensure_uv(obj) -> bool:
    """Crée une carte UV (projection intelligente) si l'objet n'en a pas. Renvoie True si créée."""
    if obj.data.uv_layers:
        return False
    from .uv import unwrap
    unwrap(obj, "smart")
    return True


def bake_pbr(engine, obj, resolution: int, maps: list | None = None, samples: int = 16,
             apply_result: bool = True) -> dict:
    """Cuisson réelle des cartes PBR avec Cycles. Enregistre des PNG et (option) remplace le matériau par
    une version à textures images, exportable vers les moteurs de jeu (glTF/FBX)."""
    if obj.type != "MESH":
        raise CommandError("La cuisson ne s'applique qu'aux maillages.")
    maps = [m for m in (maps or ["base_color", "roughness", "metallic", "normal", "height", "ao"]) if m in BAKE_MAPS]
    if not maps:
        raise CommandError(f"Aucune carte valide. Choix : {', '.join(BAKE_MAPS)}")
    if resolution not in (256, 512, 1024, 2048, 4096, 8192):
        raise CommandError("Résolution de cuisson : 256, 512, 1024, 2048, 4096 ou 8192.")
    mats = [m for m in obj.data.materials if m is not None]
    if not mats:
        raise CommandError(f"« {obj.name} » n'a pas de matériau à cuire.")
    created_uv = ensure_uv(obj)
    sc = bpy.context.scene
    prev_engine = sc.render.engine
    prev_samples = sc.cycles.samples
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = samples
    sc.render.bake.margin = max(4, resolution // 128)
    folder = engine.home / "textures" / _safe(obj.name)
    folder.mkdir(parents=True, exist_ok=True)
    results = {}
    select_only([obj])
    t0 = time.time()
    try:
        for i, kind in enumerate(maps):
            engine.progress(f"Cuisson {kind} ({resolution}px) — {obj.name}", i / len(maps))
            non_color = kind not in ("base_color", "emission")
            img = bpy.data.images.new(f"{obj.name}_{kind}", resolution, resolution, alpha=False,
                                      float_buffer=kind in ("normal", "height"), is_data=non_color)
            img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
            tmp_nodes = []
            restore = []
            for mat in mats:
                nt = mat.node_tree
                tn = nt.nodes.new("ShaderNodeTexImage")
                tn.image = img
                tn.location = (600, -600)
                nt.nodes.active = tn
                tmp_nodes.append((nt, tn))
                if kind in ("metallic", "height"):
                    restore.append(_emission_trick(mat, kind))
            with override(obj):
                if kind == "base_color":
                    bpy.ops.object.bake(type="DIFFUSE", pass_filter={"COLOR"}, use_clear=True)
                elif kind == "roughness":
                    bpy.ops.object.bake(type="ROUGHNESS", use_clear=True)
                elif kind == "normal":
                    bpy.ops.object.bake(type="NORMAL", normal_space="TANGENT", use_clear=True)
                elif kind == "ao":
                    bpy.ops.object.bake(type="AO", use_clear=True)
                elif kind == "emission":
                    bpy.ops.object.bake(type="EMIT", use_clear=True)
                else:
                    bpy.ops.object.bake(type="EMIT", use_clear=True)
            for r in restore:
                r()
            for nt, tn in tmp_nodes:
                nt.nodes.remove(tn)
            path = folder / f"{_safe(obj.name)}_{kind}.png"
            img.filepath_raw = str(path)
            img.file_format = "PNG"
            img.save()
            if not path.exists() or path.stat().st_size == 0:
                raise CommandError(f"La carte {kind} n'a pas été écrite.")
            img.pack()
            results[kind] = {"image": img.name, "path": str(path)}
    finally:
        sc.render.engine = prev_engine
        sc.cycles.samples = prev_samples
    out = {"object": obj.name, "resolution": resolution, "maps": results, "uv_created": created_uv,
           "seconds": round(time.time() - t0, 1)}
    if apply_result:
        out["material"] = _apply_baked_material(obj, results)
    return out


def _emission_trick(mat, kind):
    """Branche temporairement une valeur (métal, hauteur) sur une émission pour la cuire."""
    nt = mat.node_tree
    out = next((n for n in nt.nodes if n.type == "OUTPUT_MATERIAL" and n.is_active_output), None) or \
        next((n for n in nt.nodes if n.type == "OUTPUT_MATERIAL"), None)
    bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if out is None:
        return lambda: None
    prev_links = [(l.from_socket, l.to_socket) for l in out.inputs["Surface"].links]
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Strength"].default_value = 1.0
    val = None
    if kind == "metallic" and bsdf is not None:
        s = bsdf.inputs["Metallic"]
        if s.is_linked:
            nt.links.new(s.links[0].from_socket, em.inputs["Color"])
        else:
            val = s.default_value
    elif kind == "height":
        h = nt.nodes.get("SE_Height")
        if h is not None:
            nt.links.new(h.outputs[0], em.inputs["Color"])
        else:
            val = 0.5
    if val is not None:
        em.inputs["Color"].default_value = (val, val, val, 1)
    nt.links.new(em.outputs[0], out.inputs["Surface"])

    def restore():
        nt.nodes.remove(em)
        for a, b in prev_links:
            nt.links.new(a, b)
    return restore


def _apply_baked_material(obj, results) -> str:
    mat = bpy.data.materials.new(f"{obj.name}_PBR")
    if mat.node_tree is None:
        mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    y = 300

    def tex(kind):
        nonlocal y
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = bpy.data.images[results[kind]["image"]]
        n.location = (-600, y)
        y -= 300
        return n

    if "base_color" in results:
        t = tex("base_color")
        if "ao" in results:
            ao = tex("ao")
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.blend_type = "MULTIPLY"
            mix.inputs[0].default_value = 0.6
            mix.location = (-300, 300)
            nt.links.new(t.outputs["Color"], mix.inputs[6])
            nt.links.new(ao.outputs["Color"], mix.inputs[7])
            nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
        else:
            nt.links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
    if "roughness" in results:
        nt.links.new(tex("roughness").outputs["Color"], bsdf.inputs["Roughness"])
    if "metallic" in results:
        nt.links.new(tex("metallic").outputs["Color"], bsdf.inputs["Metallic"])
    if "emission" in results:
        nt.links.new(tex("emission").outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = 1.0
    if "normal" in results:
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nm.location = (-300, -300)
        nt.links.new(tex("normal").outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    mat["se_baked"] = True
    src = obj.data.materials[0] if obj.data.materials else None
    if src is not None and src.get("se_preview") is not None:
        mat["se_preview"] = src["se_preview"]
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    return mat.name


def _safe(name: str) -> str:
    return "".join(c if c.isalnum() or c in "-_" else "_" for c in name)[:60] or "objet"


@command("bake_textures", label="Cuire les textures PBR")
def cmd_bake(engine, p: Params):
    """Cuit les cartes PBR (base_color, roughness, metallic, normal, height, ao, emission) en images PNG à la
    résolution choisie (512, 1024, 2048, 4096), avec Cycles. Remplace le matériau par une version à textures
    images (apply=true) pour l'export vers Unity/Unreal/Godot."""
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    res = p.int("resolution", 1024, lo=256, hi=8192)
    results = [bake_pbr(engine, o, res, p.list("maps", None), samples=p.int("samples", 16, lo=1, hi=1024),
                        apply_result=p.bool("apply", True)) for o in objs]
    return {"results": results, "_label": f"Cuisson PBR {res}px"}


@command("get_material_texture", history=False)
def cmd_material_texture(engine, p: Params):
    """Chemin de l'image de couleur de base d'un matériau (pour l'aperçu du viewport)."""
    mat = bpy.data.materials.get(p.str("material"))
    if mat is None or mat.node_tree is None:
        raise CommandError("Matériau introuvable.")
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None or not bsdf.inputs["Base Color"].is_linked:
        raise CommandError("Pas de texture image.")
    node = bsdf.inputs["Base Color"].links[0].from_node
    if node.type == "MIX":
        node = next((l.from_node for s in node.inputs for l in s.links if l.from_node.type == "TEX_IMAGE"), node)
    if node.type != "TEX_IMAGE" or node.image is None:
        raise CommandError("Pas de texture image.")
    img = node.image
    path = bpy.path.abspath(img.filepath_raw or img.filepath)
    if not path or not os.path.exists(path):
        path = str(engine.home / "tmp" / f"{_safe(img.name)}.png")
        img.save(filepath=path) if not img.packed_file else _save_packed(img, path)
    return {"path": path}


def _save_packed(img, path):
    img2 = img.copy()
    img2.filepath_raw = path
    img2.file_format = "PNG"
    img2.save()
    bpy.data.images.remove(img2)
