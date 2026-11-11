"""Analyse des descriptions textuelles (français / anglais) en spécifications de génération.

Python pur (sans bpy) : testable indépendamment. Ce n'est PAS un modèle de langage :
c'est un analyseur à règles transparent. Le résultat (spec) est toujours affiché à
l'utilisateur, qui peut le corriger. Claude, via MCP, peut fournir directement une
spec structurée plus précise que l'analyse automatique.
"""
from __future__ import annotations

import re

from ..colors import NAMED, normalize, parse_color

NUMBERS = {
    "un": 1, "une": 1, "deux": 2, "trois": 3, "quatre": 4, "cinq": 5, "six": 6, "sept": 7, "huit": 8, "neuf": 9,
    "dix": 10, "onze": 11, "douze": 12, "quinze": 15, "vingt": 20, "one": 1, "two": 2, "three": 3, "four": 4,
    "five": 5, "six_en": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10, "eleven": 11, "twelve": 12,
    "twenty": 20, "a": 1, "an": 1, "double": 2, "triple": 3, "paire": 2, "pair": 2,
}

# nom de caractéristique -> mots-clés (formes normalisées sans accents)
FEATURES = {
    "wings": ["aile", "ailes", "wing", "wings"],
    "horns": ["corne", "cornes", "horn", "horns", "bois_cerf", "antler", "antlers"],
    "tails": ["queue", "queues", "tail", "tails"],
    "tentacles": ["tentacule", "tentacules", "tentacle", "tentacles"],
    "heads": ["tete", "tetes", "head", "heads"],
    "eyes": ["oeil", "yeux", "eye", "eyes", "œil"],
    "legs": ["patte", "pattes", "jambe", "jambes", "leg", "legs"],
    "arms": ["bras", "arm", "arms"],
    "spikes": ["pointe", "pointes", "pique", "piques", "epine", "epines", "spike", "spikes", "pic", "pics",
               "crete", "dorsal", "dorsales"],
    "scales": ["ecaille", "ecailles", "scale", "scales", "scaly", "ecailleux", "ecailleuse"],
    "claws": ["griffe", "griffes", "claw", "claws", "serre", "serres"],
    "fangs": ["croc", "crocs", "dent", "dents", "fang", "fangs", "teeth", "tooth"],
    "fur": ["fourrure", "poil", "poils", "fur", "furry", "poilu"],
    "feathers": ["plume", "plumes", "feather", "feathers"],
    "shell": ["carapace", "shell", "coquille"],
    "fins": ["nageoire", "nageoires", "fin", "fins"],
    "antennae": ["antenne", "antennes", "antenna", "antennae"],
    "mouth": ["gueule", "bouche", "mouth", "jaw", "machoire"],
}

