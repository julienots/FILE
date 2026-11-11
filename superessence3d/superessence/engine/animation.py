"""Animation : images clés, cycles procéduraux (marche, course, saut, attaque, vol, battement d'ailes,
ondulation de queue, respiration, tentacules...) et animation d'objets.

Les cycles sont additifs : plusieurs mouvements peuvent être combinés dans une même action
(« fais battre les ailes, bouger la queue et respirer le torse »). Ils sont écrits sous forme
de vraies images clés Blender, exportables en glTF/FBX.
"""
from __future__ import annotations

import math
from collections import defaultdict

import bpy

from .core import CommandError, Params, command
from .util import get_object, ensure_object_mode

MOTIONS = ["walk", "run", "jump", "attack", "fly", "wing_flap", "tail_wave", "breathe", "idle", "head_look",
           "tentacle_wave", "swim", "slither", "roar"]
MOTION_LABELS = {"walk": "Marche", "run": "Course", "jump": "Saut", "attack": "Attaque", "fly": "Vol",
                 "wing_flap": "Battement d'ailes", "tail_wave": "Queue", "breathe": "Respiration",
                 "idle": "Repos", "head_look": "Regard", "tentacle_wave": "Tentacules", "swim": "Nage",
                 "slither": "Reptation", "roar": "Rugissement"}
OBJECT_MOTIONS = ["spin", "bounce", "float", "pulse", "sway"]


def _bones_by_role(arm):
    roles = defaultdict(list)
    for pb in arm.pose.bones:
        r = pb.get("se_role") or _role_from_name(pb.name)
        roles[r].append(pb)
    return roles


def _role_from_name(n: str) -> str:
    """Devine le rôle d'un os d'après son nom (squelettes importés)."""
    s = n.lower()
    side = "L" if any(k in s for k in (".l", "_l", "left", "gauche", ".g")) else (
        "R" if any(k in s for k in (".r", "_r", "right", "droit", ".d")) else "")
    if "wing" in s or "aile" in s:
        return f"wing_{side}_1"
    if "tail" in s or "queue" in s:
        return "tail_1"
    if "head" in s or "tête" in s or "tete" in s:
        return "head"
    if "neck" in s or "cou" in s:
        return "neck"
    if "thigh" in s or "cuisse" in s or "upleg" in s:
        return f"leg_{side}_upper"
    if "shin" in s or "calf" in s or "tibia" in s or ("leg" in s and "up" not in s):
        return f"leg_{side}_lower"
    if "upperarm" in s or "bras" in s and "avant" not in s:
        return f"arm_{side}_upper"
    if "forearm" in s or "avant-bras" in s:
        return f"arm_{side}_lower"
    if "spine" in s or "colonne" in s or "chest" in s or "torse" in s:
        return "chest" if ("chest" in s or "torse" in s) else "spine"
    return ""


def _chain_index(role: str) -> int:
    try:
        return int(role.rsplit("_", 1)[1])
    except (IndexError, ValueError):
        return 1


