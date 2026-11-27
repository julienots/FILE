---
name: qualite-jeu-mobile
description: À utiliser à chaque passe et avant toute livraison d'un jeu ou d'une appli mobile Super Essence Studio. Applique une grille de qualité complète (plantages, graphismes, mécaniques, contrôles, performances, sauvegardes, finitions, publication) et lance les tests automatiques possibles.
---

# Qualité jeu mobile

Un projet n'est livré que si chaque ligne ci-dessous est validée. Une ligne ratée déclenche une passe de correction.

## Grille
1. Stabilité : aucun plantage au lancement à froid et à chaud, au changement d'écran, en pause, à la rotation, au retour d'un appel, au retour système.
2. Graphismes : aucun texte coupé, aucun élément hors écran, aucun scintillement, aucune texture manquante. Vérifier 360x640, 390x844 et 412x915 avec encoche et barre de gestes.
3. Mécaniques : boucle claire en 30 secondes, aucune impasse de progression, difficulté équilibrée, aucune exploitation évidente. Jouer une partie complète de 10 minutes, puis une partie volontairement tordue.
4. Contrôles : cibles tactiles d'au moins 48 dp, aucun geste caché, jouable d'une main si le genre s'y prête.
5. Performance : viser 60 images par seconde, repli à 30, pas de chauffe excessive. Les mesures se font sur un téléphone d'entrée de gamme.
6. Données : sauvegarde jamais corrompue, version de sauvegarde, reprise après arrêt brutal.
7. Son : volumes équilibrés, bouton muet, reprise après coupure audio.
8. Finitions : écran de chargement, états vides, messages d'erreur lisibles, textes relus, aucun bouton mort.
9. Identité : intro Super Essence Studio présente (skill splash-super-essence), style cohérent, aucun look IA générique.
10. Publication : icône, captures, description, politique de confidentialité, classification. Vérifier les exigences en cours de Google Play sur sa documentation.

## Tests automatiques pour un jeu en navigateur
Si Playwright et un navigateur sont disponibles dans l'environnement :
```python
from playwright.sync_api import sync_playwright
errors = []
with sync_playwright() as p:
    b = p.chromium.launch()
    for w, h in [(360,640),(390,844),(412,915)]:
        pg = b.new_page(viewport={'width':w,'height':h}, has_touch=True)
        pg.on('console', lambda m: errors.append(m.text) if m.type == 'error' else None)
        pg.on('pageerror', lambda e: errors.append(str(e)))
        pg.goto('file:///chemin/index.html')
        pg.wait_for_timeout(3000)
        pg.screenshot(path=f'shot_{w}x{h}.png')
    b.close()
print(errors or 'Aucune erreur console')
```
Regarder chaque capture et corriger tout texte coupé ou élément mal placé. Si le navigateur ne se lance pas, le dire et fournir la liste de tests à faire à la main.

## Android
Relire le code pour les cas classiques : fuite de mémoire, travail lourd sur le fil principal, permissions manquantes, rotation, service tué par le système. Sans téléphone dans l'environnement, demander à l'utilisateur de tester sur appareil, avec la liste exacte des gestes à essayer.

## Rapport final
Rendre un tableau court : ligne de la grille, validé ou non, ce qui reste à corriger. Ne jamais déclarer « sans bug » : dire ce qui a été testé et ce qui ne l'a pas été.
