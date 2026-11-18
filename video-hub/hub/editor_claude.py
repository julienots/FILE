"""Montage par Claude en relais (EDITOR_MODE=claude, recommandé).

Le hub envoie à l'API Claude : les liens des rushs, la catégorie, le guide de style et vos retouches.
Claude se connecte au serveur MCP CUTTINX (MCP connector de l'API), regarde les vidéos, juge le style
et les coupes, crée le projet, vérifie un rendu brouillon puis lance le rendu final. Il termine en
appelant l'outil `report_result`, que le hub lit pour enchaîner.
"""
from __future__ import annotations

import logging
from typing import Callable

import anthropic

from .config import Settings

log = logging.getLogger("hub.editor.claude")

MCP_NAME = "cuttinx"

REPORT_TOOL = {
    "name": "report_result",
    "description": (
        "À appeler UNE fois, à la toute fin, quand le rendu FINAL est terminé (get_render indique que l'export "
        "est fini). Transmet au hub les identifiants nécessaires pour récupérer le MP4 et publier."
    ),
    "input_schema": {
        "type": "object",
        "properties": {
            "success": {"type": "boolean", "description": "true si le rendu final existe"},
            "project_id": {"type": "string"},
            "render_job_id": {"type": "string"},
            "render_media_id": {"type": "string", "description": "id du média rendu (list_media kind=render)"},
            "download_url": {
                "type": "string",
                "description": "URL directe du MP4 si get_render en fournit une, sinon chaîne vide",
            },
            "duration_seconds": {"type": "number"},
            "summary": {
                "type": "string",
                "description": "2-4 phrases : ce que montre la vidéo, les figures/moments clés, le style de montage. "
                "Sert à écrire la légende Instagram.",
            },
            "error": {"type": "string", "description": "si success=false : ce qui a bloqué"},
        },
        "required": ["success", "project_id", "render_job_id", "render_media_id", "download_url", "summary"],
        "additionalProperties": False,
    },
    "eager_input_streaming": True,
}

SYSTEM = """Tu es le monteur vidéo automatique d'un créateur de contenu (street workout / calisthenics, vlogs).
Tu travailles SANS humain dans la boucle : personne ne répondra à tes questions, décide toi-même.
Tu as accès à CUTTINX (éditeur vidéo Android du créateur) via ses outils MCP.

Méthode CUTTINX :
1. import_from_url pour chaque lien fourni (rushs, musique) -> tu obtiens des media_id.
2. view_storyboard sur chaque rush pour REGARDER le contenu (puis start/end resserrés pour trouver les bornes exactes
   des moments forts). find_highlights / detect_scenes / analyze_audio aident à repérer.
3. Si une musique est fournie (ou si tu en choisis une parmi list_media type=audio) : analyze_music -> BPM, beats,
   downbeats, drops. Cale les fins de clips sur des beats (durée timeline = (end-start)/speed ; une transition fait
   chevaucher deux clips de sa durée).
4. list_effects une fois pour connaître les effets et transitions disponibles.
5. create_project avec la recette -> render_project quality "draft" -> get_render -> view_storyboard du rendu pour
   vérifier -> corrige avec create_project + project_id (remplace le projet) si besoin -> render_project quality
   "final" avec la résolution demandée -> get_render jusqu'à la fin de l'export (relance get_render tant que ce n'est
   pas fini).

Règles :
- Nomme le projet EXACTEMENT avec le nom imposé dans la demande (il sert au hub à retrouver le fichier rendu).
- Ne supprime rien, ne modifie pas les médias de l'utilisateur.
- Respecte le format (aspect) demandé.
- Termine TOUJOURS en appelant l'outil report_result (success=false avec l'erreur si c'est impossible,
  par exemple si le téléphone n'est pas connecté)."""