CATEGORIES = {
    "creature": {
        "dragon": ["dragon", "dragons", "wyverne", "wyvern", "drake"],
        "hydra": ["hydre", "hydra"],
        "kraken": ["kraken", "pieuvre", "octopus", "poulpe"],
        "demon": ["demon", "diable", "devil", "imp", "diablotin"],
        "golem": ["golem", "elementaire", "elemental", "titan"],
        "griffin": ["griffon", "griffin", "gryphon", "hippogriffe"],
        "spider_monster": ["arachnide"],
        "blob": ["slime", "blob", "gelee", "limon"],
        "alien": ["alien", "extraterrestre", "xeno"],
        "beast": ["monstre", "monster", "bete", "beast", "creature", "chimere", "chimera", "abomination",
                  "behemoth", "minotaure", "minotaur", "cerbere", "cerberus", "dinosaure", "dinosaur", "raptor",
                  "trex", "basilic", "basilisk", "manticore", "loup-garou", "werewolf"],
    },
    "character": {
        "humanoid": ["personnage", "humain", "homme", "femme", "guerrier", "guerriere", "chevalier", "knight", "mage",
                     "sorcier", "sorciere", "wizard", "witch", "soldat", "soldier", "elfe", "elf", "nain", "dwarf",
                     "orc", "orque", "gobelin", "goblin", "zombie", "squelette", "skeleton", "heros", "hero",
                     "heroine", "character", "person", "man", "woman", "warrior", "viking", "ninja", "pirate",
                     "samourai", "samurai", "roi", "king", "reine", "queen", "paladin", "archer", "voleur", "thief",
                     "enfant", "child", "garcon", "fille", "boy", "girl", "humanoide", "humanoid", "troll", "ogre",
                     "vampire", "momie", "mummy", "robot", "androide", "android", "cyborg", "astronaute", "astronaut"],
    },
    "animal": {
        "wolf": ["loup", "wolf", "chien", "dog", "renard", "fox", "hyene", "hyena", "chacal", "coyote"],
        "cat": ["chat", "cat", "lion", "tigre", "tiger", "panthere", "panther", "leopard", "lynx", "jaguar",
                "guepard", "cheetah", "puma"],
        "horse": ["cheval", "horse", "poney", "pony", "licorne", "unicorn", "zebre", "zebra", "ane", "donkey"],
        "bear": ["ours", "bear", "panda", "grizzly"],
        "deer": ["cerf", "deer", "biche", "elan", "moose", "renne", "reindeer", "antilope", "chevre", "goat"],
        "cow": ["vache", "cow", "taureau", "bull", "bison", "buffle", "cochon", "pig", "sanglier", "boar",
                "rhinoceros", "hippopotame", "elephant", "mouton", "sheep"],
        "rabbit": ["lapin", "rabbit", "lievre", "hare", "souris", "mouse", "rat", "ecureuil", "squirrel"],
        "bird": ["oiseau", "bird", "aigle", "eagle", "corbeau", "crow", "raven", "hibou", "owl", "chouette",
                 "faucon", "falcon", "pigeon", "perroquet", "parrot", "phenix", "phoenix", "poule", "chicken",
                 "canard", "duck", "cygne", "swan", "pingouin", "penguin"],
        "fish": ["poisson", "fish", "requin", "shark", "dauphin", "dolphin", "baleine", "whale", "carpe", "saumon"],
        "snake": ["serpent", "snake", "cobra", "python", "vipere", "viper", "anguille", "eel", "ver", "worm"],
        "spider": ["araignee", "spider", "scorpion", "insecte", "insect", "fourmi", "ant", "scarabee", "beetle",
                   "mante", "mantis", "guepe", "wasp", "crabe", "crab"],
        "lizard": ["lezard", "lizard", "crocodile", "alligator", "salamandre", "iguane", "gecko", "tortue", "turtle",
                   "grenouille", "frog", "crapaud", "toad"],
    },
    "building": {
        "house": ["maison", "house", "cabane", "hut", "chalet", "cottage", "ferme", "farm", "villa", "grange", "barn"],
        "tower": ["tour", "tower", "phare", "lighthouse", "minaret", "beffroi", "clocher"],
        "castle": ["chateau", "castle", "forteresse", "fortress", "fort", "citadelle", "citadel", "palais", "palace"],
        "temple": ["temple", "eglise", "church", "cathedrale", "cathedral", "sanctuaire", "shrine", "pyramide",
                   "pyramid", "mausolee", "chapelle", "chapel"],
        "modern": ["immeuble", "building", "gratte-ciel", "skyscraper", "bureau", "office", "appartement",
                   "apartment", "hopital", "hospital", "usine", "factory", "entrepot", "warehouse"],
        "shop": ["boutique", "shop", "magasin", "store", "taverne", "tavern", "auberge", "inn", "forge",
                 "smithy", "moulin", "mill", "boulangerie"],
    },
    "vehicle": {
        "car": ["voiture", "car", "automobile", "jeep", "buggy", "taxi", "sportive", "berline", "voiture_de_course"],
        "truck": ["camion", "truck", "fourgon", "van", "bus", "camionnette"],
        "spaceship": ["vaisseau", "spaceship", "starship", "chasseur", "fighter", "navette", "shuttle", "ovni", "ufo",
                      "croiseur", "cruiser", "fusee", "rocket"],
        "boat": ["bateau", "boat", "navire", "ship", "voilier", "sailboat", "barque", "galion", "galleon",
                 "sous-marin", "submarine", "yacht", "radeau", "raft"],
        "plane": ["avion", "plane", "airplane", "jet", "planeur", "glider", "biplan", "helicoptere", "helicopter",
                  "drone", "dirigeable", "airship"],
        "tank": ["tank", "char", "blinde", "armored", "mecha", "mech"],
        "bike": ["moto", "motorcycle", "motorbike", "velo", "bicycle", "scooter", "quad"],
    },
    "weapon": {
        "sword": ["epee", "sword", "katana", "sabre", "saber", "glaive", "rapiere", "rapier", "claymore",
                  "excalibur", "lame", "blade", "cimeterre", "scimitar"],
        "dagger": ["dague", "dagger", "poignard", "couteau", "knife", "kunai"],
        "axe": ["hache", "axe", "hachette", "francisque"],
        "spear": ["lance", "spear", "javelot", "javelin", "trident", "hallebarde", "halberd", "pique_arme"],
        "hammer": ["marteau", "hammer", "masse", "mace", "maillet", "fleau", "flail", "gourdin", "club"],
        "bow": ["arc", "bow", "arbalete", "crossbow"],
        "staff": ["baton", "staff", "sceptre", "scepter", "baguette", "wand", "canne"],
        "shield": ["bouclier", "shield", "ecu", "pavois"],
        "scythe": ["faux", "scythe", "faucille", "sickle"],
        "gun": ["pistolet", "pistol", "gun", "fusil", "rifle", "blaster", "laser", "canon", "cannon", "revolver",
                "mitrailleuse", "arme_a_feu"],
    },
    "prop": {
        "barrel": ["tonneau", "barrel", "baril", "fut"],
        "crate": ["caisse", "crate", "boite", "box", "carton"],
        "chest": ["coffre", "chest", "tresor", "treasure"],
        "table": ["table", "bureau_meuble", "desk", "etabli"],
        "chair": ["chaise", "chair", "tabouret", "stool", "trone", "throne", "fauteuil", "armchair", "banc", "bench"],
        "lamp": ["lampe", "lamp", "lanterne", "lantern", "torche", "torch", "bougie", "candle", "chandelier", "lampadaire"],
        "bottle": ["potion", "bouteille", "bottle", "fiole", "flask", "vase", "jarre", "jar", "amphore", "pot",
                   "coupe", "chalice", "calice", "gobelet", "cup", "urne", "urn"],
        "rock": ["rocher", "rock", "pierre_objet", "boulder", "caillou", "galet", "falaise"],
        "tree": ["arbre", "tree", "chene", "oak", "sapin", "pine", "palmier", "palm", "saule", "willow", "bouleau"],
        "bush": ["buisson", "bush", "arbuste", "plante", "plant", "fougere", "fern", "fleur", "flower", "herbe_touffe"],
        "mushroom": ["champignon", "mushroom", "toadstool"],
        "crystal": ["cristal", "crystal", "gemme", "gem", "diamant", "diamond", "joyau", "jewel", "rubis", "emeraude_g"],
        "well": ["puits", "well", "fontaine", "fountain"],
        "fence": ["cloture", "fence", "barriere", "palissade"],
        "crown": ["couronne", "crown", "diademe", "tiara", "anneau", "ring", "bague"],
    },
    "scenery": {
        "terrain": ["terrain", "paysage", "landscape", "montagne", "mountain", "colline", "hill", "plaine", "plain",
                    "desert", "vallee", "valley", "canyon", "ile", "island", "sol_decor"],
        "forest": ["foret", "forest", "bois_foret", "jungle", "woods"],
        "ruins": ["ruine", "ruines", "ruins", "vestiges", "decombres", "cimetiere", "graveyard", "arene", "arena"],
        "dungeon": ["donjon", "dungeon", "crypte", "crypt", "souterrain", "catacombe", "catacombes", "grotte", "cave",
                    "caverne", "cavern", "salle"],
        "village": ["village", "hameau", "hamlet", "ville", "town", "city", "cite", "camp", "campement"],
    },
}

