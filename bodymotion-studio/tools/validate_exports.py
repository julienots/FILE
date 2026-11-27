"""Validation des exports avec Blender (bpy) : import de chaque fichier, contrôle des animations, rendu d'une image.
Usage : python (avec bpy) tools/validate_exports.py <dossier_exports> <dossier_rendus>
Produit aussi blender_mannequin.fbx (FBX binaire exporté par Blender) pour tester l'import FBX de l'application."""
import bpy, sys, os, mathutils
src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def render(name, frame):
    sc = bpy.context.scene
    sc.frame_set(frame)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
    cam.location = (1.8, -3.4, 1.4)
    cam.rotation_euler = (mathutils.Vector((0, 0, 0.9)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
    l = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN")); l.data.energy = 3; l.rotation_euler = (0.8, 0.2, 0.6); sc.collection.objects.link(l)
    sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 8
    sc.world = bpy.data.worlds.new('w'); sc.world.color = (0.3, 0.3, 0.32)
    sc.render.resolution_x = 320; sc.render.resolution_y = 420
    sc.render.filepath = os.path.join(out, name + ".png")
    bpy.ops.render.render(write_still=True)

def report(fn):
    arms = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    acts = [(a.name, tuple(round(x) for x in a.frame_range)) for a in bpy.data.actions]
    print(f"[{fn}] armatures={[(a.name, len(a.data.bones)) for a in arms]} actions={acts}")
    return arms

files = sorted(os.listdir(src))
for fn in files:
    p = os.path.join(src, fn); ext = fn.split('.')[-1]
    if fn.startswith("blender_"): continue
    reset()
    try:
        if ext in ("glb", "gltf"): bpy.ops.import_scene.gltf(filepath=p)
        elif ext == "fbx": bpy.ops.import_scene.fbx(filepath=p)
        elif ext == "bvh": bpy.ops.import_anim.bvh(filepath=p, global_scale=0.01)
        else: continue
    except Exception as e:
        print(f"[{fn}] ECHEC import : {e}"); continue
    arms = report(fn)
    # Pour les exports sans maillage, on matérialise les os pour le rendu.
    if not any(o.type == 'MESH' for o in bpy.data.objects) and arms:
        for a in arms: a.data.display_type = 'STICK'; a.show_in_front = True
        import bmesh
        for b in arms[0].pose.bones:
            bpy.ops.mesh.primitive_uv_sphere_add(radius=0.03)
            s = bpy.context.active_object
            c = s.constraints.new('COPY_LOCATION'); c.target = arms[0]; c.subtarget = b.name
    render(fn.replace('.', '_'), 15)

# FBX de référence exporté par Blender pour le test d'import
reset()
bpy.ops.import_scene.gltf(filepath=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "app", "src", "main", "assets", "mannequin.glb"))
bpy.ops.export_scene.fbx(filepath=os.path.join(src, "blender_mannequin.fbx"), use_selection=False, add_leaf_bones=False, bake_anim=False)
print("FBX de référence écrit")
