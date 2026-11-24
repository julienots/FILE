# Gemini Dev

App Android : tu choisis un dépôt GitHub, tu écris ce que tu veux, Gemini lit/écrit les fichiers
et fait le commit + push tout seul. Les actions s'affichent en direct façon terminal.

## Obtenir l'APK (sans Android Studio)

1. Crée un dépôt GitHub (ex : `gemini-dev`) et envoie-y tout le contenu de ce dossier.
2. Onglet **Actions** → **Build APK** (se lance seul au push, ou « Run workflow »).
3. Quand c'est vert, ouvre l'exécution → **Artifacts** → télécharge `GeminiDev-apk` → installe `app-debug.apk`.

## Première ouverture (une seule fois)

- Token GitHub (bouton dans l'app, case « repo » cochée d'office).
- Clé Gemini gratuite (bouton dans l'app → AI Studio → Create API key).
- Modèle : `gemini-3.8-flash` par défaut (gratuit). `gemini-4-argon` = API payante uniquement.