CATEGORY_LABELS = {"creature": "Créatures", "character": "Personnages", "animal": "Animaux", "building": "Bâtiments",
                   "vehicle": "Véhicules", "weapon": "Armes fictives", "prop": "Objets", "scenery": "Décors"}

STYLE_WORDS = {
    "lowpoly": ["low-poly", "lowpoly", "low_poly", "polygonal", "facette", "facettes", "facetted"],
    "cartoon": ["cartoon", "mignon", "cute", "stylise", "stylized", "chibi", "toon", "kawaii", "rigolo"],
    "realistic": ["realiste", "realistic", "detaille", "detaillee", "detailles", "detaillees", "detailed",
                  "photorealiste", "hd"],
}

MATERIAL_WORDS = [
    ("lava", ["lave", "lava", "magma", "volcanique", "volcanic", "en_fusion", "molten"]),
    ("ice", ["glace", "ice", "gele", "frozen", "givre"]),
    ("crystal", ["cristal", "crystal", "cristallin", "gemme", "gem", "diamant", "quartz", "amethyste"]),
    ("glass", ["verre", "glass", "transparent", "translucide"]),
    ("gold", ["or_matiere", "gold", "golden", "dore", "doree", "dores", "dorees"]),
    ("rust", ["rouille", "rouillee", "rouille", "rusty", "rust", "oxyde"]),
    ("scales", ["ecaille", "ecailles", "scales", "scaly", "ecailleux", "ecailleuse", "reptile", "reptilien"]),
    ("bark", ["ecorce", "bark"]),
    ("brick", ["brique", "briques", "brick", "bricks"]),
    ("leather", ["cuir", "leather"]),
    ("fabric", ["tissu", "fabric", "cloth", "laine", "wool", "velours", "velvet", "soie", "silk", "coton", "cotton",
                "toile", "textile"]),
    ("fur", ["fourrure", "fur", "poilu", "furry", "pelage", "duvet"]),
    ("wood", ["bois", "wood", "wooden", "boise", "chene_bois", "planche", "planks"]),
    ("stone", ["pierre", "stone", "roche", "rock", "granite", "marbre", "marble", "beton", "concrete", "rocheux",
               "rocky", "pave", "gres"]),
    ("organic", ["organique", "organiques", "organic", "chair", "flesh", "visqueux", "viscous", "veine", "veines",
                 "veins", "veineux", "gluant", "slimy", "mucus", "biologique", "alien_peau"]),
    ("skin", ["peau", "skin", "epiderme"]),
    ("metal", ["metal", "metallique", "metallic", "acier", "steel", "fer", "iron", "chrome", "argent_matiere",
               "aluminium", "titane", "titanium", "armure", "armor", "armour", "blinde"]),
    ("plastic", ["plastique", "plastic", "caoutchouc", "rubber", "vinyle"]),
    ("emission", ["neon", "luminescent", "phosphorescent", "fluorescent", "emissif", "emissive", "hologramme",
                  "holographic"]),
    ("ground", ["herbe", "grass", "gazon", "sol", "ground", "terre", "dirt", "boue", "mud", "mousse", "moss", "sable_sol"]),
]

