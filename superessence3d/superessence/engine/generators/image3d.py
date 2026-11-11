"""Image(s) → modèle 3D par méthodes GÉOMÉTRIQUES (pas d'IA neuronale).

- 1 image : la silhouette est extraite puis « gonflée » (carte de distance) en un volume
  fermé, texturé par l'image en face avant. Bon pour les logos, objets plats, créatures
  vues de face, personnages cartoon. La profondeur réelle n'est pas devinée.
- 2 ou 3 images (face, profil, dessus) : reconstruction par enveloppe visuelle (sculpture
  de voxels par les silhouettes), puis remaillage lisse. Bonne forme globale si les vues
  sont orthogonales et cadrées pareil ; les creux invisibles en silhouette ne sont pas captés.

Pour une vraie reconstruction neuronale image→3D, configurer un moteur local facultatif
(voir ai_backends.py).
"""
from __future__ import annotations

import os

import bmesh
import bpy
import numpy as np

from ..core import CommandError, Params, command
from ..util import apply_modifiers, link_to_collection, select_only, set_json_prop, unique_name
from .builder import add_subsurf


def load_mask(path: str, size: int, threshold: float = 0.12):
    if not os.path.isfile(path):
        raise CommandError(f"Image introuvable : {path}")
    try:
        img = bpy.data.images.load(path, check_existing=False)
    except RuntimeError as e:
        raise CommandError(f"Image illisible : {e}") from e
    w, h = img.size
    if w == 0 or h == 0:
        raise CommandError("Image vide.")
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)  # origine en bas à gauche (convention Blender)
    alpha = px[:, :, 3]
    if alpha.min() < 0.9:
        mask = alpha > 0.5
        method = "transparence"
    else:
        corners = np.concatenate([px[:4, :4, :3].reshape(-1, 3), px[:4, -4:, :3].reshape(-1, 3),
                                  px[-4:, :4, :3].reshape(-1, 3), px[-4:, -4:, :3].reshape(-1, 3)])
        bg = np.median(corners, axis=0)
        dist = np.linalg.norm(px[:, :, :3] - bg, axis=2)
        mask = dist > threshold
        method = "fond uni détecté"
    # recadrage sur la silhouette + carré + sous-échantillonnage
    ys, xs = np.nonzero(mask)
    if len(xs) < 20:
        bpy.data.images.remove(img)
        raise CommandError("Silhouette introuvable : utilisez une image sur fond uni ou transparent.")
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    side = max(x1 - x0, y1 - y0)
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    pad = int(side * 0.06) + 2
    half = side // 2 + pad
    sq = np.zeros((2 * half, 2 * half), dtype=bool)
    sx0, sy0 = cx - half, cy - half
    for (dst, src, n, o) in (("x", xs, w, sx0),):
        pass
    xa, xb = max(0, sx0), min(w, sx0 + 2 * half)
    ya, yb = max(0, sy0), min(h, sy0 + 2 * half)
    sq[ya - sy0:yb - sy0, xa - sx0:xb - sx0] = mask[ya:yb, xa:xb]
    idx = (np.arange(size) * (2 * half) / size).astype(int)
    small = sq[np.ix_(idx, idx)]
    crop = {"x0": sx0 / w, "y0": sy0 / h, "sx": 2 * half / w, "sy": 2 * half / h}
    return small, img, crop, method, (w, h)


def _distance(mask: np.ndarray) -> np.ndarray:
    """Transformée de distance (chanfrein 3-4), en pixels."""
    INF = 10 ** 6
    d = np.where(mask, INF, 0).astype(np.float64)
    h, w = d.shape
    for y in range(h):
        row = d[y]
        prev = d[y - 1] if y else None
        for x in range(w):
            if row[x] == 0:
                continue
            best = row[x]
            if x:
                best = min(best, row[x - 1] + 3)
            if prev is not None:
                best = min(best, prev[x] + 3)
                if x:
                    best = min(best, prev[x - 1] + 4)
                if x + 1 < w:
                    best = min(best, prev[x + 1] + 4)
            row[x] = best
    for y in range(h - 1, -1, -1):
        row = d[y]
        nxt = d[y + 1] if y + 1 < h else None
        for x in range(w - 1, -1, -1):
            if row[x] == 0:
                continue
            best = row[x]
            if x + 1 < w:
                best = min(best, row[x + 1] + 3)
            if nxt is not None:
                best = min(best, nxt[x] + 3)
                if x + 1 < w:
                    best = min(best, nxt[x + 1] + 4)
                if x:
                    best = min(best, nxt[x - 1] + 4)
            row[x] = best
    return d / 3.0


