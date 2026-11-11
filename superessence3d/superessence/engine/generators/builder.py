"""Outils de construction procédurale (bmesh) utilisés par tous les générateurs.

- SkinBody : corps organiques à partir d'un graphe de squelette (modificateur Skin de Blender
  + Subdivision Surface). Le graphe sert aussi à créer l'armature (rigging exact).
- sweep : tube effilé le long d'une courbe (cornes, tentacules, griffes, branches, lames...).
- lathe : objet de révolution (vases, tonneaux, tours, bouteilles, colonnes).
- loft : surface passant par des sections (carrosseries, coques, fuselages).
- membrane : surface entre des lignes (membranes d'ailes, voiles, nageoires).
- box / ellipsoid / cylinder : primitives paramétrées.
"""
from __future__ import annotations

import math
import random
from typing import Sequence

import bmesh
import bpy
from mathutils import Matrix, Vector

from ..materials import new_material
from ..util import apply_modifiers, link_to_collection, set_json_prop, unique_name


class Model:
    """Racine (Empty) d'un modèle généré, avec ses parties et son squelette."""

    def __init__(self, name: str, spec: dict, collection=None):
        self.collection = collection or bpy.context.collection or bpy.context.scene.collection
        self.root = bpy.data.objects.new(unique_name(name), None)
        self.root.empty_display_type = "PLAIN_AXES"
        self.root.empty_display_size = 0.3
        self.collection.objects.link(self.root)
        self.spec = spec
        self.parts: list[bpy.types.Object] = []
        self.bones: list[dict] = []
        self.materials: dict[str, bpy.types.Material] = {}
        self.rng = random.Random(int(spec.get("seed", 1)))

    def material(self, key: str, preset: str, **kw) -> bpy.types.Material:
        if key not in self.materials:
            self.materials[key] = new_material(f"{self.root.name}_{key}", preset, **kw)
        return self.materials[key]

    def add(self, obj: bpy.types.Object, mat=None, bone: str | None = None, part: str | None = None):
        if obj.name not in self.collection.objects:
            for c in list(obj.users_collection):
                c.objects.unlink(obj)
            self.collection.objects.link(obj)
        obj.parent = self.root
        if mat is not None and obj.type == "MESH":
            obj.data.materials.clear()
            obj.data.materials.append(mat)
        if bone:
            obj["se_bone"] = bone
        if part:
            obj["se_part"] = part
        self.parts.append(obj)
        return obj

    def bone(self, name, head, tail, parent=None, role=""):
        self.bones.append({"name": name, "head": [float(x) for x in head], "tail": [float(x) for x in tail],
                           "parent": parent, "role": role})
        return name

    def finish(self):
        set_json_prop(self.root, "se_spec", self.spec)
        if self.bones:
            set_json_prop(self.root, "se_skeleton", self.bones)
        return self.root


# ---------------------------------------------------------------------------
# Objets depuis bmesh
# ---------------------------------------------------------------------------
def obj_from_bm(name: str, bm: bmesh.types.BMesh, smooth: bool = True) -> bpy.types.Object:
    me = bpy.data.meshes.new(name)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    if smooth:
        me.shade_smooth()
    obj = bpy.data.objects.new(name, me)
    link_to_collection(obj)
    return obj


def add_subsurf(obj, levels=2, render=None):
    m = obj.modifiers.new("Subdivision", "SUBSURF")
    m.levels = levels
    m.render_levels = render if render is not None else levels
    return m