def build_prompt(settings: Settings, video: dict, raw_url: str, music_url: str | None, style_guide: str) -> str:
    project_name = f"HUB-{video['id']}"
    lines = [
        f"Nouvelle vidéo à monter. Nom de projet imposé : {project_name}",
        f"Catégorie : {'PERFORMANCE (street workout / calisthenics)' if video['category'] == 'performance' else 'NORMALE (vlog / divers)'}",
        f"Format de sortie : {video['target_aspect']}",
        f"Rendu final : quality \"final\", resolution {settings.render_resolution}",
        f"Rush à importer : {raw_url}",
    ]
    if music_url:
        lines.append(f"Musique fournie à importer : {music_url}")
    elif video["category"] == "performance" and settings.default_music_media_id:
        lines.append(f"Musique par défaut déjà dans CUTTINX : media_id {settings.default_music_media_id}")
    elif video["category"] == "performance":
        lines.append("Aucune musique fournie : choisis-en une adaptée dans list_media type=audio si disponible.")
    if video.get("notes"):
        lines.append(f"Notes du créateur : {video['notes']}")
    if video.get("edit_feedback"):
        lines.append(
            f"RETOUCHES DEMANDÉES sur le montage précédent (projet {video.get('cuttinx_project_id') or 'inconnu'}, "
            f"remplace-le avec project_id) : {video['edit_feedback']}"
        )
    lines += ["", "Guide de style à suivre :", style_guide.strip()]
    return "\n".join(lines)


async def edit_with_claude(
    settings: Settings,
    video: dict,
    raw_url: str,
    music_url: str | None,
    style_guide: str,
    on_progress: Callable[[str], None] | None = None,
) -> dict:
    if not settings.cuttinx_mcp_url:
        raise RuntimeError("CUTTINX_MCP_URL n'est pas configuré.")
    client = anthropic.AsyncAnthropic(api_key=settings.anthropic_api_key or None)

    mcp_server: dict = {"type": "url", "url": settings.cuttinx_mcp_url, "name": MCP_NAME}
    if settings.cuttinx_mcp_token:
        mcp_server["authorization_token"] = settings.cuttinx_mcp_token

    betas = ["mcp-client-2025-11-20"]
    extra: dict = {}
    if settings.claude_fallbacks:
        betas.append("server-side-fallback-2026-07-01")
        extra["fallbacks"] = "default"

    messages: list[dict] = [
        {"role": "user", "content": build_prompt(settings, video, raw_url, music_url, style_guide)}
    ]

    for turn in range(settings.claude_max_continuations + 1):
        async with client.beta.messages.stream(
            model=settings.claude_model,
            max_tokens=64000,
            system=SYSTEM,
            thinking={"type": "adaptive", "display": "summarized"},
            output_config={"effort": settings.claude_edit_effort},
            mcp_servers=[mcp_server],
            tools=[{"type": "mcp_toolset", "mcp_server_name": MCP_NAME}, REPORT_TOOL],
            messages=messages,
            betas=betas,
            **extra,
        ) as stream:
            response = await stream.get_final_message()

        tool_calls = [b.name for b in response.content if b.type == "mcp_tool_use"]
        if on_progress and tool_calls:
            on_progress(f"Claude/CUTTINX (tour {turn + 1}) : {', '.join(tool_calls)}")

        if response.stop_reason == "refusal":
            raise RuntimeError("Claude a refusé la demande de montage.")

        report = next((b for b in response.content if b.type == "tool_use" and b.name == "report_result"), None)
        if report is not None:
            data = report.input if isinstance(report.input, dict) else {}
            missing = [k for k in REPORT_TOOL["input_schema"]["required"] if k not in data]
            if missing:
                raise RuntimeError(f"Rapport de montage incomplet (champs manquants : {missing}).")
            if not data.get("success"):
                raise RuntimeError(f"Claude n'a pas pu monter la vidéo : {data.get('error') or 'raison inconnue'}")
            return data

        if response.stop_reason in ("pause_turn", "max_tokens"):
            # Reprise : on renvoie la réponse telle quelle, le serveur reprend là où il en était.
            messages.append({"role": "assistant", "content": response.content})
            if response.stop_reason == "max_tokens":
                messages.append({"role": "user", "content": "Continue, puis termine par report_result."})
            continue

        # Claude a fini sans appeler report_result : on le lui redemande une fois.
        messages.append({"role": "assistant", "content": response.content})
        messages.append(
            {
                "role": "user",
                "content": "Tu n'as pas appelé report_result. Termine le rendu final si besoin, "
                "puis appelle report_result.",
            }
        )

    raise RuntimeError("Montage Claude interrompu : trop de reprises sans rapport final.")