def inflate_from_image(engine, path: str, height: float = 1.0, depth: float = 0.5, resolution: int = 96,
                       name: str | None = None, threshold: float = 0.12):
    mask, img, crop, method, (iw, ih) = load_mask(path, resolution, threshold)
    n = resolution
    # érosion légère : supprime les pixels isolés
    dist = _distance(mask)
    dmax = max(dist.max(), 1.0)
    hgt = np.sqrt(np.clip(dist / dmax, 0, 1))  # profil arrondi
    cell = 1.0 / n
    bm = bmesh.new()
    front, back = {}, {}
    # sommets aux coins des pixels (un coin est « dedans » s'il touche un pixel de la silhouette)
    inside = np.zeros((n + 1, n + 1), dtype=bool)
    inside[:-1, :-1] |= mask
    inside[1:, :-1] |= mask
    inside[:-1, 1:] |= mask
    inside[1:, 1:] |= mask
    border = np.zeros_like(inside)
    full = np.zeros((n + 1, n + 1), dtype=bool)
    full[1:-1, 1:-1] = mask[:-1, :-1] & mask[1:, :-1] & mask[:-1, 1:] & mask[1:, 1:]
    border = inside & ~full
    # hauteur aux coins : moyenne des pixels voisins
    hp = np.pad(hgt, 1)
    hc = (hp[:-1, :-1] + hp[1:, :-1] + hp[:-1, 1:] + hp[1:, 1:]) / 4
    uv_of = {}
    for j in range(n + 1):
        for i in range(n + 1):
            if not inside[j, i]:
                continue
            x = (i * cell - 0.5)
            z = j * cell
            t = 0.0 if border[j, i] else hc[j, i] * depth * 0.5
            front[(j, i)] = bm.verts.new((x, -t, z))
            back[(j, i)] = front[(j, i)] if border[j, i] else bm.verts.new((x, t, z))
            uv_of[(j, i)] = (crop["x0"] + crop["sx"] * i / n, crop["y0"] + crop["sy"] * j / n)
    faces_front = []
    for j in range(n):
        for i in range(n):
            if not mask[j, i]:
                continue
            a, b, c, d = (j, i), (j, i + 1), (j + 1, i + 1), (j + 1, i)
            try:
                f = bm.faces.new((front[a], front[b], front[c], front[d]))
                faces_front.append((f, (a, b, c, d)))
            except ValueError:
                pass
            try:
                bm.faces.new((back[d], back[c], back[b], back[a]))
            except ValueError:
                pass
    uv = bm.loops.layers.uv.new("UVMap")
    for f, keys in faces_front:
        for loop, k in zip(f.loops, keys):
            loop[uv].uv = uv_of[k]
    for f in bm.faces:
        if f.loops[0][uv].uv.length == 0:
            for loop in f.loops:
                co = loop.vert.co
                loop[uv].uv = (crop["x0"] + crop["sx"] * (co.x + 0.5), crop["y0"] + crop["sy"] * co.z)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    nm = unique_name(name or os.path.splitext(os.path.basename(path))[0][:40] or "Image3D")
    me = bpy.data.meshes.new(nm)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(nm, me)
    link_to_collection(obj)
    sm = obj.modifiers.new("Lissage", "SMOOTH")
    sm.factor = 0.6
    sm.iterations = 4
    add_subsurf(obj, 1)
    me.shade_smooth()
    obj.scale = (height, height, height)
    _image_material(obj, img)
    set_json_prop(obj, "se_image3d", {"source": path, "method": "gonflage de silhouette", "mask": method})
    return obj, method


def _image_material(obj, img):
    mat = bpy.data.materials.new(f"{obj.name}_image")
    if mat.node_tree is None:
        mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    img.pack()
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.6
    obj.data.materials.clear()
    obj.data.materials.append(mat)