def add_bevel(obj, width=0.02, segments=2, angle=35):
    m = obj.modifiers.new("Biseau", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.angle_limit = math.radians(angle)
    m.harden_normals = False
    return m


# ---------------------------------------------------------------------------
# SkinBody : corps organiques
# ---------------------------------------------------------------------------
class SkinBody:
    def __init__(self):
        self.verts: list[Vector] = []
        self.radii: list[tuple[float, float]] = []
        self.edges: list[tuple[int, int]] = []
        self.roots: set[int] = set()

    def v(self, pos, r) -> int:
        self.verts.append(Vector(pos))
        if isinstance(r, (int, float)):
            r = (r, r)
        self.radii.append((max(r[0], 0.002), max(r[1], 0.002)))
        return len(self.verts) - 1

    def e(self, a: int, b: int):
        self.edges.append((a, b))

    def chain(self, start: int | None, points: Sequence, radii: Sequence) -> list[int]:
        """Ajoute une chaîne de sommets reliés ; `start` = sommet existant auquel l'attacher."""
        idx = []
        prev = start
        for p, r in zip(points, radii):
            i = self.v(p, r)
            if prev is not None:
                self.e(prev, i)
            idx.append(i)
            prev = i
        return idx

    def build(self, name: str, subdiv: int = 2, apply_skin: bool = True) -> bpy.types.Object:
        me = bpy.data.meshes.new(name)
        me.from_pydata([tuple(v) for v in self.verts], self.edges, [])
        obj = bpy.data.objects.new(name, me)
        link_to_collection(obj)
        skin = obj.modifiers.new("Peau", "SKIN")
        skin.use_smooth_shade = True
        skin.branch_smoothing = 0.6
        layer = me.skin_vertices[0].data if len(me.skin_vertices) else me.skin_vertices.new().data
        for i, r in enumerate(self.radii):
            layer[i].radius = r
        roots = self.roots or {0}
        for i in roots:
            layer[i].use_root = True
        if apply_skin:
            apply_modifiers(obj, only=[skin.name])
            me = obj.data
            me.shade_smooth()
            # nettoyage léger du maillage de peau (sommets proches aux jonctions)
            bm = bmesh.new()
            bm.from_mesh(me)
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
            bm.to_mesh(me)
            bm.free()
        if subdiv:
            add_subsurf(obj, subdiv)
        return obj


# ---------------------------------------------------------------------------
# Tubes, révolutions, sections
# ---------------------------------------------------------------------------
def _frames(path: list[Vector]):
    """Repères à transport parallèle le long d'une polyligne (sans torsion)."""
    tangents = []
    n = len(path)
    for i in range(n):
        if i == 0:
            t = path[1] - path[0]
        elif i == n - 1:
            t = path[-1] - path[-2]
        else:
            t = path[i + 1] - path[i - 1]
        tangents.append(t.normalized() if t.length > 1e-9 else Vector((0, 0, 1)))
    up = Vector((0, 0, 1)) if abs(tangents[0].z) < 0.9 else Vector((1, 0, 0))
    normal = tangents[0].cross(up).normalized()
    frames = []
    for i, t in enumerate(tangents):
        if i > 0:
            axis = tangents[i - 1].cross(t)
            if axis.length > 1e-9:
                ang = tangents[i - 1].angle(t)
                normal = Matrix.Rotation(ang, 3, axis.normalized()) @ normal
        binormal = t.cross(normal).normalized()
        frames.append((t, normal.normalized(), binormal))
    return frames


def sweep(name: str, path: Sequence, radii: Sequence, sides: int = 10, cap_start: bool = True,
          tip: bool = False, flatten: float = 1.0, smooth: bool = True, profile: Sequence | None = None) -> bpy.types.Object:
    """Tube le long de `path`, rayon `radii[i]` à chaque point. `tip`=pointe finale ; `flatten` aplatit la section ;
    `profile` = liste de (x, y) pour une section personnalisée (lames, profils)."""
    path = [Vector(p) for p in path]
    if len(path) < 2:
        raise ValueError("sweep : au moins 2 points")
    bm = bmesh.new()
    rings = []
    frames = _frames(path)
    if profile is None:
        profile = [(math.cos(2 * math.pi * k / sides), math.sin(2 * math.pi * k / sides) * flatten) for k in range(sides)]
    last = len(path) - 1
    for i, (p, (t, nrm, bn)) in enumerate(zip(path, frames)):
        r = radii[i]
        if tip and i == last:
            rings.append([bm.verts.new(p)])
            continue
        ring = [bm.verts.new(p + nrm * (x * r) + bn * (y * r)) for x, y in profile]
        rings.append(ring)
    k = len(profile)
    for a, b in zip(rings[:-1], rings[1:]):
        if len(b) == 1:
            for j in range(k):
                bm.faces.new((a[j], a[(j + 1) % k], b[0]))
        else:
            for j in range(k):
                bm.faces.new((a[j], a[(j + 1) % k], b[(j + 1) % k], b[j]))
    if cap_start and len(rings[0]) > 2:
        bm.faces.new(list(reversed(rings[0])))
    if not tip and len(rings[-1]) > 2:
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, smooth)