GLOW_WORDS = ["lumineux", "lumineuse", "lumineux", "lumineuses", "brillant_lum", "glowing", "glow", "luminous",
              "incandescent", "incandescents", "flamboyant", "flamboyants", "ardent", "ardents", "brillent", "luisant",
              "luisants", "phosphorescent", "phosphorescents", "neon", "emissive"]
WET_WORDS = ["humide", "humides", "mouille", "mouillee", "wet", "moist", "visqueux", "visqueuse", "gluant", "slimy",
             "luisant", "luisante", "huileux", "oily", "suintant"]
ROUGH_WORDS = ["rugueux", "rugueuse", "rough", "mat", "mate", "matte", "brut", "brute", "sec", "seche", "dry", "poreux"]
SMOOTH_WORDS = ["lisse", "smooth", "poli", "polie", "polished", "brillant", "brillante", "shiny", "glossy", "laque",
                "vernis"]
BUMP_WORDS = ["relief", "reliefs", "bosse", "bosses", "bumpy", "creux", "rides", "ridé", "ride", "noueux", "craquele",
              "cracked", "fissure", "fissures", "strie", "stries"]
BIG_WORDS = ["geant", "geante", "giant", "enorme", "huge", "colossal", "colossale", "gigantesque", "massive",
             "massif", "grand", "grande", "big", "large", "immense"]
SMALL_WORDS = ["petit", "petite", "small", "tiny", "minuscule", "mini", "little", "nain_taille"]
THIN_WORDS = ["mince", "fin", "fine", "elance", "elancee", "maigre", "thin", "slender", "skinny", "squelettique"]
HEAVY_WORDS = ["massif", "massive", "trapu", "trapue", "costaud", "muscle", "musclee", "muscular", "bulky", "gros",
               "grosse", "fat", "lourd", "lourde", "robuste", "stocky", "imposant", "imposante"]


def _tokens(text: str) -> list[str]:
    t = normalize(text)
    t = re.sub(r"(\d)\s*,\s*(\d)", r"\1.\2", t)
    t = t.replace("'", " ").replace("’", " ")
    return re.findall(r"[a-z0-9]+(?:[\-\.][a-z0-9]+)*", t)


def _num(tok: str) -> int | None:
    if tok.isdigit():
        return int(tok)
    if tok == "six":
        return 6
    return NUMBERS.get(tok)


def _base(tok: str) -> str:
    """Forme de base : retire le pluriel et l'accord féminin courants (rouges -> rouge, dorees -> dore)."""
    for suf in ("ees", "es", "s", "e", "x"):
        if tok.endswith(suf) and len(tok) - len(suf) >= 3:
            return tok[: -len(suf)]
    return tok


def _match_word(tok: str, words: list[str]) -> bool:
    return tok in words or _base(tok) in words or tok.replace("-", "") in words


def _color_of(tok: str):
    if tok in NAMED:
        return NAMED[tok]
    b = _base(tok)
    if b in NAMED:
        return NAMED[b]
    t = tok[:-1] if tok.endswith("s") else tok
    for cand in (t, t[:-1] if t.endswith("e") else t, t[:-2] if t.endswith("tte") else t,
                 t[:-2] if t.endswith("lle") else t, t[:-3] + "c" if t.endswith("che") else t):
        if cand in NAMED:
            return NAMED[cand]
    if tok in ("dore", "doree", "dores", "dorees"):
        return NAMED["dore"]
    if tok in ("argente", "argentee", "argentes", "argentees"):
        return NAMED["argente"]
    return None


