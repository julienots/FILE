---
name: ship-check
description: Vérification complète avant de publier ou livrer un projet (APK/Play Store, site, jeu web, release GitHub). Utiliser quand l'utilisateur dit « c'est prêt ? », « on publie », « release », « je l'envoie », ou à la fin d'un gros chantier.
---

# Ship check

Exécuter réellement chaque point possible ; marquer ✅ / ❌ / ⏭️ (non vérifiable ici, avec la raison).

## Fonctionnel
- Build release OK, tests OK (donner la sortie en cas d'échec).
- Parcours principal testé de bout en bout, captures à l'appui.
- Cas limites : premier lancement, hors-ligne, données vides, rotation/redimensionnement, retour arrière.

## Qualité
- Aucune erreur console / crash dans les logs.
- Perf : démarrage rapide, 60 FPS sur les animations, taille du build raisonnable.
- Passe `ui-polish` faite ; textes relus (fautes, langue cohérente).

## Sécurité & légal
- Aucun secret dans le code ou l'historique (clés, keystore, mots de passe, `.env`).
- Licences des assets/polices/sons respectées et créditées.
- Permissions demandées = strict nécessaire ; politique de confidentialité si données collectées.

## Publication
- Version et changelog à jour, README clair (installation, captures).
- Store : icône, captures, description courte + longue, mots-clés.

Finir par : verdict (prêt / pas prêt), liste des ❌ classés par gravité.
