# Prompts pour un jeu de qualité

## Prompt de départ
```
Crée un jeu [genre] original : [pitch en 1 phrase].
Plateforme : [web HTML5 / Android Compose / Godot].
Applique le skill game-polish : mini GDD d'abord, puis prototype,
test réel avec captures, autocritique /10 et au moins 3 itérations.
```

## Si tu as un prompt de ChatGPT
```
Voici un prompt de jeu (de ChatGPT). Applique game-polish :
transforme-le en mini GDD, corrige ce qui est flou, puis construis-le.
<colle le prompt ici>
```

## Relances après une première version
- « Ajoute 10 améliorations de game feel, une par une, avec avant/après. »
- « Joue 5 parties via Playwright et liste ce qui est frustrant, ennuyeux ou confus. »
- « Passe de direction artistique : palette, fond, ombres, lumière, cohérence. »
- « Équilibre la difficulté : quand un joueur moyen perd-il ? Ajuste. »
- « Critique comme un testeur de jeux indé : qu'est-ce qui fait amateur ? Corrige. »

## Économiser des tokens
- Une demande précise par message vaut mieux que « améliore tout ».
- `/compact` quand la conversation devient longue ; `/clear` pour changer de sujet.
- Nommer le fichier/la fonction à modifier évite que Claude explore tout.