def _motion_channels(motion: str, roles: dict, t: float, amp: float):
    """Renvoie {(os, canal, axe): valeur} pour la phase t ∈ [0,1)."""
    out = {}
    w = 2 * math.pi * t
    S = math.sin

    def rot(pb, axis, v):
        key = (pb.name, "rotation_euler", axis)
        out[key] = out.get(key, 0.0) + v

    def loc(pb, axis, v):
        key = (pb.name, "location", axis)
        out[key] = out.get(key, 0.0) + v

    def scl(pb, axis, v):
        key = (pb.name, "scale", axis)
        out[key] = out.get(key, 0.0) + v

    def each(prefix):
        for r, pbs in roles.items():
            if r.startswith(prefix):
                for pb in pbs:
                    yield r, pb

    if motion in ("walk", "run"):
        k = 1.0 if motion == "walk" else 1.7
        a = math.radians(22) * amp * k
        for r, pb in each("leg_"):
            side = 1 if "_L_" in r else -1
            front = "_front_" in r or ("_front_" not in r and "_back_" not in r)
            phase = 0 if front else math.pi
            phase += 0 if side == 1 else math.pi
            if r.endswith("_upper"):
                rot(pb, 0, a * S(w + phase))
            elif r.endswith("_lower"):
                rot(pb, 0, -abs(a) * 0.8 * max(0.0, S(w + phase + 1.2)))
            elif r.endswith("_foot"):
                rot(pb, 0, a * 0.4 * S(w + phase - 0.8))
        for r, pb in each("arm_"):
            side = 1 if "_L_" in r else -1
            if r.endswith("_upper"):
                rot(pb, 0, -a * 0.7 * S(w + (0 if side == 1 else math.pi)))
            elif r.endswith("_lower"):
                rot(pb, 0, -abs(a) * 0.4)
        for pb in roles.get("root", []):
            loc(pb, 1, 0.0)
            loc(pb, 2, 0.02 * amp * k * abs(S(2 * w)))  # rebond
        for r, pb in each("tail_"):
            rot(pb, 2, math.radians(8) * amp * S(w + _chain_index(r) * 0.6))
        for pb in roles.get("head", []):
            rot(pb, 0, math.radians(3) * amp * S(2 * w))
        for pb in roles.get("spine", []):
            rot(pb, 1, math.radians(3) * amp * k * S(w))
    elif motion == "jump":
        # accroupi -> impulsion -> vol -> réception
        h = max(0.0, S(math.pi * (t - 0.2) / 0.6)) if 0.2 <= t <= 0.8 else 0.0
        crouch = max(0.0, S(math.pi * t / 0.2)) if t < 0.2 else (max(0.0, S(math.pi * (t - 0.8) / 0.2)) if t > 0.8 else 0)
        for pb in roles.get("root", []):
            loc(pb, 2, 0.8 * amp * h - 0.08 * crouch)
        for r, pb in each("leg_"):
            if r.endswith("_upper"):
                rot(pb, 0, math.radians(35) * crouch * amp + math.radians(20) * h * amp)
            elif r.endswith("_lower"):
                rot(pb, 0, -math.radians(60) * crouch * amp - math.radians(30) * h * amp)
        for r, pb in each("arm_"):
            if r.endswith("_upper"):
                rot(pb, 0, -math.radians(60) * h * amp)
        for r, pb in each("wing_"):
            rot(pb, 0, math.radians(35) * h * amp * (1 if "_L_" in r else -1))
    elif motion in ("attack", "roar"):
        # préparation 0..0.35, frappe 0.35..0.5, retour
        if t < 0.35:
            e = S(math.pi / 2 * t / 0.35)
        elif t < 0.5:
            e = 1 - 2.2 * (t - 0.35) / 0.15
        else:
            e = -1.2 * (1 - (t - 0.5) / 0.5)
        e *= amp
        for pb in roles.get("head", []) + roles.get("neck", []):
            rot(pb, 0, math.radians(-25 if motion == "attack" else -15) * e)
        for pb in roles.get("jaw", []):
            rot(pb, 0, math.radians(30) * abs(e))
        for pb in roles.get("chest", []) + roles.get("spine", []):
            rot(pb, 0, math.radians(-8) * e)
        for r, pb in each("arm_"):
            if r.endswith("_upper"):
                rot(pb, 0, math.radians(-70) * e if "_R_" in r else math.radians(-20) * e)
            elif r.endswith("_lower"):
                rot(pb, 0, math.radians(-40) * max(0, e))
        for r, pb in each("wing_"):
            rot(pb, 0, math.radians(30) * abs(e) * (1 if "_L_" in r else -1))
        for r, pb in each("tail_"):
            rot(pb, 2, math.radians(15) * e * S(_chain_index(r)))
    elif motion in ("wing_flap", "fly"):
        k = 1.0 if motion == "wing_flap" else 1.2
        for r, pb in each("wing_"):
            side = 1 if "_L_" in r else -1
            i = _chain_index(r)
            rot(pb, 0, side * math.radians(38) * amp * k * S(w - 0.5 * (i - 1)))
            rot(pb, 2, math.radians(6) * amp * S(w + math.pi / 2))
        if motion == "fly":
            for pb in roles.get("root", []):
                loc(pb, 2, 0.12 * amp * S(w + math.pi))
            for r, pb in each("leg_"):
                if r.endswith("_upper"):
                    rot(pb, 0, math.radians(-40) * amp)
                elif r.endswith("_lower"):
                    rot(pb, 0, math.radians(50) * amp)
            for r, pb in each("tail_"):
                rot(pb, 0, math.radians(5) * amp * S(w + _chain_index(r) * 0.7))
    elif motion in ("tail_wave", "slither", "swim"):
        a = math.radians(14 if motion == "tail_wave" else 18) * amp
        for r, pb in each("tail_"):
            i = _chain_index(r)
            rot(pb, 2, a * S(w - i * 0.8))
            if motion == "tail_wave":
                rot(pb, 0, a * 0.3 * S(w * 0.5 - i * 0.5))
        if motion in ("slither", "swim"):
            for pb in roles.get("head", []):
                rot(pb, 2, a * 0.5 * S(w + 0.8))
            for r, pb in each("fin_"):
                rot(pb, 0, a * S(w * 2))
    elif motion == "tentacle_wave":
        for r, pb in each("tentacle_"):
            parts = r.split("_")
            try:
                k, i = int(parts[1]), int(parts[2])
            except (IndexError, ValueError):
                k, i = 0, 1
            rot(pb, 0, math.radians(18) * amp * S(w - i * 0.9 + k * 1.3))
            rot(pb, 2, math.radians(12) * amp * S(w * 0.5 - i * 0.6 + k * 2.1))
    elif motion == "breathe":
        b = 0.5 + 0.5 * S(w)
        for pb in roles.get("chest", []) or roles.get("spine", [])[:1]:
            scl(pb, 0, 0.05 * amp * b)
            scl(pb, 2, 0.05 * amp * b)
            rot(pb, 0, math.radians(-2) * amp * b)
        for pb in roles.get("head", []):
            rot(pb, 0, math.radians(1.5) * amp * b)
        for r, pb in each("shoulder_"):
            loc(pb, 1, 0.005 * amp * b)
    elif motion == "idle":
        for key, v in _motion_channels("breathe", roles, t, amp * 0.8).items():
            out[key] = out.get(key, 0.0) + v
        for pb in roles.get("head", []):
            rot(pb, 2, math.radians(6) * amp * S(w))
        for r, pb in each("tail_"):
            rot(pb, 2, math.radians(6) * amp * S(w - _chain_index(r) * 0.6))
        for r, pb in each("wing_"):
            rot(pb, 0, math.radians(3) * amp * S(w) * (1 if "_L_" in r else -1))
    elif motion == "head_look":
        for pb in roles.get("head", []):
            rot(pb, 2, math.radians(35) * amp * S(w))
            rot(pb, 0, math.radians(8) * amp * S(2 * w))
        for pb in roles.get("neck", []):
            rot(pb, 2, math.radians(15) * amp * S(w))
    return out