def curve_points(start, direction, length, segments=8, bend_axis=(1, 0, 0), bend=0.0, twist=0.0, droop=0.0):
    """Points d'une courbe : départ, direction, longueur, courbure (radians au total) autour d'un axe."""
    pts = [Vector(start)]
    d = Vector(direction).normalized()
    axis = Vector(bend_axis).normalized()
    step = length / segments
    p = Vector(start)
    for i in range(segments):
        rot = Matrix.Rotation(bend / segments, 3, axis)
        d = (rot @ d).normalized()
        if twist:
            axis = (Matrix.Rotation(twist / segments, 3, d) @ axis).normalized()
        if droop:
            d = (d + Vector((0, 0, -droop / segments))).normalized()
        p = p + d * step
        pts.append(p.copy())
    return pts


def taper(n, r0, r1=0.0, power=1.0):
    return [r0 + (r1 - r0) * ((i / (n - 1)) ** power) for i in range(n)]


def lathe(name: str, profile: Sequence, segments: int = 32, smooth: bool = True, cap: bool = True) -> bpy.types.Object:
    """Révolution d'un profil [(rayon, z), ...] autour de l'axe Z."""
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        if r < 1e-5:
            rings.append([bm.verts.new((0, 0, z))])
        else:
            rings.append([bm.verts.new((r * math.cos(2 * math.pi * k / segments),
                                        r * math.sin(2 * math.pi * k / segments), z)) for k in range(segments)])
    for a, b in zip(rings[:-1], rings[1:]):
        if len(a) == 1 and len(b) == 1:
            continue
        if len(a) == 1:
            for j in range(segments):
                bm.faces.new((a[0], b[j], b[(j + 1) % segments]))
        elif len(b) == 1:
            for j in range(segments):
                bm.faces.new((a[j], a[(j + 1) % segments], b[0]))
        else:
            for j in range(segments):
                bm.faces.new((a[j], a[(j + 1) % segments], b[(j + 1) % segments], b[j]))
    if cap:
        if len(rings[0]) > 2:
            bm.faces.new(list(reversed(rings[0])))
        if len(rings[-1]) > 2:
            bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, smooth)


def loft(name: str, sections: Sequence[Sequence], cap: bool = True, smooth: bool = True,
         closed: bool = True) -> bpy.types.Object:
    """Surface passant par des sections de même nombre de points (anneaux fermés)."""
    bm = bmesh.new()
    rings = [[bm.verts.new(Vector(p)) for p in sec] for sec in sections]
    k = len(rings[0])
    for a, b in zip(rings[:-1], rings[1:]):
        rng = range(k) if closed else range(k - 1)
        for j in rng:
            bm.faces.new((a[j], a[(j + 1) % k], b[(j + 1) % k], b[j]))
    if cap and closed:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, smooth)


