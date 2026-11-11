# Relais Claude ↔ SUPERESSENCE 3D Mobile (Supabase)

Ce relais permet à l'application **Claude sur téléphone** de piloter **SUPERESSENCE 3D Mobile** sur ce même téléphone, sans ordinateur, sur le même principe que CUTTINX :

```
Claude (connecteur MCP) ──HTTPS──▶ fonction Supabase « superessence » ◀──HTTPS── application SUPERESSENCE (ouverte, « Connecté à Claude »)
```

L'offre gratuite de Supabase suffit. Vous pouvez réutiliser un projet existant (celui de CUTTINX par exemple) : les tables portent le préfixe `se_` et n'entrent pas en conflit.

## Installation (une seule fois, depuis le navigateur, sans programmer)

1. **Table** : supabase.com › votre projet › **SQL Editor** › *New query*. Collez le contenu de `relais.sql`, puis **Run**.
2. **Fonction** : **Edge Functions** › *Deploy a new function* › *Via Editor*.
   - Nom : `superessence` (exactement).
   - Remplacez tout le code par le contenu de `functions/superessence/index.ts`, puis **Deploy**.
3. **Désactivez la vérification JWT** : ouvrez la fonction `superessence` › *Details* (ou *Settings*) › décochez **Enforce JWT verification** (« Verify JWT ») › **Save**. Claude n'envoie pas de jeton Supabase : sans cette étape, il reçoit une erreur 401.
4. Copiez l'adresse de la fonction : `https://VOTRE-PROJET.supabase.co/functions/v1/superessence`.

Avec la CLI, au lieu des étapes 2 et 3 : `supabase functions deploy superessence --no-verify-jwt`.

## Dans l'application

1. SUPERESSENCE 3D › menu **⋮** › **✦ Claude**.
2. Collez l'adresse de la fonction, puis touchez **Créer l'adresse du connecteur**.
3. Touchez **Copier**. Dans claude.ai ou l'app Claude : **Paramètres › Connecteurs › Ajouter un connecteur personnalisé**, avec le nom `SUPERESSENCE` et l'URL copiée.
4. Touchez **Activer**, pour passer en « Connecté à Claude ».
5. Dans une conversation Claude, activez le connecteur, puis demandez par exemple : « Crée un dragon noir avec quatre ailes et des yeux rouges, fais-lui battre des ailes et montre-moi le résultat ».

**Gardez l'application SUPERESSENCE ouverte** pendant que Claude travaille. Android met en pause les applications en arrière-plan. Le plus pratique est l'écran partagé, ou une tablette.

## Outils disponibles pour Claude (15)
`get_scene_info`, `generate_model`, `modify_model`, `set_material`, `transform_object`, `animate`, `stop_animation`, `add_shape`, `duplicate_object`, `delete_object` (avec confirmation), `set_lighting`, `view_scene` (renvoie une image de la vue), `save_to_library`, `export_model` (vous choisissez où enregistrer le fichier), `undo`.

## Sécurité
- L'adresse du connecteur contient un secret de 32 caractères propre à votre téléphone. Ne la partagez pas. En cas de doute, touchez **Régénérer** et mettez à jour le connecteur dans Claude.
- Les tables ont la sécurité RLS activée, sans règle publique : seule la fonction, avec la clé de service côté serveur, peut les lire.
- Les modèles 3D restent sur le téléphone. Seules les commandes, les descriptions de scène et les images d'aperçu demandées passent par le relais. Les commandes de plus de 24 h sont supprimées automatiquement.