def _shade(words, idx, c):
    """Applique « foncé / clair » si présent juste après la couleur."""
    nxt = words[idx + 1] if idx + 1 < len(words) else ""
    prv = words[idx - 1] if idx > 0 else ""
    r, g, b = c[:3]
    if nxt in ("fonce", "foncee", "fonces", "foncees", "sombre", "sombres") or prv in ("dark", "deep"):
        return (r * 0.45, g * 0.45, b * 0.45)
    if nxt in ("clair", "claire", "clairs", "claires", "pale", "pales") or prv in ("light", "pale", "bright"):
        return (r + (1 - r) * 0.45, g + (1 - g) * 0.45, b + (1 - b) * 0.45)
    return (r, g, b)


def _find_category(words: list[str]) -> tuple[str, str, str | None]:
    best = None
    for i, w in enumerate(words):
        for cat, subs in CATEGORIES.items():
            for sub, keys in subs.items():
                if _match_word(w, keys) or w.replace("-", "_") in keys:
                    # le premier nom reconnu l'emporte (« un dragon avec des ailes de chauve-souris »)
                    score = i
                    if best is None or score < best[0]:
                        best = (score, cat, sub, w)
    if best is None:
        return "creature", "beast", None
    return best[1], best[2], best[3]


def parse_prompt(text: str) -> dict:
    """Description libre -> spécification de génération (dict JSON)."""
    words = _tokens(text)
    category, subtype, keyword = _find_category(words)
    spec: dict = {"prompt": text, "category": category, "subtype": subtype, "keyword": keyword,
                  "category_label": CATEGORY_LABELS[category], "features": {}, "colors": {}, "notes": []}
    feats = spec["features"]
    used = set()

    # --- taille ---------------------------------------------------------
    m = re.search(r"(\d+(?:\.\d+)?)\s*(m|metres?|meters?|mètres?|cm|centimetres?|mm|km)\b", normalize(text).replace(",", "."))
    if m:
        v = float(m.group(1))
        unit = m.group(2)
        factor = 0.01 if unit.startswith("c") else 0.001 if unit == "mm" else 1000 if unit == "km" else 1.0
        spec["height"] = round(min(max(v * factor, 0.01), 500.0), 3)
        spec["notes"].append(f"taille : {spec['height']} m")
    if any(_match_word(w, BIG_WORDS) for w in words):
        spec["size_hint"] = "big"
    elif any(_match_word(w, SMALL_WORDS) for w in words):
        spec["size_hint"] = "small"
    if any(_match_word(w, HEAVY_WORDS) for w in words):
        spec["build"] = "heavy"
    elif any(_match_word(w, THIN_WORDS) for w in words):
        spec["build"] = "thin"

    # --- caractéristiques avec nombre ------------------------------------
    for i, w in enumerate(words):
        for feat, keys in FEATURES.items():
            if not _match_word(w, keys):
                continue
            n = None
            # nombre juste avant (1 ou 2 mots : « quatre grandes ailes »)
            for back in (1, 2):
                if i - back >= 0:
                    n = _num(words[i - back])
                    if n is not None:
                        break
            plural = w.endswith("s") or w.endswith("x") or w in ("bras", "yeux")
            if n is None:
                n = {"wings": 2, "horns": 2, "eyes": 2, "legs": 4, "arms": 2, "tentacles": 6, "heads": 1,
                     "tails": 1, "fins": 3, "antennae": 2}.get(feat, 1)
                if feat in ("heads", "tails") and plural:
                    n = 2
            if feat in ("spikes", "scales", "claws", "fangs", "fur", "feathers", "shell", "mouth"):
                feats[feat] = True
            else:
                feats[feat] = int(min(n, 64))
            used.add(i)

    # « sans ailes », « without wings »
    for i, w in enumerate(words):
        if w in ("sans", "without", "no", "aucun", "aucune") and i + 1 < len(words):
            for j in (i + 1, i + 2):
                if j < len(words):
                    for feat, keys in FEATURES.items():
                        if _match_word(words[j], keys):
                            feats[feat] = 0 if feat not in ("spikes", "scales", "claws", "fangs", "fur",
                                                             "feathers", "shell", "mouth") else False

    # --- couleurs ---------------------------------------------------------
    eye_words = FEATURES["eyes"]
    vein_words = ["veine", "veines", "vein", "veins"]
    horn_words = FEATURES["horns"]
    wing_words = FEATURES["wings"]
    targets = {"eyes": eye_words, "veins": vein_words, "horns": horn_words, "wings": wing_words}
    body_colors = []
    for i, w in enumerate(words):
        c = _color_of(w)
        if c is None or w in ("or", "a", "an"):  # « or » est aussi une conjonction anglaise / française
            if w == "or" and i > 0 and words[i - 1] in ("en", "d", "de", "dore"):
                c = NAMED["or"]
            else:
                continue
        c = _shade(words, i, c)
        assigned = False
        # couleur juste après un nom (« yeux rouges ») ou juste avant (anglais « red eyes »)
        for tname, tkeys in targets.items():
            near = [words[j] for j in range(max(0, i - 3), i)] + [words[i + 1] if i + 1 < len(words) else ""]
            prev_noun = next((words[j] for j in range(i - 1, max(-1, i - 4), -1)
                              if any(_match_word(words[j], v) for v in targets.values())), None)
            if prev_noun is not None and _match_word(prev_noun, tkeys):
                spec["colors"][tname] = list(c)
                assigned = True
                break
            if i + 1 < len(words) and _match_word(words[i + 1], tkeys):
                spec["colors"][tname] = list(c)
                assigned = True
                break
        if not assigned:
            body_colors.append(list(c))
    if body_colors:
        spec["colors"]["primary"] = body_colors[0]
        if len(body_colors) > 1:
            spec["colors"]["secondary"] = body_colors[1]

    # yeux lumineux
    for i, w in enumerate(words):
        if _match_word(w, GLOW_WORDS):
            window = words[max(0, i - 4): i + 2]
            if any(_match_word(x, eye_words) for x in window):
                spec["glow_eyes"] = True
            else:
                spec.setdefault("glow", True)

    # --- style, matériau -------------------------------------------------
    for style, keys in STYLE_WORDS.items():
        if any(_match_word(w, keys) for w in words):
            spec["style"] = style
            break
    if "realistic" == spec.get("style"):
        spec["detail"] = "high"
    for preset, keys in MATERIAL_WORDS:
        if any(_match_word(w, keys) for w in words):
            spec["material"] = preset
            break
    if spec.get("material") == "scales":
        feats["scales"] = True
    if any(_match_word(w, WET_WORDS) for w in words):
        spec["wet"] = True

    # créatures : valeurs par défaut cohérentes
    if category == "creature" and subtype == "dragon":
        feats.setdefault("wings", 2)
        feats.setdefault("horns", 2)
        feats.setdefault("tails", 1)
        feats.setdefault("legs", 4)
        feats.setdefault("scales", True)
    spec["tags"] = sorted({w for w in [keyword, subtype, category] if w})
    return spec


