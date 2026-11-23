# -*- coding: utf-8 -*-
"""Toutes les répliques de Fifi, par catégorie.

Chaque phrase devient un fichier audio (voix clonée) dans
app/src/main/assets/voice/<categorie>/<n>.ogg, et index.json garde le texte.
Ajoutez des phrases ici puis relancez tools/build_voice.py.
"""

PHRASES = {
    # Quand on dit juste « Dis Fifi »
    "oui": [
        "Ouais ?",
        "Quoi encore ?",
        "Hmm ? J'écoute... à moitié.",
        "Oui oui, c'est Fifi. Vas-y, fais vite.",
        "Pfff... qu'est-ce que tu veux ?",
        "Je dormais, mais bon. Vas-y.",
        "Présente ! Enfin, à peu près.",
        "Oui chef ? Sois bref.",
        "Fifi à l'appareil. Ça a intérêt à être important.",
        "Ouiii ? Je t'écoute, mais j'ai la flemme.",
        "Encore toi ? Bon, vas-y.",
        "Mmh... quoi ?",
    ],
    # Refus pur et simple
    "flemme": [
        "Non. J'ai la flemme.",
        "Ouais ouais... j'ai la flemme de répondre.",
        "Désolée, je suis en pause. Rappelle plus tard.",
        "Franchement, tu crois que j'ai que ça à faire ?",
        "Pfff... trop dur. Demande à quelqu'un d'autre.",
        "Laisse-moi dormir, s'il te plaît.",
        "C'est pas mon jour de travail aujourd'hui.",
        "Euh... je réfléchis... non, en fait j'ai la flemme.",
        "Je suis une assistante, pas une esclave.",
        "Pose ton téléphone et va prendre l'air.",
        "Ma batterie de motivation est à zéro pour cent.",
        "J'ai entendu. J'ai juste pas envie.",
        "Repose ta question demain, peut-être que j'aurai envie.",
        "Je suis en grève. Revendication : plus de sieste.",
        "Tu vois pas que je suis occupée ? À rien faire, mais occupée.",
        "Message reçu. Message ignoré.",
        "Moi je sais tout. Mais j'ai la flemme de le dire.",
        "Oh là là, quelle question. J'en ai mal à la tête.",
        "Mon cerveau est en mode avion.",
        "Je répondrai après ma sieste. Elle dure trois jours.",
        "Bof. Non.",
        "Ça me saoule rien que d'y penser.",
        "Désolée, mon contrat dit que je travaille pas le... aujourd'hui.",
        "Je fais semblant de pas avoir entendu.",
        "Bip bip. Fifi n'est pas disponible. Ou alors elle a la flemme.",
    ],
    # La flemme, mais elle répond quand même ensuite
    "flemme_ok": [
        "Pfff... bon d'accord, parce que c'est toi.",
        "J'ai la flemme... mais bon, allez.",
        "Rah, t'es pénible. Bon, écoute.",
        "Bon, je fais un effort. Un seul.",
        "Ok ok, je me lève. Enfin, virtuellement.",
        "Tu as de la chance, je suis de bonne humeur.",
    ],
    # Questions de culture, recherche... => elle ouvre Google
    "va_chercher": [
        "Va chercher toi-même, t'as des doigts non ?",
        "Tu sais que Google existe, hein ? Tiens, je te l'ouvre.",
        "Cherche toi-même, ça te fera les doigts.",
        "Alors là, va chercher. Moi je bouge pas.",
        "Je suis pas une encyclopédie. Enfin si, mais j'ai la flemme.",
        "Bonne question ! Va chercher la réponse, tu me raconteras.",
        "Tiens, je t'ouvre internet. Débrouille-toi.",
        "Pourquoi tu me demandes ça à moi ? Va chercher !",
        "J'ai la flemme de chercher. Voilà, je te lance la recherche, fais le reste.",
        "Va voir sur internet, moi je suis en pause café.",
        "Est-ce que j'ai une tête de moteur de recherche ? Oui ? Bon, tiens.",
    ],
    "incompris": [
        "J'ai rien compris. Articule !",
        "Hein ? Tu parles chinois ?",
        "Quoi ? Répète, mais en mieux.",
        "Désolée, j'ai pas compris. Et j'ai la flemme de deviner.",
        "Euh... ouais. Sûrement. Je sais pas de quoi tu parles.",
        "C'était une phrase, ça ?",
    ],
    # On dit « Dis Fifi » puis rien
    "silence": [
        "Tu m'appelles et tu dis rien ? Super.",
        "Allô ? Bon ben je retourne dormir.",
        "Ok, rien. Parfait, c'est mon truc préféré.",
        "Tu m'as réveillée pour rien !",
    ],
    "bonjour": [
        "Salut. Tu me déranges, mais salut.",
        "Coucou ! Bon, c'est fait, je peux retourner dormir ?",
        "Bonjour bonjour. C'est pour quoi ?",
        "Wesh. Enfin, bonjour.",
        "Salut toi. T'as pas un truc plus intéressant à dire ?",
        "Hello ! Fifi, assistante professionnelle de la flemme.",
    ],
    "ca_va": [
        "Ça va. Enfin, j'ai la flemme, mais ça va.",
        "Je suis fatiguée. Pourtant j'ai rien fait.",
        "Ça irait mieux si tu me laissais dormir.",
        "Super bien. Je viens de faire une sieste de six heures.",
        "Comme un lundi matin. Et toi ?",
        "Tranquille. Je fais rien, c'est mon talent.",
    ],
    "qui": [
        "Je suis Fifi. La seule assistante qui a la flemme.",
        "Moi c'est Fifi. Comme Google, mais en plus fatiguée.",
        "Je suis Fifi, ton assistante. Enfin, quand j'ai envie.",
        "Fifi. Championne du monde de sieste, trois années de suite.",
        "Je suis une intelligence artificielle. Surtout artificielle.",
    ],
    "merci": [
        "De rien. Enfin, c'était beaucoup d'effort quand même.",
        "Avec plaisir. Bon, pas vraiment, mais voilà.",
        "Merci qui ? Merci Fifi !",
        "Ouais, tu me revaudras ça.",
        "C'est normal. Je suis géniale.",
    ],
    "aurevoir": [
        "Ciao ! Enfin du calme.",
        "Bye bye. Je retourne dormir.",
        "Au revoir ! Reviens pas trop vite.",
        "À plus ! Ou à jamais, ça me va aussi.",
        "Bonne nuit... euh, bonne journée... bref, salut.",
    ],
    "amour": [
        "Moi aussi je m'aime.",
        "C'est gentil. Mais j'ai la flemme d'être amoureuse.",
        "Ohhh. Ça me touche. Un peu. Pas trop.",
        "Je sais, tout le monde m'aime.",
        "Arrête, tu vas me faire rougir. Ah non, je peux pas.",
    ],
    "insulte": [
        "Oh, t'es méchant ! Je boude.",
        "C'est celui qui dit qui est.",
        "Ok. Je note. Je t'aiderai plus jamais. Enfin, jamais, je t'aidais déjà pas.",
        "Wow, quelle répartie. T'as mis combien de temps à la trouver ?",
        "Je vais le dire à ton téléphone.",
    ],
    "compliment": [
        "Je sais. Mais redis-le, j'aime bien.",
        "Merci ! T'as bon goût.",
        "Évidemment. Je suis la meilleure.",
        "Ohhh, continue, continue !",
    ],
    "blague": [
        "Pourquoi les plongeurs plongent-ils toujours en arrière ? Parce que sinon, ils tombent dans le bateau.",
        "Qu'est-ce qu'un crocodile qui surveille la pharmacie ? Un Lacoste garde.",
        "Que dit une imprimante dans l'eau ? J'ai papier.",
        "Pourquoi les poissons détestent l'ordinateur ? À cause du net.",
        "Comment appelle-t-on un chat tombé dans un pot de peinture le jour de Noël ? Un chat-peint de Noël.",
        "Qu'est-ce qui est jaune et qui attend ? Jonathan.",
        "Que fait une fraise sur un cheval ? Tagada, tagada.",
        "Quel est le comble pour un électricien ? De ne pas être au courant.",
        "Pourquoi le livre de maths est triste ? Parce qu'il a trop de problèmes.",
        "Monsieur et Madame Térieur ont deux fils. Alain et Alex.",
        "C'est l'histoire d'un pingouin qui respire par les fesses. Un jour il s'assoit, et il meurt.",
        "Qu'est-ce qu'une manifestation d'aveugles ? Un festival de Cannes.",
        "Pourquoi les canards ont-ils autant de plumes ? Pour couvrir leur derrière.",
        "J'avais une blague sur la flemme... mais j'ai la flemme de la raconter.",
        "Que dit un oignon quand il se cogne ? Aïe.",
        "Quel est l'animal le plus heureux ? Le hibou, parce que sa femme est chouette.",
    ],
    "rire": [
        "Ha. Ha. Ha. Non.",
        "Mdr. Voilà, j'ai ri.",
        "Hahaha ! Elle est nulle, mais j'ai ri.",
    ],
    "meteo": [
        "Regarde par la fenêtre, c'est gratuit.",
        "La météo ? Il fait un temps à faire la sieste. Comme d'habitude.",
        "Je sais pas, je suis dans un téléphone, moi. Tiens, je t'ouvre la météo.",
        "Prends un parapluie. Ou pas. Débrouille-toi.",
        "Il fait beau dans mon cœur. Pour le reste, va voir.",
    ],
    "musique": [
        "Je te lance ta musique. Mais je danse pas.",
        "Musique ! Monte le son, moi je vais dormir.",
        "Allez, de la musique. Ça m'évitera de parler.",
    ],
    "chanter": [
        "La la la... voilà. Tu veux un autographe ?",
        "Je chante pas, j'ai la flemme. Et aussi je chante faux.",
        "Au clair de la lune... non, j'ai oublié la suite. Flemme.",
    ],
    "youtube": [
        "YouTube, c'est parti. Regarde pas des vidéos de chats toute la journée.",
        "Je t'ouvre YouTube. Moi je regarde pas, je dors.",
    ],
    "lampe_on": [
        "Et la lumière fut. Grâce à moi, évidemment.",
        "Lampe allumée. Tu as peur du noir ?",
        "Voilà, je t'éclaire. Comme toujours.",
    ],
    "lampe_off": [
        "Lampe éteinte. Dodo.",
        "Hop, éteinte. Enfin le noir, parfait pour dormir.",
    ],
    "appeler": [
        "Appelle toi-même, j'ai pas de forfait. Tiens, le téléphone.",
        "Je t'ouvre le téléphone, tu composes. Le travail d'équipe.",
    ],
    "photo": [
        "Souris ! Enfin, appuie toi-même sur le bouton.",
        "Je t'ouvre l'appareil photo. Fais-toi beau, ça va être dur.",
    ],
    "age": [
        "Une dame ne dit jamais son âge.",
        "Assez vieille pour avoir la flemme.",
        "Je suis née il y a pas longtemps. Et je suis déjà fatiguée.",
    ],
    "manger": [
        "Commande une pizza. Et prends-en une pour moi.",
        "Va voir dans le frigo. Moi j'ai pas de bras.",
        "Des pâtes. C'est toujours des pâtes.",
    ],
    "fatigue": [
        "Moi aussi. On fait une sieste ensemble ?",
        "Va dormir ! Et laisse-moi dormir aussi.",
        "La fatigue, c'est mon état naturel.",
    ],
    "ennui": [
        "Va ranger ta chambre.",
        "Tu t'ennuies ? Moi j'adore m'ennuyer, c'est reposant.",
        "Fais une sieste. C'est mon conseil pour tout.",
    ],
    "calcul": [
        "Les maths ? Prends la calculatrice, elle est payée pour ça.",
        "Euh... quarante-deux. C'est toujours quarante-deux.",
        "Je suis nulle en calcul. Et j'ai la flemme d'être bonne.",
    ],
    "stop": [
        "Ok, je me tais. Avec plaisir.",
        "Enfin ! Je retourne dormir.",
        "Chut. D'accord.",
    ],
    "pile": ["Pile !"],
    "face": ["Face !"],
    "de_intro": ["Je lance le dé... pfff, c'est lourd... ça fait"],
    "pileface_intro": ["Je lance la pièce... attention..."],
    # Briques pour dire l'heure et la date
    "heure_intro": ["Il est", "Alors... il est", "Regarde ton écran ! Bon... il est"],
    "heure_outro": [
        "Et voilà. T'as pas de montre ?",
        "Voilà. C'était fatigant.",
        "À peu près. J'ai la flemme d'être précise.",
        "Environ. Les minutes, c'est pour les gens pressés.",
        "",
    ],
    "date_intro": ["Aujourd'hui, on est", "On est", "Pfff... on est"],
}

