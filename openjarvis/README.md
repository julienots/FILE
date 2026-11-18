# OpenJarvis — mise en place

[OpenJarvis](https://github.com/open-jarvis/OpenJarvis) (Stanford, Apache 2.0) est un assistant IA
personnel qui tourne **en local** sur ta machine, avec Ollama comme moteur de modèles.

## Installation (une commande)

```bash
bash openjarvis/install.sh
```

Le script : installe `uv`, clone OpenJarvis dans `~/OpenJarvis`, installe ses dépendances,
installe Ollama et le modèle `qwen3.5:4b`, puis copie `config.toml` dans `~/.openjarvis/`
(l'ancienne config est sauvegardée en `config.toml.bak`).

Variables optionnelles : `OPENJARVIS_DIR` (dossier d'installation), `OPENJARVIS_MODEL` (ex. `qwen3.5:9b`).

Windows natif : `irm https://open-jarvis.github.io/OpenJarvis/install.ps1 | iex`,
ou l'appli de bureau depuis les [releases](https://github.com/open-jarvis/OpenJarvis/releases/latest).

## Utilisation

```bash
cd ~/OpenJarvis
uv run jarvis                 # chat dans le terminal
uv run jarvis ask "Bonjour"   # question unique
uv run jarvis gui             # interface web (Node.js 22+)
uv run jarvis serve           # API compatible OpenAI sur http://127.0.0.1:8000
uv run jarvis doctor          # diagnostic
```

Autres presets : `uv run jarvis init --preset code-assistant --force`
(`deep-research`, `scheduled-monitor`, `morning-digest-linux`…).

## Utiliser un modèle cloud à la place d'Ollama

```bash
uv sync --extra server --extra inference-cloud
export ANTHROPIC_API_KEY=...   # ou OPENAI_API_KEY, GEMINI_API_KEY, OPENROUTER_API_KEY
```