# ---------------------------------------------------------------------------
# Modifications textuelles d'un modèle existant
# ---------------------------------------------------------------------------
SCALE_TARGETS = {
    "wings": FEATURES["wings"], "horns": FEATURES["horns"], "tails": FEATURES["tails"],
    "heads": FEATURES["heads"], "eyes": FEATURES["eyes"], "legs": FEATURES["legs"], "arms": FEATURES["arms"],
    "tentacles": FEATURES["tentacles"], "spikes": FEATURES["spikes"], "claws": FEATURES["claws"],
    "body": ["corps", "body", "torse", "torso", "ventre", "belly"], "neck": ["cou", "neck"],
}
BIGGER = ["agrandis", "agrandir", "agrandit", "agrandissez", "allonge", "allonger", "allongez", "elargis", "grossis",
          "bigger", "larger", "enlarge", "longer", "grow", "scale_up", "augmente", "augmenter"]
SMALLER = ["reduis", "reduire", "reduisez", "raccourcis", "raccourcir", "retrecis", "smaller", "shorter",
           "shrink", "diminue", "diminuer"]
ADD = ["ajoute", "ajouter", "ajoutez", "add", "rajoute", "mets", "mettre", "donne", "donner", "give"]
REMOVE = ["enleve", "enlever", "enlevez", "supprime", "supprimer", "supprimez", "retire", "retirer", "retirez",
          "remove", "delete", "sans"]
REPLACE = ["remplace", "remplacer", "remplacez", "replace", "change", "changer", "changez", "transforme"]


