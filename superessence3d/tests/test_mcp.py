"""Le serveur MCP expose bien les outils demandés, avec des schémas valides."""
import asyncio

REQUIRED = ["get_scene_info", "list_objects", "get_object_details", "create_mesh", "create_character", "create_creature",
            "create_building", "create_organic_model", "create_material", "create_texture", "modify_geometry",
            "sculpt_object", "apply_modifier", "create_uv_map", "create_rig", "create_animation", "create_camera",
            "create_lighting", "render_scene", "export_model", "import_model", "delete_object", "duplicate_object",
            "save_project", "load_project", "inspect_model", "optimize_mesh"]


def test_tools_registered():
    from superessence import mcp_server
    tools = asyncio.run(mcp_server.mcp.list_tools())
    names = {t.name for t in tools}
    missing = [n for n in REQUIRED if n not in names]
    assert not missing, missing
    for t in tools:
        schema = getattr(t, "input_schema", None) or getattr(t, "inputSchema", None)
        assert schema and schema.get("type") == "object"
        assert (t.description or "").strip()


def test_every_tool_maps_to_engine_command():
    """Chaque outil MCP appelle une commande moteur existante (pas d'outil fictif)."""
    import inspect
    import re

    from superessence import mcp_server
    from superessence.engine import register_all
    from superessence.engine.core import COMMANDS
    register_all()
    src = inspect.getsource(mcp_server)
    called = set(re.findall(r'call\("(\w+)"', src)) | set(re.findall(r'_gen\("(\w+)"', src))
    unknown = [c for c in called if c not in COMMANDS]
    assert not unknown, unknown