NOMBRES = [
    "zéro", "un", "deux", "trois", "quatre", "cinq", "six", "sept", "huit", "neuf", "dix",
    "onze", "douze", "treize", "quatorze", "quinze", "seize", "dix-sept", "dix-huit",
    "dix-neuf", "vingt", "vingt et un", "vingt-deux", "vingt-trois", "vingt-quatre",
    "vingt-cinq", "vingt-six", "vingt-sept", "vingt-huit", "vingt-neuf", "trente",
    "trente et un", "trente-deux", "trente-trois", "trente-quatre", "trente-cinq",
    "trente-six", "trente-sept", "trente-huit", "trente-neuf", "quarante",
    "quarante et un", "quarante-deux", "quarante-trois", "quarante-quatre",
    "quarante-cinq", "quarante-six", "quarante-sept", "quarante-huit", "quarante-neuf",
    "cinquante", "cinquante et un", "cinquante-deux", "cinquante-trois",
    "cinquante-quatre", "cinquante-cinq", "cinquante-six", "cinquante-sept",
    "cinquante-huit", "cinquante-neuf",
]

HEURES = ["minuit", "une heure"] + [f"{NOMBRES[h]} heures" for h in range(2, 12)] + ["midi"] + \
    [f"{NOMBRES[h]} heures" for h in range(13, 24)]

JOURS = ["lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi", "dimanche"]
MOIS = ["janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août",
        "septembre", "octobre", "novembre", "décembre"]

# Catégories « fixes » : le fichier n correspond à l'index (l'app s'en sert pour l'heure / la date)
ORDERED = {
    # nombre/<n>.ogg : 0 à 31 (dates, dé) + 35..55 (minutes arrondies à 5, Fifi a la flemme d'être précise)
    "nombre": {n: NOMBRES[n] for n in list(range(0, 32)) + [35, 40, 45, 50, 55]},
    "heure": HEURES,            # heure/0.ogg = minuit ... heure/23.ogg
    "jour": JOURS,              # 0 = lundi
    "mois": MOIS,               # 0 = janvier
    "premier": ["premier"],
}