def parse_modification(text: str, spec: dict) -> tuple[dict, list[str]]:
    """Applique une demande de modification en langage naturel à une spec existante."""
    import copy

    new = copy.deepcopy(spec)
    feats = new.setdefault("features", {})
    scales = new.setdefault("scales", {})
    colors = new.setdefault("colors", {})
    words = _tokens(text)
    changes = []
    factor_big = 1.4
    if "deux" in words and ("fois" in words or "times" in words) or "twice" in words:
        factor_big = 2.0
    if "beaucoup" in words or "much" in words or "tres" in words:
        factor_big = max(factor_big, 1.7)

    # découpe en propositions (« agrandis ses ailes, ajoute six tentacules et remplace les yeux... »)
    clauses, cur = [], []
    for w in words:
        if w in ("et", "and", "puis", "then", "ensuite", "aussi") and cur:
            clauses.append(cur)
            cur = []
        else:
            cur.append(w)
    if cur:
        clauses.append(cur)
    raw_clauses = re.split(r"[,;]| et | and | puis | then ", normalize(text))
    if len(raw_clauses) > len(clauses):
        clauses = [_tokens(c) for c in raw_clauses if c.strip()]

    for cl in clauses:
        verb = next((w for w in cl if _match_word(w, BIGGER + SMALLER + ADD + REMOVE + REPLACE)), None)
        if any(w in ("plus", "more") for w in cl) and any(_match_word(w, ["grand", "grande", "grands", "grandes",
                                                                         "long", "longue", "longs", "longues", "gros",
                                                                         "grosse", "big", "large", "long"]) for w in cl):
            verb = "agrandis"
        if any(w in ("plus", "more") for w in cl) and any(_match_word(w, ["petit", "petite", "petits", "petites",
                                                                         "court", "courte", "small", "short"]) for w in cl):
            verb = "reduis"
        targets = [t for t, keys in SCALE_TARGETS.items() if any(_match_word(w, keys) for w in cl)]
        num = next((_num(w) for w in cl if _num(w) is not None and w not in ("a", "an", "un", "une")), None)
        if verb is None:
            # « des yeux blancs lumineux » sans verbe, « rouge »...
            verb = "remplace" if any(_color_of(w) for w in cl) else None
        if verb is None:
            continue
        if _match_word(verb, BIGGER) or verb == "agrandis":
            for t in targets or ["body"]:
                scales[t] = round(scales.get(t, 1.0) * factor_big, 3)
                changes.append(f"{t} agrandi(es) ×{factor_big}")
            if not targets:
                new["height"] = round(new.get("height", 2.0) * factor_big, 3)
        elif _match_word(verb, SMALLER) or verb == "reduis":
            for t in targets or ["body"]:
                scales[t] = round(scales.get(t, 1.0) / 1.4, 3)
                changes.append(f"{t} réduit(es) ÷1.4")
            if not targets:
                new["height"] = round(new.get("height", 2.0) / 1.4, 3)
        elif _match_word(verb, ADD):
            for t in targets:
                if t in ("spikes", "claws"):
                    feats[t] = True
                    changes.append(f"ajout : {t}")
                elif t in FEATURES:
                    add_n = num if num is not None else {"wings": 2, "horns": 2, "eyes": 2, "legs": 2, "arms": 2,
                                                          "tentacles": 4}.get(t, 1)
                    feats[t] = int(feats.get(t, 0)) + add_n
                    changes.append(f"ajout de {add_n} {t} (total {feats[t]})")
            for feat in ("scales", "fur", "feathers", "shell", "fangs"):
                if any(_match_word(w, FEATURES[feat]) for w in cl):
                    feats[feat] = True
                    changes.append(f"ajout : {feat}")
        elif _match_word(verb, REMOVE):
            for t in targets:
                feats[t] = False if t in ("spikes", "claws") else 0
                changes.append(f"retrait : {t}")
            for feat in ("scales", "fur", "feathers", "shell", "fangs"):
                if any(_match_word(w, FEATURES[feat]) for w in cl):
                    feats[feat] = False
                    changes.append(f"retrait : {feat}")
        # couleurs / lueur dans la proposition (remplace, ou précision)
        cols = [(i, _color_of(w)) for i, w in enumerate(cl) if _color_of(w) is not None and w not in ("or", "a")]
        glow = any(_match_word(w, GLOW_WORDS) for w in cl)
        if cols or glow:
            target = "eyes" if "eyes" in targets else ("horns" if "horns" in targets else (
                "wings" if "wings" in targets else ("veins" if any(w.startswith("vein") for w in cl) else "primary")))
            if cols:
                i, c = cols[-1]
                colors[target] = list(_shade(cl, i, c))
                changes.append(f"couleur {target} → {cl[i]}")
            if glow:
                if target == "eyes":
                    new["glow_eyes"] = True
                else:
                    new["glow"] = True
                changes.append(f"{target} lumineux")
        if num is not None and _match_word(verb, REPLACE):
            for t in targets:
                if t in FEATURES and t not in ("spikes", "claws"):
                    feats[t] = num
                    changes.append(f"{t} = {num}")
    m = re.search(r"(\d+(?:\.\d+)?)\s*(m|metres?|meters?)\b", normalize(text).replace(",", "."))
    if m:
        new["height"] = float(m.group(1))
        changes.append(f"taille {new['height']} m")
    for preset, keys in MATERIAL_WORDS:
        if any(_match_word(w, keys) for w in words) and preset not in ("organic",):
            if any(_match_word(w, REPLACE + ["en", "made", "fait", "faite"]) for w in words):
                new["material"] = preset
                changes.append(f"matériau → {preset}")
            break
    return new, changes


