# Shaders personnalisés (`.agrshader`)

Un shader personnalisé est un fichier texte de code GLSL **branché sur le rendu PBR** du moteur. Vous ne
réécrivez pas l'éclairage, les ombres, le ciel, le brouillard ou le post-traitement : vous modifiez la
surface (couleur, rugosité, émission…) et, si vous le souhaitez, la position des sommets. Le même
fichier fonctionne en OpenGL, OpenGL ES (Android) et Vulkan.

## Exemple

```glsl
// Paramètres : réglables dans l'inspecteur de chaque matériau qui utilise ce shader.
param color tint = (1.0, 0.55, 0.25, 1.0);
param float speed = 2.0 range(0, 10);
param texture noise = "assets://fx/noise.png";

// Facultatif : déplace les sommets (espace monde). Aussi appliqué à la passe d'ombre.
void vertex(inout Vertex v) {
    v.position += v.normal * sin(TIME * speed + v.position.y * 6.0) * 0.02;
}

// Facultatif : modifie la surface avant l'éclairage.
void surface(inout Surface s) {
    float n = texture(noise, s.uv * 4.0).r;
    s.baseColor *= tint.rgb;
    s.emissive += tint.rgb * n * (0.5 + 0.5 * sin(TIME * speed));
}
```

Au moins une des deux fonctions `surface` ou `vertex` doit exister. Les autres fonctions (aides,
constantes) sont autorisées.

## Ce que reçoivent les fonctions

| `Surface s` (fragment) | Rôle |
|---|---|
| `baseColor` (vec3), `alpha` | couleur linéaire et opacité, déjà multipliées par le matériau, sa texture et la couleur de l'objet |
| `metallic`, `roughness` | déjà calculés à partir du matériau (bornés ensuite à 0..1 et 0,045..1) |
| `normal` | normale monde (normal map comprise) ; renormalisée après l'appel |
| `emissive` (vec3), `occlusion` | lumière émise (ajoutée après l'éclairage), occlusion ambiante |
| `worldPos`, `viewDir`, `uv`, `color` | lecture : position monde, direction vers la caméra, UV, couleur de l'objet |

| `Vertex v` (sommet) | Rôle |
|---|---|
| `position`, `normal` | position et normale **monde** (après la transformation de l'objet et le squelette) |
| `uv`, `color` | coordonnées de texture, couleur de l'objet |
| `localPosition` | lecture : position dans le modèle |

`TIME` donne le temps en secondes (horloge de rendu, qui avance aussi en mode édition). Les fonctions
GLSL standard (`sin`, `mix`, `texture`, `smoothstep`…) sont disponibles.

## Paramètres

```glsl
param float  name [= valeur] [range(min, max)];
param vec2   name [= (x, y)];
param vec3   name [= (x, y, z)];   // une seule valeur : copiée dans chaque composante
param vec4   name [= (x, y, z, w)];
param color  name [= (r, g, b[, a])];          // sRGB comme toutes les couleurs de l'éditeur
param texture name [= "assets://chemin.png"] [srgb];  // srgb : image en couleur ; sinon données linéaires
```

- 8 paramètres de valeur et 4 textures au maximum par shader.
- Dans l'inspecteur d'un matériau, chaque paramètre a le bon widget (curseur si `range`, sélecteur de
  couleur, glisser-déposer d'image). Une valeur modifiée n'est enregistrée que dans ce matériau ;
  clic droit → *Revenir à la valeur du shader*.
- Noms interdits : mots-clés et fonctions GLSL (`texture`, `color`…), préfixes du moteur (`u_`, `gl_`,
  `agr`…), `surface`, `vertex`, `TIME`.
- Les textures ne sont lisibles que dans `surface` (et dans les fonctions qu'elle appelle).

## Utiliser un shader

1. Explorateur d'assets → **Nouveau shader** : crée un fichier à partir d'un modèle commenté et
   l'ouvre dans l'éditeur.
2. Dans un matériau `.agrmat` (cliquer le fichier), champ **Shader** : glisser le `.agrshader` ou
   taper son chemin. Les paramètres apparaissent dessous.
3. Glisser le matériau sur un objet de la scène.

Format du matériau : `"shader"`, `"shaderParams"` (valeurs modifiées) et `"shaderTextures"`. Les
matériaux sans shader gardent exactement l'ancien format.

## Éditeur de shaders

Panneau **Shader** (clic sur un `.agrshader`, ou *Affichage › Éditeur de shaders*) :

- code avec numéros de ligne, police à chasse fixe, tabulation ;
- **aperçu en direct** : 0,35 s après la dernière frappe, le code est recompilé et appliqué à la
  scène et à la sphère d'aperçu, sans écrire le fichier ;
- erreurs du parseur et du compilateur GPU **avec leur ligne dans votre fichier** (surlignée dans la
  marge) ; cliquer une erreur place le curseur sur la ligne ;
- *Enregistrer* (Ctrl+S) / *Annuler les modifications* ; fermer le panneau, ouvrir un autre shader,
  changer de projet ou quitter avec des modifications non enregistrées demande d'abord quoi faire.

Un fichier modifié hors de l'éditeur est rechargé automatiquement (comme les matériaux).

## Fonctionnement et erreurs

- Le moteur assemble le shader PBR, des `#define` vers les paramètres (`u_custom[i]`, textures en
  unités 7 à 10), puis votre code après `#line 1 1` : les messages du compilateur (Mesa, glslang,
  pilotes) portent vos numéros de ligne.
- Chaque étage est compilé sans la fonction de l'autre (`surface` n'est pas compilé dans le shader
  de sommet), ce qui permet `discard`, `dFdx`… dans `surface`.
- Les variantes (opaque / découpe / fondu, simple ou double face, squelette, ombre) sont compilées
  à la première utilisation, puis gardées jusqu'à la prochaine modification du fichier.
- **Un shader qui ne compile pas ne casse rien** : ses matériaux sont dessinés avec le shader standard
  et l'erreur est affichée (éditeur, inspecteur du matériau, console).

## Limites actuelles

- Shaders de surface pour le rendu 3D PBR uniquement : pas de shaders pour les sprites, l'interface,
  le ciel, l'eau, le terrain ni le post-traitement ; pas d'éditeur de shaders par nœuds (graphe).
- La passe d'ombre applique `vertex` mais pas `surface` : un shader qui découpe des trous dans
  `surface` garde une ombre pleine (utiliser le mode de transparence *Découpe* du matériau).
- Les boîtes englobantes ne suivent pas les déplacements de `vertex` : garder des déplacements petits
  devant la taille de l'objet, sinon il peut disparaître en bord d'écran.
- Pas de vérification statique avant compilation GPU au-delà des paramètres et des déclarations
  interdites : l'éditeur affiche les messages du pilote tels quels (en anglais).
- L'aperçu de l'éditeur utilise les valeurs par défaut des paramètres (les valeurs d'un matériau
  s'affichent dans l'aperçu du matériau).
