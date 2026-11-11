"""Moteur SUPERESSENCE : toutes les opérations sur Blender (bpy)."""

_registered = False


def register_all() -> None:
    """Importe les modules qui déclarent des commandes (@command)."""
    global _registered
    if _registered:
        return
    from . import (ai_backends, animation, io_ops, library, materials, mesh_data, modeling, project, render,  # noqa: F401
                   rigging, scene_ops, sculpt, uv)
    from .generators import image3d  # noqa: F401
    from . import generators  # noqa: F401
    _registered = True