def visual_hull(engine, front: str, side: str, top: str | None = None, height: float = 1.0, resolution: int = 72,
                name: str | None = None, threshold: float = 0.12):
    n = resolution
    fm, fimg, fcrop, _, _ = load_mask(front, n, threshold)   # fm[z, x]
    sm, simg, _, _, _ = load_mask(side, n, threshold)        # sm[z, y] (profil vu de la droite : y vers l'avant)
    tm = load_mask(top, n, threshold)[0] if top else None    # tm[y, x]
    # vox[x, y, z]
    vox = fm.T[:, None, :] & sm.T[None, :, :]
    if tm is not None:
        vox &= tm.T[:, :, None]
    if vox.sum() < 30:
        raise CommandError("Les silhouettes ne se recoupent pas : vérifiez que les vues sont cadrées pareil.")
    # faces frontières du volume de voxels
    verts = {}
    bm = bmesh.new()

    def V(x, y, z):
        k = (x, y, z)
        if k not in verts:
            verts[k] = bm.verts.new(((x / n) - 0.5, (y / n) - 0.5, z / n))
        return verts[k]
    padded = np.pad(vox, 1)
    xs, ys, zs = np.nonzero(vox)
    for x, y, z in zip(xs, ys, zs):
        px, py, pz = x + 1, y + 1, z + 1
        if not padded[px - 1, py, pz]:
            bm.faces.new((V(x, y, z), V(x, y, z + 1), V(x, y + 1, z + 1), V(x, y + 1, z)))
        if not padded[px + 1, py, pz]:
            bm.faces.new((V(x + 1, y, z), V(x + 1, y + 1, z), V(x + 1, y + 1, z + 1), V(x + 1, y, z + 1)))
        if not padded[px, py - 1, pz]:
            bm.faces.new((V(x, y, z), V(x + 1, y, z), V(x + 1, y, z + 1), V(x, y, z + 1)))
        if not padded[px, py + 1, pz]:
            bm.faces.new((V(x, y + 1, z), V(x, y + 1, z + 1), V(x + 1, y + 1, z + 1), V(x + 1, y + 1, z)))
        if not padded[px, py, pz - 1]:
            bm.faces.new((V(x, y, z), V(x, y + 1, z), V(x + 1, y + 1, z), V(x + 1, y, z)))
        if not padded[px, py, pz + 1]:
            bm.faces.new((V(x, y, z + 1), V(x + 1, y, z + 1), V(x + 1, y + 1, z + 1), V(x, y + 1, z + 1)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    nm = unique_name(name or "Reconstruction")
    me = bpy.data.meshes.new(nm)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(nm, me)
    link_to_collection(obj)
    rm = obj.modifiers.new("Remaillage", "REMESH")
    rm.mode = "VOXEL"
    rm.voxel_size = 1.2 / n
    sm_mod = obj.modifiers.new("Lissage", "SMOOTH")
    sm_mod.factor = 0.8
    sm_mod.iterations = 6
    apply_modifiers(obj)
    obj.data.shade_smooth()
    # projection de la vue de face comme texture (UV planaires XZ)
    me = obj.data
    uvl = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uvl.data[li].uv = (fcrop["x0"] + fcrop["sx"] * (co.x + 0.5), fcrop["y0"] + fcrop["sy"] * co.z)
    _image_material(obj, fimg)
    obj.scale = (height, height, height)
    set_json_prop(obj, "se_image3d", {"source": [front, side, top], "method": "enveloppe visuelle (silhouettes)"})
    return obj


@command("image_to_model", label="Image vers 3D")
def cmd_image_to_model(engine, p: Params):
    """Image(s) → 3D par méthode géométrique (PAS une IA neuronale). `image` seule : gonflage de silhouette texturé.
    `images` [face, profil, (dessus)] : enveloppe visuelle par silhouettes. Image sur fond uni ou transparent.
    `backend` : nom d'un moteur IA local configuré (facultatif) pour une vraie reconstruction neuronale."""
    if p.has("backend"):
        from ..ai_backends import run_backend
        return run_backend(engine, p.str("backend"), p.str("image", None) or p.list("images")[0], p.str("name", None))
    height = p.float("height", 1.0, lo=0.01, hi=1000)
    res = p.int("resolution", 96, lo=24, hi=256)
    thr = p.float("threshold", 0.12, lo=0.01, hi=1.0)
    if p.has("images") and len(p.list("images")) >= 2:
        ims = [str(x) for x in p.list("images")]
        obj = visual_hull(engine, ims[0], ims[1], ims[2] if len(ims) > 2 else None, height, min(res, 128),
                          p.str("name", None), thr)
        method = "enveloppe visuelle à partir de silhouettes (géométrique, non neuronale)"
    else:
        obj, mask = inflate_from_image(engine, p.str("image"), height, p.float("depth", 0.5, lo=0.01, hi=5), res,
                                       p.str("name", None), thr)
        method = f"gonflage de silhouette ({mask}) — géométrique, non neuronal : la profondeur est estimée par la forme"
    select_only([obj])
    return {"name": obj.name, "method": method, "vertices": len(obj.data.vertices),
            "_label": f"Image → 3D : {obj.name}"}