def _fcurves(action, obj=None):
    """F-curves d'une action (API « slotted actions » de Blender 4.4+/5.x, avec repli)."""
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    out = []
    for layer in action.layers:
        for strip in layer.strips:
            for slot in action.slots:
                cb = strip.channelbag(slot)
                if cb is not None:
                    out.extend(cb.fcurves)
    return out


def make_cyclic(action) -> None:
    for fc in _fcurves(action):
        if not any(m.type == "CYCLES" for m in fc.modifiers):
            fc.modifiers.new("CYCLES")


@command("create_animation", label="Créer une animation")
def cmd_create_animation(engine, p: Params):
    """Crée une animation réelle (images clés Blender).

    - Squelette : `motions` combinables parmi walk, run, jump, attack, fly, wing_flap, tail_wave, breathe, idle,
      head_look, tentacle_wave, swim, slither, roar. `amplitude` (1 = normal), `frames` (durée du cycle),
      `cycles` (répétitions), `speed`.
    - Objet sans squelette : `object_motion` spin, bounce, float, pulse, sway.
    - Clés personnalisées : `keyframes` [{frame, location?, rotation?(degrés), scale?, bone?}].
    Le nom `name` peut désigner le modèle, son maillage ou directement l'armature."""
    ensure_object_mode()
    obj = get_object(p.str("name"))
    sc = bpy.context.scene
    start = p.int("start", sc.frame_start, lo=0, hi=1_000_000)
    frames = p.int("frames", 0, lo=0, hi=10000)
    cycles = p.int("cycles", 1, lo=1, hi=100)
    amp = p.float("amplitude", 1.0, lo=0, hi=5)
    speed = p.float("speed", 1.0, lo=0.1, hi=10)
    action_name = p.str("action_name", None)
    result = {"object": obj.name}

    if p.has("keyframes"):
        result.update(_custom_keyframes(obj, p.list("keyframes")))
        return {**result, "_label": f"Clés {obj.name}"}

    if p.has("object_motion"):
        motion = p.str("object_motion", choices=OBJECT_MOTIONS)
        frames = frames or 48
        _object_motion(obj, motion, start, int(frames / speed), cycles, amp)
        sc.frame_end = max(sc.frame_end, start + int(frames / speed) * cycles)
        return {**result, "motion": motion, "frames": [start, start + int(frames / speed) * cycles],
                "_label": f"Animation {motion}"}

    arm = _find_armature(obj)
    if arm is None:
        raise CommandError("Aucun squelette trouvé. Créez-le d'abord (create_rig), ou utilisez object_motion.")
    motions = [str(m) for m in p.list("motions", ["idle"])]
    for m in motions:
        if m not in MOTIONS:
            raise CommandError(f"Mouvement inconnu : « {m} ». Choix : {', '.join(MOTIONS)}")
    default_len = {"walk": 32, "run": 20, "jump": 36, "attack": 30, "roar": 40, "fly": 24, "wing_flap": 24,
                   "tail_wave": 40, "breathe": 60, "idle": 72, "head_look": 96, "tentacle_wave": 48,
                   "swim": 40, "slither": 40}
    frames = frames or max(default_len[m] for m in motions)
    frames = max(4, int(round(frames / speed)))
    roles = _bones_by_role(arm)
    used_roles = sorted(r for r in roles if r)
    if not arm.animation_data:
        arm.animation_data_create()
    if p.bool("replace", True) or arm.animation_data.action is None:
        act = bpy.data.actions.new(action_name or f"{arm.name}_{'+'.join(motions)}")
        arm.animation_data.action = act
    act = arm.animation_data.action
    for pb in arm.pose.bones:
        pb.rotation_mode = "XYZ"
        pb.location = (0, 0, 0)
        pb.rotation_euler = (0, 0, 0)
        pb.scale = (1, 1, 1)
    step = 2 if frames >= 16 else 1
    keyed = set()
    total = frames * cycles
    for f in range(0, total + 1, step):
        t = (f % frames) / frames if f < total else 0.0
        ch = {}
        for m in motions:
            for k, v in _motion_channels(m, roles, t, amp).items():
                ch[k] = ch.get(k, 0.0) + v
        for (bname, path, axis), v in ch.items():
            pb = arm.pose.bones[bname]
            vec = list(getattr(pb, path))
            base = 1.0 if path == "scale" else 0.0
            vec[axis] = base + v
            setattr(pb, path, vec)
            pb.keyframe_insert(path, index=axis, frame=start + f)
            keyed.add((bname, path, axis))
        for (bname, path, axis) in keyed - set(ch):
            pb = arm.pose.bones[bname]
            vec = list(getattr(pb, path))
            vec[axis] = 1.0 if path == "scale" else 0.0
            setattr(pb, path, vec)
            pb.keyframe_insert(path, index=axis, frame=start + f)
    if not keyed:
        raise CommandError("Aucun os du squelette ne correspond à ces mouvements "
                           f"(rôles disponibles : {', '.join(used_roles) or 'aucun'}).")
    if motions != ["jump"] and motions != ["attack"] and p.bool("loop", True):
        make_cyclic(act)
    sc.frame_start = min(sc.frame_start, start)
    sc.frame_end = start + total
    sc.frame_set(start)
    try:
        act.use_fake_user = True
    except AttributeError:
        pass
    return {**result, "armature": arm.name, "action": act.name, "motions": motions,
            "frames": [start, start + total], "animated_channels": len(keyed),
            "_label": f"Animation {', '.join(MOTION_LABELS[m] for m in motions)}"}