def ellipse_section(center, rx, rz, n=16, squareness=0.0, y=None):
    """Section elliptique dans le plan XZ (pour des lofts le long de Y)."""
    cx, cy, cz = center
    pts = []
    for k in range(n):
        a = 2 * math.pi * k / n
        c, s = math.cos(a), math.sin(a)
        if squareness:
            e = 2 / (2 + 6 * squareness)
            c = math.copysign(abs(c) ** e, c)
            s = math.copysign(abs(s) ** e, s)
        pts.append((cx + rx * c, cy if y is None else y, cz + rz * s))
    return pts


def membrane(name: str, lines: Sequence[Sequence], sag: float = 0.0, thickness: float = 0.01,
             smooth: bool = True) -> bpy.types.Object:
    """Surface tendue entre des polylignes successives de même longueur (membrane d'aile, voile)."""
    bm = bmesh.new()
    rows = []
    for li, line in enumerate(lines):
        rows.append([bm.verts.new(Vector(p)) for p in line])
    # affaissement entre les doigts : on décale les points intermédiaires
    for a, b in zip(rows[:-1], rows[1:]):
        for j in range(len(a) - 1):
            bm.faces.new((a[j], a[j + 1], b[j + 1], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = obj_from_bm(name, bm, smooth)
    if thickness:
        m = obj.modifiers.new("Épaisseur", "SOLIDIFY")
        m.thickness = thickness
        m.offset = 0
    return obj


def box(name: str, size, location=(0, 0, 0), bevel: float = 0.0, segments: int = 2, smooth=False):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(location)
    obj = obj_from_bm(name, bm, smooth)
    if bevel:
        add_bevel(obj, bevel, segments, angle=60)
    return obj


def ellipsoid(name: str, radii, location=(0, 0, 0), segments=24, rings=12, smooth=True):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * radii[0], v.co.y * radii[1], v.co.z * radii[2])) + Vector(location)
    return obj_from_bm(name, bm, smooth)


def cylinder(name: str, radius, depth, location=(0, 0, 0), segments=24, axis="Z", smooth=True, cap=True,
             radius_top=None):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=cap, cap_tris=False, segments=segments, radius1=radius,
                          radius2=radius if radius_top is None else radius_top, depth=depth)
    rot = {"Z": Matrix.Identity(3), "X": Matrix.Rotation(math.pi / 2, 3, "Y"),
           "Y": Matrix.Rotation(math.pi / 2, 3, "X")}[axis]
    for v in bm.verts:
        v.co = rot @ v.co + Vector(location)
    obj = obj_from_bm(name, bm, smooth)
    if smooth:
        obj.data.set_sharp_from_angle(angle=math.radians(40))
    return obj


def join(objs: list, name: str | None = None):
    """Fusionne des objets maillage (le premier reçoit les autres)."""
    objs = [o for o in objs if o is not None]
    if not objs:
        return None
    if len(objs) == 1:
        if name:
            objs[0].name = name
        return objs[0]
    for o in objs:
        for m in list(o.modifiers):
            if m.type in ("SOLIDIFY", "BEVEL", "SUBSURF"):
                apply_modifiers(o, only=[m.name])
    from ..util import override
    with override(objs[0], objs):
        bpy.ops.object.join()
    if name:
        objs[0].name = name
    return objs[0]


def displace_detail(obj, strength=0.02, scale=0.15, kind="VORONOI", name="Détail"):
    """Détail géométrique réel (écailles, rugosité) via un modificateur Displace texturé."""
    tex = bpy.data.textures.new(f"{obj.name}_{name}", type=kind)
    if kind == "VORONOI":
        tex.noise_scale = scale
        tex.distance_metric = "DISTANCE"
        tex.color_mode = "INTENSITY"
    elif kind == "CLOUDS":
        tex.noise_scale = scale
        tex.noise_depth = 4
    m = obj.modifiers.new(name, "DISPLACE")
    m.texture = tex
    m.strength = strength
    m.mid_level = 0.5
    m.texture_coords = "OBJECT" if False else "LOCAL"
    return m


def mirror_x(points):
    return [Vector((-p[0], p[1], p[2])) for p in points]