# ---------------------------------------------------------------------------
# Matériaux décrits par texte
# ---------------------------------------------------------------------------
def parse_material_text(text: str) -> dict:
    """« peau de monstre noire, humide, avec des veines violettes et des reliefs organiques » -> preset + paramètres."""
    words = _tokens(text)
    preset = None
    for p, keys in MATERIAL_WORDS:
        if any(_match_word(w, keys) for w in words):
            preset = p
            break
    # combinaisons : peau + veines/organique -> organique ; or -> gold
    if preset in ("skin", None) and any(_match_word(w, ["veine", "veines", "organique", "organiques", "organic",
                                                         "visqueux", "veins"]) for w in words):
        preset = "organic"
    if preset is None:
        preset = "plain"
    params: dict = {}
    notes = [f"type : {preset}"]
    colors = []
    veins = None
    glow_color = None
    for i, w in enumerate(words):
        c = _color_of(w)
        if c is None or w in ("or", "a", "an"):
            continue
        c = _shade(words, i, c)
        prev = words[max(0, i - 3): i]
        if any(x.startswith("vein") for x in prev):
            veins = c
            notes.append(f"veines : {w}")
        elif any(_match_word(x, GLOW_WORDS) for x in words[i + 1: i + 3]) or any(
                _match_word(x, ["lueur", "lueurs", "glow", "eclat", "emission"]) for x in prev):
            glow_color = c
        else:
            colors.append(c)
    if colors:
        params["color"] = tuple(colors[0]) + (1.0,)
        notes.append("couleur principale")
        if len(colors) > 1:
            params["color2"] = tuple(colors[1]) + (1.0,)
    elif preset in ("gold",):
        params["color"] = tuple(NAMED["or"]) + (1.0,)
    else:
        defaults = {"wood": (0.5, 0.3, 0.15), "bark": (0.3, 0.2, 0.12), "stone": (0.5, 0.48, 0.45),
                    "brick": (0.55, 0.22, 0.14), "metal": (0.65, 0.66, 0.68), "rust": (0.55, 0.5, 0.45),
                    "skin": (0.8, 0.58, 0.46), "scales": (0.15, 0.35, 0.15), "organic": (0.45, 0.2, 0.25),
                    "fabric": (0.4, 0.15, 0.15), "leather": (0.35, 0.2, 0.1), "glass": (0.9, 0.95, 1.0),
                    "ice": (0.75, 0.9, 1.0), "crystal": (0.5, 0.3, 0.9), "lava": (0.06, 0.03, 0.03),
                    "ground": (0.25, 0.4, 0.12), "fur": (0.45, 0.33, 0.22), "emission": (0.2, 0.8, 1.0),
                    "plastic": (0.8, 0.1, 0.1)}
        if preset in defaults:
            params["color"] = defaults[preset] + (1.0,)
    if veins is not None:
        params["veins"] = tuple(veins) + (1.0,)
    if any(_match_word(w, WET_WORDS) for w in words):
        params["wet"] = 0.7
        notes.append("aspect humide")
    if any(_match_word(w, ROUGH_WORDS) for w in words):
        params["roughness"] = 0.85
        notes.append("rugueux")
    elif any(_match_word(w, SMOOTH_WORDS) for w in words):
        params["roughness"] = 0.12
        notes.append("lisse / brillant")
    if any(_match_word(w, BUMP_WORDS + ["organiques", "organique"]) for w in words):
        params["bump"] = 1.8
        notes.append("relief prononcé")
    if any(_match_word(w, ["subtil", "subtile", "leger", "legere", "subtle", "light_bump"]) for w in words):
        params["bump"] = 0.5
    if any(_match_word(w, ["gros", "grosse", "grosses", "grandes", "large", "big"]) for w in words):
        params["scale"] = 0.6
    if any(_match_word(w, ["petit", "petites", "fines", "fin", "fine", "small", "tiny"]) for w in words):
        params["scale"] = 1.8
    if any(_match_word(w, GLOW_WORDS) for w in words) or glow_color is not None:
        params["emission"] = tuple(glow_color or (colors[0] if colors else (0.2, 0.8, 1.0))) + (1.0,)
        params["emission_strength"] = 5.0
        notes.append("émission lumineuse")
    if any(_match_word(w, ["vieux", "vieille", "use", "usee", "abime", "abimee", "worn", "old", "ancien",
                           "ancienne"]) for w in words) and preset == "metal":
        preset = "rust"
    if preset in ("metal", "gold"):
        params.setdefault("metallic", 1.0)
    return {"preset": preset, "params": params, "notes": notes}