def _find_armature(obj):
    if obj.type == "ARMATURE":
        return obj
    name = obj.get("se_armature")
    if name and name in bpy.data.objects:
        return bpy.data.objects[name]
    arm = obj.find_armature() if obj.type == "MESH" else None
    if arm:
        return arm
    for c in obj.children_recursive:
        if c.type == "ARMATURE":
            return c
        if c.type == "MESH" and c.find_armature():
            return c.find_armature()
    root = obj
    while root.parent is not None:
        root = root.parent
        name = root.get("se_armature")
        if name and name in bpy.data.objects:
            return bpy.data.objects[name]
    return None


def _object_motion(obj, motion, start, frames, cycles, amp):
    base_loc = obj.location.copy()
    base_rot = obj.rotation_euler.copy()
    base_scale = obj.scale.copy()
    total = frames * cycles
    step = max(1, frames // 24)
    for f in range(0, total + 1, step):
        t = (f % frames) / frames if f < total else 0.0
        w = 2 * math.pi * t
        obj.location, obj.rotation_euler, obj.scale = base_loc.copy(), base_rot.copy(), base_scale.copy()
        if motion == "spin":
            obj.rotation_euler.z = base_rot.z + 2 * math.pi * (f / frames) * amp
            obj.keyframe_insert("rotation_euler", index=2, frame=start + f)
            continue
        if motion == "bounce":
            obj.location.z = base_loc.z + abs(math.sin(w)) * 0.5 * amp
            obj.keyframe_insert("location", index=2, frame=start + f)
        elif motion == "float":
            obj.location.z = base_loc.z + math.sin(w) * 0.15 * amp
            obj.rotation_euler.z = base_rot.z + math.sin(w) * 0.1 * amp
            obj.keyframe_insert("location", index=2, frame=start + f)
            obj.keyframe_insert("rotation_euler", index=2, frame=start + f)
        elif motion == "pulse":
            s = 1 + 0.08 * amp * math.sin(w)
            obj.scale = base_scale * s
            obj.keyframe_insert("scale", frame=start + f)
        elif motion == "sway":
            obj.rotation_euler.y = base_rot.y + math.sin(w) * 0.12 * amp
            obj.keyframe_insert("rotation_euler", index=1, frame=start + f)
    obj.location, obj.rotation_euler, obj.scale = base_loc, base_rot, base_scale
    if obj.animation_data and obj.animation_data.action and motion != "spin":
        make_cyclic(obj.animation_data.action)
    elif obj.animation_data and obj.animation_data.action:
        for fc in _fcurves(obj.animation_data.action):
            fc.extrapolation = "LINEAR"
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"


def _custom_keyframes(obj, keys):
    count = 0
    arm = obj if obj.type == "ARMATURE" else _find_armature(obj)
    for k in keys:
        if not isinstance(k, dict) or "frame" not in k:
            raise CommandError("Chaque clé doit être un objet avec « frame ».")
        f = int(k["frame"])
        target = obj
        if k.get("bone"):
            if arm is None or k["bone"] not in arm.pose.bones:
                raise CommandError(f"Os introuvable : {k.get('bone')}")
            target = arm.pose.bones[k["bone"]]
            target.rotation_mode = "XYZ"
        if "location" in k:
            target.location = [float(x) for x in k["location"]]
            target.keyframe_insert("location", frame=f)
            count += 1
        if "rotation" in k:
            target.rotation_euler = [math.radians(float(x)) for x in k["rotation"]]
            target.keyframe_insert("rotation_euler", frame=f)
            count += 1
        if "scale" in k:
            target.scale = [float(x) for x in k["scale"]]
            target.keyframe_insert("scale", frame=f)
            count += 1
    frames = [int(k["frame"]) for k in keys]
    sc = bpy.context.scene
    if frames:
        sc.frame_end = max(sc.frame_end, max(frames))
    return {"keyframes": count, "frames": [min(frames), max(frames)] if frames else []}


@command("insert_keyframe", label="Insérer une clé")
def cmd_insert_keyframe(engine, p: Params):
    """Insère une image clé sur la position/rotation/échelle courante d'un objet (ou d'un os) à l'image donnée."""
    obj = get_object(p.str("name"))
    frame = p.int("frame", bpy.context.scene.frame_current)
    target = obj
    if p.has("bone"):
        arm = obj if obj.type == "ARMATURE" else _find_armature(obj)
        if arm is None or p.str("bone") not in arm.pose.bones:
            raise CommandError("Os introuvable.")
        target = arm.pose.bones[p.str("bone")]
    paths = [str(x) for x in p.list("channels", ["location", "rotation_euler", "scale"])]
    for path in paths:
        if path not in ("location", "rotation_euler", "scale"):
            raise CommandError(f"Canal invalide : {path}")
        target.keyframe_insert(path, frame=frame)
    return {"object": obj.name, "frame": frame, "channels": paths}


@command("clear_animation", label="Effacer l'animation", destructive=True)
def cmd_clear_animation(engine, p: Params):
    """Supprime l'animation d'un objet (ou de son squelette). Exige confirm=true."""
    obj = get_object(p.str("name"))
    targets = [obj]
    arm = _find_armature(obj)
    if arm and arm not in targets:
        targets.append(arm)
    p.confirm(f"effacer l'animation de {', '.join(t.name for t in targets)}")
    for t in targets:
        if t.animation_data:
            t.animation_data.action = None
        if t.type == "ARMATURE":
            for pb in t.pose.bones:
                pb.location = (0, 0, 0)
                pb.rotation_euler = (0, 0, 0)
                pb.scale = (1, 1, 1)
    return {"cleared": [t.name for t in targets]}


@command("list_actions", history=False)
def cmd_list_actions(engine, p: Params):
    """Actions (animations) du projet."""
    return {"actions": [{"name": a.name, "frame_range": list(a.frame_range), "users": a.users}
                        for a in bpy.data.actions],
            "motions": [{"id": m, "label": MOTION_LABELS[m]} for m in MOTIONS], "object_motions": OBJECT_MOTIONS}
