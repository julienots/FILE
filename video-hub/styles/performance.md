# Style — Vidéo PERFORMANCE (street workout / calisthenics)

Objectif : un Reel 9:16 qui arrête le scroll dès la 1re seconde et met en valeur les figures.

- **Durée cible** : 15 à 35 s. Mieux vaut court et intense que long.
- **Ouverture** : commencer DIRECTEMENT par le moment le plus impressionnant (ou un teaser de 1-2 s de la figure la plus
  dure), jamais par une mise en place ou un temps mort.
- **Sélection** : garder uniquement les répétitions propres et les figures réussies (muscle-up, front lever, planche,
  human flag, handstand, dips, tractions explosives...). Couper les préparations, les ratés et les pauses.
- **Rythme** : coupes calées sur les beats/downbeats de la musique (analyze_music). Accélérer légèrement (speed 1.1-1.3)
  les passages de transition, garder la vitesse réelle sur les figures.
- **Ralentis** : speed_ramp (1.0 -> 0.35-0.5 -> 1.0) sur le pic de chaque figure clé (blocage du front lever, passage du
  muscle-up, tenue de la planche). Placer le drop de la musique sur la meilleure figure.
- **Zooms** : zoom léger (1.0 -> 1.10-1.15, easing smooth) sur les tenues statiques ; punch zoom sur les impacts.
- **Transitions** : dynamiques mais lisibles — flash_zoom, punch_zoom, whip, zoom_blur, glitch_slide (0.2-0.35 s).
  Pas plus d'une transition « effet » toutes les 2-3 coupes ; coupes franches sinon.
- **Effets** : look propre et contrasté (film_look ou teal_orange léger, clarity). beat_flash ou shake ponctuels sur les
  drops seulement. Pas d'effets gadgets (hearts, confetti...).
- **Son** : musique énergique (drill, phonk, trap, électro) ; son d'origine baissé (volume 0.1-0.3) sauf s'il apporte
  quelque chose (souffle, impact).
- **Texte** : facultatif, 1 texte court en haut au début (ex. nom de la figure ou « 1er muscle-up strict ») en police
  bebas neue / anton, blanc avec contour noir, anim pop. Jamais de texte qui cache le corps.
- **Cadrage** : en 9:16, centrer l'athlète (pos_x/pos_y) ; ne jamais couper la tête ou les pieds pendant une figure.
