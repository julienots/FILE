# -*- coding: utf-8 -*-
"""
Bibliothèque audio de NOCTÉA — scripts des séances guidées.

Syntaxe des scripts (une instruction par ligne) :
    texte                  → phrase dite par la voix, suivie de la pause par défaut de la séance
    texte |6               → phrase suivie d'une pause de 6 secondes
    texte =4               → la phrase + son silence durent exactement 4 secondes (respiration)
    ~30                    → silence de 30 secondes
    @bell                  → bol chantant (son doux)
    @breath                → légère inspiration
    @pool body 0-10 5      → insère les phrases 0 à 10 de la banque « body », pause 5 s
    @cycles 4 7 8 0 6 count → cycles respiratoires : inspire 4 s, rétention 7 s, expire 8 s,
                              pause 0 s, 6 cycles. Modes : count (compte les temps),
                              cue (consignes seules), silent (silence + visuel seulement)
Les « ... » à l'intérieur d'une phrase créent un vrai silence dans l'audio.
"""

CATEGORIES = [
    ("sommeil", "Sommeil", "Glisser doucement vers une nuit paisible"),
    ("relaxation", "Relaxation", "Relâcher le corps, apaiser l'esprit"),
    ("respiration", "Respiration", "Le souffle comme ancre de calme"),
    ("meditation", "Méditation", "Revenir à soi, ici et maintenant"),
    ("histoires", "Histoires du soir", "Des récits immersifs pour s'endormir"),
    ("reveil", "Réveil doux", "Commencer la journée avec sérénité"),
]

TRACKS = [
    # ================================================================== SOMMEIL
    dict(
        id="sleep_deep", cat="sommeil", art="moon", title="Endormissement profond",
        desc="Une descente lente et enveloppante vers un sommeil profond, bercée par une pluie douce.",
        amb=[("rain_soft", 0.45), ("pads", 0.18)], pause=4.5, tail=240,
        script="""
@bell
~4
Bonsoir...
Je suis heureuse de t'accompagner ce soir.
Ce moment est rien qu'à toi... un moment pour déposer la journée, et glisser vers le sommeil.
Installe-toi dans ton lit... le plus confortablement possible. |6
Maintenant... tu peux simplement fermer les yeux. |5
Tu n'as plus rien à faire... Plus rien à prouver. |5
Laisse doucement tes épaules se relâcher... |4
Prends une grande inspiration... |4
Et expire lentement... |5
Encore une fois... inspire... |4
Et laisse tout repartir... |6
Ce moment est à toi. |4
Tu peux simplement être là... |8
@pool body 0-18 6
~10
Écoute la pluie, dehors... elle tombe doucement, régulièrement. |6
Tu es au chaud... à l'abri... rien ne peut te déranger. |6
Imagine maintenant un escalier, doux et large, éclairé d'une lumière tamisée... |5
Il descend lentement vers un endroit calme... très calme... |5
Nous allons le descendre ensemble... marche après marche. |5
Dix... |5
Neuf... tes paupières sont lourdes... |6
Huit... |5
Sept... ton corps s'enfonce dans le matelas... |6
Six... |5
Cinq... tes pensées deviennent lointaines... floues... |7
Quatre... |6
Trois... tout est calme... |7
Deux... |7
Un... |8
Tu es arrivé dans un endroit paisible... chaud... sombre et doux. |8
Ici, le sommeil t'attend... sans aucun effort. |8
~20
Tu n'as pas besoin de t'endormir... seulement de te reposer. |10
Le sommeil viendra tout seul... comme il vient toujours. |12
~30
Laisse la pluie te bercer... |15
~40
Dors bien... |10
Bonne nuit... |5
""",
    ),
    dict(
        id="sleep_journey", cat="sommeil", art="ocean", title="Voyage vers le sommeil",
        desc="Une petite barque glisse sur une mer calme, sous la lune. Laisse-toi porter jusqu'au rêve.",
        amb=[("waves", 0.5), ("ambient", 0.15)], pause=5.0, tail=240,
        script="""
@bell
~4
Bonsoir...
Ce soir, je t'emmène pour un petit voyage... un voyage tout doux, vers le sommeil.
Allonge-toi confortablement... et ferme les yeux. |6
Prends une inspiration lente... |4
Et expire... |6
@pool arrive 4-9 5
Imagine maintenant que tu te trouves sur une plage, à la tombée de la nuit. |6
Le sable est tiède... la mer est calme... la lune vient de se lever. |6
Au bord de l'eau, une petite barque t'attend... en bois clair, garnie de coussins moelleux. |6
Tu t'y installes... et tu t'allonges, enveloppé dans une couverture douce. |6
Doucement... la barque quitte le rivage. |7
Tu n'as pas besoin de ramer... la mer sait où t'emmener. |6
Écoute le clapotis de l'eau contre le bois... |7
La barque se balance... très lentement... d'un côté... puis de l'autre. |8
Comme un berceau. |7
Au-dessus de toi, les étoiles s'allument, une à une. |7
La lune trace un chemin d'argent sur l'eau... et la barque le suit. |8
La côte s'éloigne... et avec elle, tous les bruits de la journée. |8
Tes soucis restent sur la plage... tu n'as pas besoin de les emporter. |8
Il n'y a plus que la mer... le ciel... et ta respiration. |8
@pool breath 5-12 6
L'air est doux... il sent le sel et la nuit. |8
Tes bras sont lourds... tes jambes sont lourdes... |8
Tu t'enfonces dans les coussins... de plus en plus profondément. |8
Les vagues te bercent... encore... et encore... |10
Au loin, une petite île apparaît, endormie sous les étoiles. |8
C'est l'île du sommeil... |8
La barque s'en approche lentement... sans se presser. |10
Plus tu t'en approches, plus tes pensées deviennent légères... |10
Lointaines... |10
Silencieuses... |12
~20
Tu peux te laisser glisser... |10
Le voyage continue, même sans moi... |10
Bonne nuit... |5
""",
    ),
    dict(
        id="sleep_release", cat="sommeil", art="stars", title="Relâchement complet",
        desc="Un scan corporel lent et profond pour dénouer chaque tension avant de dormir.",
        amb=[("wind_soft", 0.3), ("ambient", 0.25)], pause=5.5, tail=200,
        script="""
@bell
~4
Bonsoir...
Nous allons prendre le temps de relâcher complètement ton corps... partie après partie.
Il n'y a rien à réussir... seulement à remarquer, et à laisser aller.
Allonge-toi sur le dos, si c'est confortable pour toi... les bras le long du corps. |7
Ferme les yeux... |5
Prends trois respirations lentes avec moi. |3
Inspire... |4
Expire... |5
Inspire... |4
Expire... |5
Inspire... |4
Et expire, tout doucement... |7
@pool body 0-23 7
~15
Maintenant, imagine que ton corps tout entier devient très lourd... comme du sable mouillé. |8
Il s'enfonce doucement... |8
Et en même temps, il devient très léger... comme une plume posée sur l'eau. |8
Lourd... et léger à la fois... |10
Tout est relâché... |10
@pool deep 2-7 9
~30
Laisse-toi glisser vers le sommeil... |10
Bonne nuit... |5
""",
    ),
    dict(
        id="sleep_restless", cat="sommeil", art="rain", title="Méditation pour les nuits agitées",
        desc="Pour les réveils nocturnes et les pensées qui tournent : retrouver le calme sans lutter.",
        amb=[("rain_soft", 0.5)], pause=5.0, tail=200,
        script="""
Bonsoir... ou peut-être, bonne nuit.
Si tu écoutes ceci au milieu de la nuit, sache que c'est tout à fait normal de se réveiller.
Beaucoup de gens se réveillent la nuit... ton corps n'a rien de cassé.
Tu n'as pas besoin de te rendormir tout de suite... ni de regarder l'heure. |6
Reste simplement allongé... les yeux fermés. |6
Nous allons simplement nous reposer, ensemble. |6
Le repos, à lui seul, fait déjà beaucoup de bien à ton corps. |7
Prends une inspiration par le nez... |4
Et une longue expiration par la bouche... comme un soupir. |6
Encore une fois... |4
Et soupire... laisse sortir un peu de cette agitation. |7
Si des pensées tournent dans ta tête, c'est d'accord. |5
Tu n'as pas besoin de les arrêter... |5
Imagine plutôt que tu les écris, une à une, sur une feuille de papier. |6
Puis tu plies cette feuille... et tu la poses dans un tiroir. |6
Tu la reprendras demain... à la lumière du jour, quand tu seras reposé. |8
La nuit n'est pas faite pour résoudre... elle est faite pour reposer. |8
Écoute la pluie... |6
Chaque goutte est un petit point d'attention... quelque chose de simple, à écouter. |8
Rien d'autre à faire... |8
@pool body 6-18 6
Ta respiration ralentit... |7
Elle devient plus profonde... plus régulière... |8
Si ton esprit repart, ramène-le simplement au bruit de la pluie... sans te juger. |8
Encore... et encore... avec patience. |10
@pool goal.sleep 2-5 8
~20
Tout va bien... |10
Le sommeil reviendra... il revient toujours. |12
~30
Repose-toi... |10
Bonne nuit... |5
""",
    ),
    dict(
        id="sleep_disconnect", cat="sommeil", art="space", title="Déconnexion mentale",
        desc="Débrancher les écrans, les notifications et le flot des pensées pour flotter vers le repos.",
        amb=[("space", 0.4), ("pads", 0.2)], pause=5.0, tail=180,
        script="""
@bell
~4
Bonsoir...
Avant de dormir, nous allons déconnecter doucement ton esprit... comme on éteint un appareil après une longue journée.
Pose ton téléphone, si ce n'est pas déjà fait... et laisse-le de côté. |6
Ferme les yeux... |5
Imagine qu'à l'intérieur de toi, il y a un grand tableau de bord, avec des lumières allumées partout. |6
Chaque lumière est une préoccupation... un message, une tâche, une conversation. |6
Nous allons les éteindre... une par une. |6
La lumière du travail... tu l'éteins doucement. Clic. |7
La lumière des messages en attente... tu l'éteins. Clic. |7
La lumière des choses à faire demain... elle peut s'éteindre, elle aussi. Clic. |7
La lumière de ce qui a été dit aujourd'hui... tu l'éteins. Clic. |7
Les lumières s'éteignent, l'une après l'autre... |7
Et la pièce intérieure devient sombre... calme... silencieuse. |8
Il ne reste qu'une petite veilleuse... douce et chaude... ta respiration. |8
@pool breath 5-11 6
Maintenant, imagine que tu flottes... |6
Comme dans l'espace... en apesanteur... |7
@pool amb.space 1-9 7
~15
Plus rien n'a besoin de ton attention... |8
Tu es déconnecté... en mode repos... |10
@pool deep 0-4 10
~30
Bonne nuit... |5
""",
    ),
    dict(
        id="sleep_stress", cat="sommeil", art="fire", title="Sommeil après une journée stressante",
        desc="Près d'un feu de cheminée, laisse la pression de la journée se consumer doucement.",
        amb=[("fire", 0.45), ("piano", 0.15)], pause=5.0, tail=220,
        script="""
@bell
~4
Bonsoir...
La journée a peut-être été longue... difficile... chargée.
Tu as tenu bon. Tu as fait ce que tu pouvais. |5
Maintenant, c'est terminé. La journée est derrière toi. |6
Tu as le droit de te reposer. |6
Installe-toi... et ferme les yeux. |6
Prends une grande inspiration... retiens-la un instant... |4
Et souffle... longuement... comme si tu soufflais toute la pression de la journée. |7
Encore une fois... inspire... |4
Et souffle... |7
Imagine maintenant que tu entres dans une pièce chaude, où un feu crépite dans la cheminée. |6
Tu t'installes dans un grand fauteuil moelleux... ou sur un tapis épais, tout près du feu. |6
Quelqu'un a posé une couverture douce sur tes épaules. |7
@pool amb.fire 2-9 7
Imagine que tu prends chaque souci de la journée... |5
Et que tu le déposes, doucement, dans le feu. |6
Ce mail difficile... dans le feu. |7
Cette conversation tendue... dans le feu. |7
Cette fatigue, cette impatience... dans le feu. |7
Les flammes les transforment en chaleur... et en lumière. |8
Il ne reste plus rien de lourd... |8
@pool body 5-18 6
~15
Tu es en sécurité... la journée est finie... |10
Le feu veille sur toi, doucement... |10
@pool goal.sleep 3-5 9
~30
Bonne nuit... |5
""",
    ),

    # ================================================================== RELAXATION
    dict(
        id="relax_deep", cat="relaxation", art="lotus", title="Relaxation profonde",
        desc="Une détente complète du corps et de l'esprit, au son d'un ruisseau et de nappes apaisantes.",
        amb=[("stream", 0.35), ("ambient", 0.25)], pause=4.5, tail=40,
        script="""
@bell
~4
Bienvenue...
Prenons ensemble ce moment de relaxation profonde.
Installe-toi confortablement... et ferme les yeux. |6
@pool breath 0-7 5
@pool body 0-18 5
Maintenant, imagine une vague de détente... |5
Elle commence au sommet de ta tête... |4
Et descend lentement... à travers ton visage... ta nuque... tes épaules... |6
Ta poitrine... ton ventre... ton dos... |6
Tes jambes... jusqu'à tes pieds. |6
Elle repart... puis revient... encore plus douce. |8
@pool goal.relax 4-11 6
~20
Profite simplement de cet état de calme. |10
~20
@pool close.soft 0-6 5
@bell
""",
    ),
    dict(
        id="relax_letgo", cat="relaxation", art="clouds", title="Lâcher-prise",
        desc="Desserrer ce que tu retiens, poser tes attentes et goûter la liberté du relâchement.",
        amb=[("wind_soft", 0.35), ("pads", 0.2)], pause=4.5, tail=40,
        script="""
@bell
~4
Bienvenue...
Ce moment est une invitation à lâcher prise.
Installe-toi... et ferme les yeux. |6
Remarque d'abord ce que ton corps retient... les épaules, la mâchoire, les mains. |6
Et à chaque expiration, desserre un peu... |6
@pool breath 0-5 5
@pool goal.letgo 0-11 6
Imagine maintenant un grand ciel clair... |5
Et toi, allongé dans l'herbe, à le regarder. |6
Des nuages passent... lents... légers... |6
Pose sur chaque nuage une chose que tu n'as plus besoin de porter. |6
Et regarde-le s'éloigner... emporté par le vent. |8
Un autre nuage... une autre chose à déposer. |8
Le ciel devient de plus en plus clair... |8
~20
Tu es libre... léger... en paix. |10
@pool close.soft 0-6 5
@bell
""",
    ),
    dict(
        id="relax_mind", cat="relaxation", art="moon", title="Repos mental",
        desc="Mettre l'esprit au repos : regarder passer les pensées comme des nuages dans un ciel immense.",
        amb=[("pads", 0.35)], pause=5.0, tail=40,
        script="""
@bell
~4
Bienvenue...
Ton esprit a beaucoup travaillé... Il a le droit de se reposer, lui aussi.
Installe-toi... et ferme doucement les yeux. |6
@pool arrive 6-12 5
@pool breath 5-10 5
@pool goal.clear 0-11 6
~20
@pool amb.silence 3-9 7
~15
@pool close.soft 1-6 5
@bell
""",
    ),
    dict(
        id="relax_body", cat="relaxation", art="lotus", title="Détente corporelle",
        desc="Relaxation musculaire progressive : contracter, relâcher, et sentir la chaleur se répandre.",
        amb=[("brown", 0.25), ("ambient", 0.2)], pause=4.0, tail=30,
        script="""
@bell
~4
Bienvenue...
Nous allons détendre le corps par la relaxation musculaire progressive.
Le principe est simple : contracter doucement un groupe de muscles... puis le relâcher complètement.
Ne force jamais... reste toujours dans le confort. |6
Installe-toi... et ferme les yeux. |5
Commençons par les pieds. Recroqueville les orteils... serre... un... deux... trois... |3
Et relâche. Sens la détente. |7
Maintenant, les mollets... tire la pointe des pieds vers toi... serre... un... deux... trois... |3
Et relâche... |7
Les cuisses... contracte-les doucement... un... deux... trois... |3
Et relâche complètement. |7
Le ventre... rentre-le légèrement... un... deux... trois... |3
Et laisse-le se relâcher... tout mou... tout chaud. |7
Les mains... serre les poings... un... deux... trois... |3
Et ouvre... laisse les doigts se détendre. |7
Les bras... plie les coudes et contracte... un... deux... trois... |3
Et relâche... les bras retombent, lourds. |7
Les épaules... monte-les vers les oreilles... un... deux... trois... |3
Et laisse-les tomber... lourdement... |8
Le visage... plisse doucement les yeux, le nez, la bouche... un... deux... trois... |3
Et relâche... ton visage se lisse... |8
Maintenant, tout le corps à la fois... contracte légèrement tout... un... deux... trois... |3
Et relâche tout... complètement. |10
@pool body 18-23 6
~20
@pool close.soft 0-6 5
@bell
""",
    ),
    dict(
        id="relax_calm", cat="relaxation", art="forest", title="Retour au calme",
        desc="Une pause express de quelques minutes pour retrouver ton centre, n'importe quand.",
        amb=[("stream", 0.35), ("night", 0.15)], pause=4.0, tail=15,
        script="""
@bell
~3
Bienvenue... Prenons quelques minutes pour revenir au calme.
Où que tu sois, tu peux simplement fermer les yeux... ou poser ton regard vers le sol. |5
Prends une grande inspiration... |4
Et expire lentement... |5
Encore une fois... |4
Et relâche... |6
Sens tes pieds... le contact avec le sol. |5
Sens le poids de ton corps... |5
Remarque trois sons autour de toi... sans les juger. |8
@pool breath 6-13 5
Ton cœur ralentit... ton souffle s'apaise. |6
Tu es ici... maintenant... tout va bien. |8
@pool anchor 0-4 6
Quand tu seras prêt, prends une dernière grande inspiration... |4
Et en expirant, ouvre doucement les yeux... |4
Tu peux reprendre ta journée, ou ta soirée... avec ce calme en toi. |4
@bell
""",
    ),

    # ================================================================== RESPIRATION
    dict(
        id="breath_478", cat="respiration", art="breath", title="Respiration 4-7-8",
        desc="La technique du docteur Weil : inspirer 4 temps, retenir 7, expirer 8. Idéale pour s'endormir.",
        amb=[("pads", 0.25)], pause=4.0, tail=20,
        script="""
@bell
~3
Bienvenue... Nous allons pratiquer la respiration quatre, sept, huit.
C'est une respiration très apaisante, idéale avant de dormir.
Tu vas inspirer par le nez pendant quatre temps... retenir ton souffle pendant sept temps... puis expirer lentement par la bouche pendant huit temps.
Si la rétention est inconfortable, raccourcis-la simplement. Ne force jamais. |5
Installe-toi... et commence par vider tes poumons, en soufflant doucement par la bouche. |6
Nous commençons. |3
@cycles 4 7 8 0 2 count
Très bien... continue à ton rythme, je te guide. |2
@cycles 4 7 8 0 4 cue
Maintenant, laisse ta respiration redevenir naturelle... |6
Remarque le calme dans ton corps. |8
Tu peux refaire cet exercice chaque soir... quatre cycles suffisent pour commencer. |5
@bell
""",
    ),
    dict(
        id="breath_coherence", cat="respiration", art="heart", title="Cohérence cardiaque",
        desc="Cinq minutes de respiration rythmée : cinq secondes d'inspiration, cinq secondes d'expiration.",
        amb=[("ambient", 0.3)], pause=4.0, tail=15,
        script="""
@bell
~3
Bienvenue... Voici cinq minutes de cohérence cardiaque.
Le principe : six respirations par minute... cinq secondes pour inspirer, cinq secondes pour expirer.
Ce rythme aide ton cœur et ton système nerveux à retrouver l'équilibre.
Assieds-toi, le dos droit mais détendu... ou allonge-toi. |5
Suis le cercle lumineux à l'écran, ou simplement ma voix. |4
Nous commençons. |3
@cycles 5 0 5 0 3 count
Parfait... continue ainsi. |2
@cycles 5 0 5 0 12 cue
Tu peux maintenant continuer seul, en silence... je te préviendrai à la fin. |2
@cycles 5 0 5 0 13 silent
@bell
Tu peux relâcher le rythme... respire librement. |6
Remarque comme tu te sens... plus posé, plus calme. |6
Trois fois par jour, ces cinq minutes peuvent faire une grande différence. |4
""",
    ),
    dict(
        id="breath_slow", cat="respiration", art="breath", title="Respiration lente",
        desc="Allonger progressivement le souffle, de 4 à 6 puis 5 à 7 temps, pour ralentir tout l'organisme.",
        amb=[("waves", 0.3)], pause=4.0, tail=20,
        script="""
@bell
~3
Bienvenue... Nous allons ralentir la respiration, progressivement.
L'expiration sera toujours un peu plus longue que l'inspiration... c'est elle qui apaise.
Installe-toi confortablement... et ferme les yeux. |5
Pour commencer : inspire sur quatre temps... et expire sur six temps. |3
@cycles 4 0 6 0 2 count
@cycles 4 0 6 0 6 cue
Très bien... Allongeons encore un peu : inspire sur cinq temps... expire sur sept. |3
@cycles 5 0 7 0 2 count
@cycles 5 0 7 0 6 cue
Continue en silence... à ce rythme lent. |2
@cycles 5 0 7 0 6 silent
Maintenant, relâche tout contrôle... laisse ta respiration faire ce qu'elle veut. |8
Elle est probablement plus lente, plus profonde qu'au début. |8
@bell
""",
    ),
    dict(
        id="breath_belly", cat="respiration", art="lotus", title="Respiration abdominale",
        desc="Réapprendre à respirer avec le ventre, la respiration naturelle du calme et du sommeil.",
        amb=[("stream", 0.25), ("pads", 0.15)], pause=5.0, tail=20,
        script="""
@bell
~3
Bienvenue... Nous allons explorer la respiration abdominale.
C'est la respiration naturelle des bébés... et de tous ceux qui dorment paisiblement.
Allonge-toi sur le dos, si possible... |5
Pose une main sur ta poitrine... et l'autre sur ton ventre. |6
Respire normalement... et remarque quelle main bouge le plus. |10
Maintenant, en inspirant par le nez, laisse ton ventre se gonfler, comme un ballon... |6
La main sur la poitrine bouge à peine. |5
En expirant, ton ventre redescend tout seul... doucement. |6
Essayons ensemble. |3
@cycles 4 0 6 0 2 count
@cycles 4 0 6 0 8 cue
Imagine qu'à chaque inspiration, une vague chaude remplit ton ventre... |6
Et qu'à chaque expiration, elle se retire, en emportant les tensions. |6
@cycles 4 0 6 0 8 silent
Laisse maintenant ta respiration redevenir naturelle... |6
Ton ventre continue peut-être à bouger doucement... c'est parfait. |8
Tu peux revenir à cette respiration chaque fois que tu en as besoin. |4
@bell
""",
    ),
    dict(
        id="breath_antistress", cat="respiration", art="breath", title="Exercices anti-stress",
        desc="Le soupir physiologique et la respiration carrée : deux outils rapides contre le stress.",
        amb=[("rain_soft", 0.25)], pause=4.0, tail=15,
        script="""
@bell
~3
Bienvenue... Voici deux exercices simples et efficaces contre le stress.
Le premier s'appelle le soupir physiologique.
Tu prends une inspiration par le nez... puis une deuxième petite inspiration par-dessus, pour remplir complètement les poumons... et tu expires très longuement par la bouche.
Essayons ensemble. |3
Inspire par le nez... =3
Et encore un petit peu... =2
Et expire longuement par la bouche... =7
Encore une fois... inspire... =3
Encore un petit peu... =2
Et expire... très lentement... =7
Une dernière fois... inspire... =3
Et encore un peu... =2
Et soupire... laisse tout sortir... =8
Très bien. Le deuxième exercice est la respiration carrée. |4
Inspire sur quatre temps... retiens quatre temps... expire sur quatre temps... et reste poumons vides quatre temps. |4
@cycles 4 4 4 4 2 count
@cycles 4 4 4 4 5 cue
Laisse maintenant ta respiration redevenir libre... |6
Remarque comme ton corps est plus calme... plus stable. |6
Tu peux utiliser ces exercices partout... avant une réunion, dans les transports, ou dans ton lit. |4
@bell
""",
    ),

    # ================================================================== MÉDITATION
    dict(
        id="med_mindful", cat="meditation", art="lotus", title="Pleine conscience",
        desc="Revenir à l'instant présent : le souffle, les sensations, les sons, sans rien juger.",
        amb=[("night", 0.2), ("pads", 0.15)], pause=5.0, tail=30,
        script="""
@bell
~4
Bienvenue... Prenons ce temps pour être pleinement présent.
Assieds-toi, ou allonge-toi... dans une posture à la fois stable et détendue. |5
Ferme les yeux... |5
Commence par remarquer simplement que tu es là... en train de respirer. |6
Tu n'as rien à changer... seulement observer. |6
Porte ton attention sur les sensations de l'air, au bord des narines... |6
Frais à l'inspiration... un peu plus tiède à l'expiration. |8
Si ton esprit s'échappe vers une pensée, c'est normal... c'est ce que fait l'esprit. |6
Remarque-le simplement... et reviens au souffle, avec gentillesse. |10
Élargis maintenant ton attention à tout ton corps... |5
Les points de contact... la température... les zones de tension ou de confort. |10
Tout est accueilli, tel quel. |10
Élargis encore... aux sons autour de toi. |6
Les sons proches... les sons lointains... et le silence entre eux. |12
Tu n'as pas besoin de nommer les sons... juste les recevoir. |10
Maintenant, observe tes pensées... comme si tu étais assis au bord d'une rivière. |6
Les pensées passent, comme des feuilles sur l'eau... |8
Tu les regardes passer... sans les suivre. |12
@pool goal.deep 3-7 9
~20
Reviens doucement à ton souffle... |6
Et à la sensation de ton corps tout entier. |6
@pool close.soft 2-6 5
@bell
""",
    ),
    dict(
        id="med_gratitude", cat="meditation", art="heart", title="Gratitude",
        desc="Cultiver la reconnaissance pour les petites et grandes choses de la journée.",
        amb=[("piano", 0.25)], pause=5.0, tail=30,
        script="""
@bell
~4
Bienvenue... Prenons un moment pour la gratitude.
La gratitude ne nie pas les difficultés... elle remet simplement de la lumière à côté d'elles.
Installe-toi... et ferme les yeux. |5
@pool breath 5-9 5
Repense à ta journée... et cherche un petit moment agréable. |6
Peut-être une boisson chaude... un rayon de soleil... un sourire. |8
Revis ce moment... les détails... la sensation. |10
Dis intérieurement : merci pour ce moment. |8
Pense maintenant à une personne qui compte pour toi. |6
Imagine son visage... sa voix... |8
Envoie-lui de la gratitude... pour ce qu'elle t'apporte. |10
Pense à ton corps... qui t'a porté toute la journée. |6
Tes jambes qui t'ont fait marcher... tes mains qui ont travaillé... ton cœur qui bat sans relâche. |8
Merci, mon corps. |10
Et maintenant... pense à toi. |6
À un effort que tu as fait aujourd'hui... même petit. |8
Tu peux te remercier, toi aussi. |10
Sens la chaleur de la gratitude dans ta poitrine... |8
Elle s'étend... doucement... |12
~15
Emporte cette chaleur avec toi... dans ta nuit, ou dans ta journée. |6
@pool close.soft 4-6 5
@bell
""",
    ),
    dict(
        id="med_confidence", cat="meditation", art="sun", title="Confiance en soi",
        desc="Réveiller ta force tranquille et ta valeur intrinsèque, avec des affirmations douces.",
        amb=[("ambient", 0.3)], pause=5.0, tail=30,
        script="""
@bell
~4
Bienvenue...
@pool intro.confidence 1-1 5
Installe-toi... le dos droit, les épaules ouvertes... et ferme les yeux. |6
@pool breath 0-5 5
@pool goal.confidence 0-11 7
Répète maintenant intérieurement, après moi... |4
Je suis assez, tel que je suis. |8
J'ai le droit de prendre ma place. |8
J'apprends et je grandis, chaque jour. |8
Je peux faire face à ce qui vient. |8
Je mérite le calme, et la joie. |10
~10
Sens comme ces phrases résonnent en toi... |8
Garde la lumière dorée au centre de ta poitrine... elle t'accompagne. |8
@pool close.wake 0-5 5
@bell
""",
    ),
    dict(
        id="med_acceptance", cat="meditation", art="clouds", title="Acceptation",
        desc="Faire la paix avec ce qui est, accueillir les émotions et adoucir la résistance intérieure.",
        amb=[("rain_soft", 0.3), ("pads", 0.15)], pause=5.0, tail=30,
        script="""
@bell
~4
Bienvenue...
Cette méditation est une invitation à accueillir ce qui est... sans lutter.
Accepter ne veut pas dire abandonner... cela veut dire arrêter de se battre contre la réalité de cet instant.
Installe-toi... et ferme les yeux. |6
@pool breath 5-10 5
@pool goal.emotions 0-11 7
Il y a peut-être en toi une situation difficile à accepter... |6
Tu n'as pas besoin de l'aimer... |5
Seulement de reconnaître : c'est comme ça, en ce moment. |8
Imagine que tu ouvres tes bras... pour faire de la place. |8
De la place pour ce qui est là... et pour toi, au milieu. |10
@pool goal.letgo 4-7 8
~15
@pool close.soft 1-6 5
@bell
""",
    ),
    dict(
        id="med_visualization", cat="meditation", art="garden", title="Visualisation positive",
        desc="Visualiser un lieu ressource et une journée idéale pour nourrir l'optimisme et la sérénité.",
        amb=[("stream", 0.3), ("ambient", 0.2)], pause=5.0, tail=30,
        script="""
@bell
~4
Bienvenue... Nous allons créer ensemble un lieu ressource, un endroit intérieur où tu pourras toujours revenir.
Installe-toi... et ferme les yeux. |6
@pool breath 0-5 5
@pool amb.nature 0-9 7
Ce lieu est à toi... personne ne peut y entrer sans ton invitation. |8
Choisis un détail que tu aimes particulièrement... une couleur, un son, un parfum. |10
Ce détail sera ta clé... il te ramènera ici quand tu en auras besoin. |8
Maintenant, depuis ce lieu paisible, imagine demain se dérouler parfaitement. |6
Tu te réveilles reposé... |6
Tu traverses ta journée avec calme et confiance... |6
Les difficultés sont là, peut-être... mais tu y réponds avec sérénité. |8
Le soir venu, tu te sens fier ou fière... et apaisé. |10
Garde cette image... elle prépare ton esprit à la vivre. |10
~10
@pool close.soft 0-6 5
@bell
""",
    ),

    # ================================================================== HISTOIRES
    dict(
        id="story_stars", cat="histoires", art="stars", title="Une promenade sous les étoiles",
        desc="Une marche lente dans la campagne endormie, jusqu'à une colline où le ciel tout entier s'offre à toi.",
        amb=[("night", 0.35), ("wind_soft", 0.15)], pause=3.5, tail=180,
        script="""
@bell
~4
Bonsoir... Installe-toi confortablement, et laisse-moi te raconter une histoire.
Ferme les yeux... et respire tranquillement. |6
C'est une nuit d'été, douce et silencieuse.
La journée s'est terminée il y a longtemps... et la campagne s'est endormie.
Tu sors de la maison, sans bruit... la porte se referme derrière toi avec un petit déclic.
L'air est tiède... il sent l'herbe coupée et le chèvrefeuille.
Un petit chemin de terre part devant toi... clair sous la lune.
Tu commences à marcher... lentement... sans aucun but.
Tes pas font un petit bruit doux sur la terre sèche.
De chaque côté, les champs dorment... les blés ondulent à peine, sous une brise légère. |5
Les grillons chantent... leur musique est régulière, comme une respiration.
Une chouette appelle, au loin... puis se tait.
Tu passes près d'un vieux muret de pierres, encore chaud du soleil de la journée.
Tu y poses la main... la pierre est douce, tiède... rassurante. |5
Le chemin monte doucement vers une colline.
Tu n'es pas pressé... tu prends ton temps.
À chaque pas, tu te sens plus léger... comme si la nuit te débarrassait, peu à peu, de la fatigue. |5
En haut de la colline, il y a un grand chêne... immense, paisible.
Sous ses branches, quelqu'un a laissé une couverture épaisse, comme si on t'attendait.
Tu t'y allonges... sur le dos... les bras ouverts. |5
Et là... le ciel tout entier s'offre à toi. |6
Des milliers d'étoiles... |5
Certaines brillent fort, comme des petites lanternes.
D'autres sont si lointaines qu'elles ne sont qu'une poussière de lumière.
La Voie lactée traverse le ciel, comme une rivière laiteuse. |5
Tu reconnais peut-être la Grande Ourse... ou peut-être pas... ça n'a aucune importance.
Tu regardes simplement... et tu respires. |6
Une étoile filante passe... si vite que tu te demandes si tu l'as vraiment vue.
Tu fais un vœu... tout doucement... rien que pour toi. |6
Le chêne murmure au-dessus de toi... ses feuilles chuchotent avec le vent.
La terre sous la couverture est encore chaude... elle te porte entièrement.
Tes yeux deviennent lourds... les étoiles se brouillent un peu... |5
Elles semblent se rapprocher... comme pour veiller sur toi. |6
Tu te sens minuscule... et en même temps, parfaitement à ta place. |6
Les grillons continuent leur chanson... |6
La brise passe... et repasse... |6
Ta respiration ralentit... au rythme de la nuit. |7
Tu pourrais rester là pour toujours... sous ce ciel tranquille. |7
Et peut-être que c'est ce que tu vas faire... cette nuit... en rêve. |8
Les étoiles veillent... |8
Dors bien... |8
""",
    ),
    dict(
        id="story_cabin", cat="histoires", art="cabin", title="Une cabane au milieu de la forêt",
        desc="Une cabane en bois, une pluie douce sur le toit, un feu qui crépite : un refuge pour s'endormir.",
        amb=[("rain_soft", 0.35), ("fire", 0.25)], pause=3.5, tail=180,
        script="""
@bell
~4
Bonsoir... Installe-toi bien au chaud. Je vais te raconter l'histoire d'une petite cabane.
Ferme les yeux... et laisse ton corps se reposer. |6
Au cœur d'une grande forêt, loin des routes et des villes, il y a une petite cabane en bois.
Ce soir, cette cabane est à toi.
Tu y arrives à la tombée de la nuit, par un sentier couvert d'aiguilles de pin.
Il commence à pleuvoir... une pluie fine, douce, régulière.
Tu pousses la porte... elle grince un peu, comme pour te souhaiter la bienvenue. |5
À l'intérieur, ça sent le bois, la cire... et la soupe qui mijote.
Un petit poêle ronronne dans un coin... ses flammes orange dansent derrière la vitre.
Tu retires tes chaussures... le plancher est tiède sous tes pieds. |5
Il y a un grand fauteuil, recouvert d'un plaid en laine.
Et près de la fenêtre, un lit, avec un édredon épais... et des oreillers moelleux. |5
Tu allumes une petite lampe... sa lumière est dorée, très douce.
Tu te sers un bol de soupe chaude... et tu t'assieds près du poêle.
La chaleur se pose sur tes mains... sur ton visage. |5
Dehors, la pluie tambourine doucement sur le toit.
Chaque goutte fait un petit bruit... tic... tic... tic... |5
Et toi, tu es au sec... au chaud... à l'abri de tout. |6
Personne ne sait que tu es ici... personne ne viendra te déranger.
Ton téléphone n'a pas de réseau... et c'est très bien comme ça. |5
Tu poses ton bol vide... et tu t'étires lentement.
Le bois du poêle craque... une bûche s'effondre doucement en braises.
Tu remets une dernière bûche... les flammes repartent, paisibles. |5
Puis tu te glisses dans le lit... |5
Les draps sont frais... puis ils se réchauffent... |5
L'édredon est lourd et doux... il t'enveloppe complètement. |6
Par la fenêtre, tu devines les silhouettes des grands sapins, qui se balancent dans la nuit.
La pluie continue... régulière... comme une berceuse. |6
Le feu crépite... de plus en plus doucement. |6
La lumière des braises danse sur le plafond en bois. |6
Tes paupières sont lourdes... |6
Tu n'as rien à faire demain matin... rien qui presse. |6
La forêt veille sur la cabane... la cabane veille sur toi. |7
Tu es exactement là où tu dois être. |7
La pluie... le feu... ton souffle... |8
Tout s'apaise... |8
Bonne nuit... |8
""",
    ),
    dict(
        id="story_ocean", cat="histoires", art="ocean", title="Une nuit au bord de l'océan",
        desc="Une maison sur la dune, les vagues dans la nuit et un phare qui veille sur ton sommeil.",
        amb=[("waves", 0.45), ("wind_soft", 0.15)], pause=3.5, tail=180,
        script="""
@bell
~4
Bonsoir... Ce soir, je t'emmène au bord de l'océan.
Ferme les yeux... et écoute les vagues. |6
Il y a une petite maison blanche, posée sur une dune, face à la mer.
Ses volets sont bleus... délavés par le sel et le soleil.
Tu viens d'y arriver... et tu poses ton sac dans l'entrée.
Le soleil est en train de se coucher... il colore le ciel d'orange, de rose, puis de violet. |5
Tu marches pieds nus jusqu'à la plage.
Le sable est encore chaud... il glisse doucement entre tes orteils. |5
Les vagues arrivent... s'étalent sur le sable... et repartent.
Encore... et encore... depuis toujours. |5
Tu t'assieds face à l'eau... et tu respires l'air salé. |5
Le soleil disparaît derrière l'horizon... et la première étoile s'allume.
Au loin, sur la pointe rocheuse, un phare commence à tourner.
Sa lumière balaie la mer... lentement... régulièrement.
Un... deux... trois... et la lumière revient. |6
La nuit tombe tout à fait... |5
La lune se lève, ronde et claire... et son reflet danse sur les vagues. |5
Tu remontes vers la maison... le sable est maintenant frais sous tes pieds.
Dans la chambre, les fenêtres sont ouvertes... les rideaux blancs se gonflent doucement avec la brise.
Tu t'allonges dans un lit aux draps de lin, frais et légers. |5
D'ici, tu entends toujours la mer... |5
Une vague... qui se retire... |5
Une autre... qui arrive... |5
Le phare continue sa ronde... sa lumière passe sur le plafond, très doucement... puis disparaît. |6
Tu respires au rythme des vagues... |6
Inspire quand la vague monte... |5
Expire quand elle se retire... |6
Ta respiration et l'océan ne font plus qu'un. |6
L'air sent le sel et la nuit... |6
Le vent fait chanter doucement les herbes de la dune. |6
Ton corps devient lourd... comme le sable mouillé. |7
Tes pensées s'éloignent... comme des bateaux à l'horizon. |7
L'océan te berce... |8
Le phare veille sur toi... |8
Bonne nuit... |8
""",
    ),
    dict(
        id="story_clouds", cat="histoires", art="clouds", title="Un voyage dans les nuages",
        desc="Allongé sur un nuage moelleux, tu dérives au-dessus des montagnes et des villes endormies.",
        amb=[("wind_soft", 0.3), ("pads", 0.25)], pause=3.5, tail=180,
        script="""
@bell
~4
Bonsoir... Installe-toi, ferme les yeux. Ce soir, nous partons en voyage... dans les nuages.
Prends une grande inspiration... et expire. |6
Imagine que tu te trouves dans un grand champ, au crépuscule.
L'herbe est douce... le ciel est rose et doré.
Un petit nuage descend lentement vers toi... blanc, rond, moelleux.
Il se pose juste devant toi... comme une invitation. |5
Tu poses une main dessus... il est doux comme du coton, et tiède comme une couette. |5
Tu t'y allonges... et il te soutient parfaitement. |5
Tout doucement, le nuage s'élève. |5
Le champ devient petit sous toi... les arbres ressemblent à des brindilles.
Tu n'as pas peur... le nuage te tient, tendrement. |5
Vous montez... lentement... dans l'air du soir.
Le vent est léger... il caresse ton visage. |5
En bas, tu vois une rivière qui brille... comme un ruban d'argent.
Des petits villages allument leurs lumières, une à une... comme des lucioles.
Le nuage dérive au-dessus des montagnes... leurs sommets enneigés sont teintés de rose. |5
Puis le soleil disparaît... et la nuit arrive, bleue et douce.
D'autres nuages flottent autour de toi... ils voyagent eux aussi, sans bruit. |5
Au-dessus, les étoiles s'allument... tout près, comme si tu pouvais les toucher. |5
La lune se lève... elle éclaire la mer de nuages d'une lumière argentée. |5
C'est un océan blanc, immense, silencieux... et tu flottes à sa surface. |6
Ton nuage ralentit... il se balance doucement... |6
Comme un hamac... d'un côté... puis de l'autre. |6
Tu t'enfonces un peu plus dans sa douceur... |6
Il t'enveloppe... il te tient chaud. |6
En bas, le monde dort... les villes, les forêts, les océans. |6
Et toi, tu flottes au-dessus de tout... léger, si léger. |7
Tes pensées deviennent des petits nuages... qui s'en vont, chacun de leur côté. |7
Il ne reste que le calme... et la lumière de la lune. |7
Le nuage continue son voyage... vers le pays des rêves. |8
Tu peux le laisser t'emmener... |8
Bonne nuit... |8
""",
    ),
    dict(
        id="story_garden", cat="histoires", art="garden", title="Un jardin secret",
        desc="Derrière une porte couverte de lierre, un jardin enchanté s'éveille à la lueur des lucioles.",
        amb=[("stream", 0.3), ("night", 0.25)], pause=3.5, tail=180,
        script="""
@bell
~4
Bonsoir... Installe-toi confortablement. Je vais te raconter l'histoire d'un jardin secret.
Ferme les yeux... et respire doucement. |6
Au bout d'un vieux chemin bordé de lavande, il y a un mur de pierre couvert de lierre.
Peu de gens le savent... mais dans ce mur, il y a une petite porte en bois.
Ce soir, tu as trouvé la clé... elle est petite, dorée, un peu tiède dans ta main. |5
Tu la glisses dans la serrure... elle tourne sans effort... clic. |5
La porte s'ouvre lentement... |5
Et derrière... un jardin, baigné de la lumière bleue du soir. |6
Tu fais un pas à l'intérieur... la porte se referme doucement derrière toi.
Ici, tout est calme... protégé... comme hors du temps. |5
Un petit sentier de pierres plates serpente entre les massifs de fleurs.
Des roses blanches... des jasmins... des chèvrefeuilles qui parfument l'air. |5
Tu marches lentement... chaque pierre est douce sous tes pieds.
Au centre du jardin, une petite fontaine chante... l'eau coule dans un bassin de pierre. |5
Tu t'en approches... et tu trempes tes doigts dans l'eau fraîche. |5
Des poissons dorés glissent lentement sous les nénuphars.
La nuit tombe... et soudain, des petites lumières apparaissent.
Des lucioles... des dizaines de lucioles, qui s'allument et s'éteignent dans l'herbe. |5
Elles dansent autour de toi... lentement... comme pour te saluer. |5
Près de la fontaine, il y a un banc de bois, recouvert de coussins moelleux.
Tu t'y allonges... la tête posée sur un coussin qui sent la lavande. |5
Au-dessus de toi, les branches d'un grand saule pleureur forment comme un toit.
Entre ses feuilles, tu aperçois les étoiles. |5
La fontaine murmure... |5
Les lucioles brillent... puis s'éteignent... puis brillent à nouveau... |6
Les fleurs de nuit s'ouvrent, une à une... elles libèrent leur parfum doux. |6
Ce jardin existe depuis très longtemps... il a vu passer des milliers de nuits paisibles.
Et ce soir, il veille sur toi. |6
Tu n'as rien à faire ici... seulement te reposer. |6
L'eau coule... ton souffle ralentit... |7
Les lucioles deviennent floues... comme des étoiles qui dansent. |7
Le jardin garde ton secret... et ton sommeil. |8
Tu peux revenir ici chaque nuit... la clé est à toi. |8
Bonne nuit... |8
""",
    ),
    dict(
        id="story_chalet", cat="histoires", art="snow", title="Une nuit dans un chalet enneigé",
        desc="Dehors la neige tombe en silence ; dedans, une cheminée, une couverture épaisse et un chocolat chaud.",
        amb=[("fire", 0.35), ("wind_soft", 0.2)], pause=3.5, tail=180,
        script="""
@bell
~4
Bonsoir... Ce soir, je t'emmène à la montagne, en plein hiver.
Ferme les yeux... et installe-toi bien au chaud. |6
Il est tard... et tu viens d'arriver dans un petit chalet en bois, perché au-dessus d'un village.
Dehors, la neige tombe à gros flocons... lents, silencieux.
Tu tapes tes bottes sur le seuil... la neige s'en détache en petites mottes blanches.
Tu entres... et la chaleur t'enveloppe aussitôt. |5
Une grande cheminée en pierre occupe le mur... un feu y danse, généreux et doré.
Les murs sont en bois clair... ils sentent le pin et la résine.
Tu retires ton manteau, ton bonnet, tes gants... et tu enfiles de grosses chaussettes en laine. |5
Sur la table basse, quelqu'un a préparé un chocolat chaud... avec un nuage de crème. |5
Tu prends la tasse entre tes mains... elle est brûlante, réconfortante. |5
Tu t'installes dans un grand canapé, face au feu... et tu remontes une couverture épaisse sur tes genoux.
Par la grande fenêtre, tu regardes la neige tomber.
Les flocons passent dans la lumière du chalet... puis disparaissent dans la nuit. |5
Au loin, les lumières du village scintillent... toutes petites. |5
Le monde extérieur est recouvert d'un grand manteau blanc... et tout est silencieux.
La neige étouffe tous les bruits... il n'y a plus de voitures, plus d'agitation. |5
Seulement le crépitement du feu... et ta respiration. |6
Tu bois ton chocolat, gorgée après gorgée... la chaleur descend dans ta poitrine. |5
Puis tu poses la tasse... et tu t'allonges dans le canapé. |5
La couverture est lourde et douce... tu t'y enfonces. |5
Le feu dessine des ombres dansantes sur le plafond. |5
Une bûche craque... des étincelles montent dans la cheminée... puis s'éteignent. |6
Dehors, la neige continue de tomber... flocon après flocon... |6
Elle recouvre doucement les sapins... les toits... les chemins. |6
Demain, tout sera blanc et neuf... mais pour l'instant, il n'y a que cette nuit tranquille. |6
Tes mains sont chaudes... tes pieds sont chauds... |6
Tes paupières deviennent lourdes, à regarder le feu. |7
La neige tombe... le feu crépite... |7
Le chalet te protège du froid... et du monde. |7
Tu peux t'endormir ici... en toute sécurité. |8
Bonne nuit... |8
""",
    ),

    # ================================================================== RÉVEIL
    dict(
        id="wake_gentle", cat="reveil", art="sun", title="Réveil doux",
        desc="Quelques minutes pour s'éveiller en douceur, s'étirer et poser une intention pour la journée.",
        amb=[("piano", 0.35), ("night", 0.0)], pause=4.0, tail=20,
        script="""
@bell
~6
Bonjour...
C'est le matin... prends ton temps, rien ne presse. |6
Reste encore un instant les yeux fermés... et sens ton corps, reposé par la nuit. |6
@pool goal.wake 0-11 5
@pool close.wake 1-6 4
@bell
""",
    ),
    dict(
        id="wake_serene", cat="reveil", art="sun", title="Matin serein",
        desc="Une courte méditation matinale pour accueillir la journée avec calme et clarté.",
        amb=[("stream", 0.3), ("ambient", 0.2)], pause=4.5, tail=20,
        script="""
@bell
~4
Bonjour... Bienvenue dans cette nouvelle journée.
Assieds-toi au bord du lit, ou reste allongé... comme tu le préfères. |5
@pool intro.wake 1-1 5
@pool breath 0-3 4
Sens l'air frais du matin entrer dans tes poumons... |5
Il réveille chaque cellule de ton corps, doucement. |6
@pool goal.wake 5-11 6
@pool close.wake 1-6 4
@bell
""",
    ),
]

# ---------------------------------------------------------------------------
# Séances ajoutées en version 1.1 (regroupées ensuite par catégorie).
TRACKS += [
    # ================================================================== SOMMEIL
    dict(
        id="sleep_again", cat="sommeil", art="moon", title="Se rendormir en pleine nuit",
        desc="Tu t'es réveillé au milieu de la nuit ? Une séance très douce, sans effort, pour retrouver le sommeil.",
        amb=[("rain_soft", 0.35), ("brown", 0.12)], pause=6.0, tail=300,
        script="""
~3
Tu t'es réveillé... et ce n'est pas grave.
Cela arrive à tout le monde... et la nuit est encore longue.
Tu n'as pas besoin de regarder l'heure... ni de compter les heures qu'il te reste. |7
Garde les yeux fermés... et laisse ton corps peser dans le lit. |8
Même éveillé, ton corps se repose... chaque minute de calme lui fait du bien. |8
Laisse ta respiration faire ce qu'elle veut... sans la guider. |8
Remarque seulement l'air qui entre... et l'air qui ressort. |9
Si une pensée arrive, tu peux lui dire doucement : pas maintenant... demain. |9
Et tu reviens au poids de ton corps... à la chaleur des draps. |9
Sens tes pieds... lourds et tièdes. |7
Tes jambes... qui s'abandonnent. |7
Ton ventre, qui se soulève... et redescend. |8
Tes épaules, qui s'enfoncent dans le matelas. |8
Ton visage... lisse, détendu... la mâchoire desserrée. |9
~12
Imagine que la nuit est une eau calme et tiède... et que tu flottes dessus, sans rien faire. |10
Elle te porte... elle t'a toujours porté. |10
Tu n'as pas à chercher le sommeil... c'est lui qui te cherche. |10
Il reviendra... comme la marée revient toujours. |12
~20
Rien à faire... |12
Rien à penser... |14
~30
Seulement te laisser porter... |15
~45
Bonne nuit... |5
""",
    ),
    dict(
        id="sleep_heavy", cat="sommeil", art="stars", title="Corps lourd, esprit léger",
        desc="Inspirée du training autogène : la lourdeur puis la chaleur gagnent tout le corps, jusqu'au sommeil.",
        amb=[("night", 0.3), ("pads", 0.2)], pause=6.0, tail=240,
        script="""
@bell
~4
Bonsoir...
Ce soir, nous allons simplement laisser ton corps devenir lourd... puis chaud... et le sommeil fera le reste.
Allonge-toi sur le dos... les bras le long du corps... et ferme les yeux. |7
@pool breath 0-3 5
Porte ton attention sur ton bras droit... |5
Mon bras droit est lourd... très lourd... |8
Il pèse sur le matelas... comme s'il était fait de sable tiède. |8
Mon bras gauche est lourd... très lourd... |8
Mes deux bras sont lourds... agréablement lourds. |9
Ma jambe droite est lourde... |8
Ma jambe gauche est lourde... |8
Mes bras et mes jambes sont lourds... |9
Tout mon corps est lourd... et calme. |10
~8
Maintenant, une douce chaleur se répand... |6
Mes mains sont chaudes... comme posées au soleil. |8
La chaleur remonte dans mes bras... |8
Mes pieds sont chauds... mes jambes sont chaudes. |9
Mon ventre est chaud et détendu... |9
Mon cœur bat calmement... régulièrement. |10
Ma respiration est tranquille... ça respire en moi. |10
Mon front est frais... léger... sans aucun pli. |10
~12
Tout mon corps est lourd... chaud... et parfaitement calme. |12
Et mon esprit devient léger... comme une plume qui descend lentement. |12
~20
Elle descend... |12
Encore... |14
~30
@pool close.sleep 0-6 9
""",
    ),

    # ================================================================== RELAXATION
    dict(
        id="relax_muscle", cat="relaxation", art="lotus", title="Relaxation musculaire progressive",
        desc="La méthode de Jacobson : contracter puis relâcher chaque groupe de muscles pour dissoudre les tensions.",
        amb=[("stream", 0.25), ("ambient", 0.2)], pause=5.0, tail=40,
        script="""
@bell
~4
Bienvenue...
Nous allons contracter, puis relâcher, chaque partie du corps... l'une après l'autre.
C'est en sentant la différence entre la tension et le relâchement que le corps apprend à se détendre.
Contracte sans jamais forcer... et si une zone est douloureuse, passe-la simplement.
Installe-toi confortablement... et ferme les yeux. |6
@pool breath 0-3 5
Commence par les mains... serre les poings, fort. |5
Tiens... encore un peu... |3
Et relâche, d'un seul coup. |8
Sens tes doigts s'ouvrir... la chaleur revenir dans tes paumes. |8
Maintenant les bras... plie les coudes et contracte les biceps. |5
Tiens... |4
Et relâche... laisse tes bras retomber, lourds. |9
Hausse les épaules vers les oreilles... le plus haut possible. |5
Tiens... |4
Et relâche... laisse-les descendre, loin des oreilles. |9
Le visage, à présent... plisse les yeux, fronce le nez, serre les dents. |5
Tiens... |3
Et relâche... ton front se lisse, ta mâchoire se desserre. |9
Inspire profondément et gonfle la poitrine... retiens l'air. |5
Et relâche en soufflant... longuement. |9
Rentre le ventre... contracte les abdominaux. |5
Tiens... |4
Et relâche... laisse ton ventre redevenir souple. |9
Serre les fesses et les cuisses... |5
Tiens... |4
Et relâche... sens tes jambes peser. |9
Enfin, ramène les pointes de pieds vers toi... contracte les mollets. |5
Tiens... |3
Et relâche... complètement. |10
~8
Parcours maintenant ton corps en entier... des pieds jusqu'à la tête. |8
S'il reste une tension quelque part, envoie-lui ton souffle... et laisse-la fondre. |10
Savoure cette sensation de corps délié... reposé. |12
~15
@pool close.soft 0-6 5
@bell
""",
    ),
    dict(
        id="relax_pause", cat="relaxation", art="clouds", title="Pause express",
        desc="Quatre minutes pour souffler au milieu de la journée, où que tu sois, les yeux ouverts ou fermés.",
        amb=[("wind_soft", 0.25), ("piano", 0.2)], pause=4.5, tail=15,
        script="""
@bell
~3
Bienvenue dans cette petite pause.
Où que tu sois, tu peux t'arrêter un instant... rien d'autre à faire pendant quelques minutes.
Pose tes deux pieds au sol... et laisse ton dos trouver un appui. |5
Relâche les épaules... desserre la mâchoire... adoucis ton regard, ou ferme les yeux. |6
Prends une grande inspiration par le nez... |4
Et souffle lentement par la bouche... comme à travers une paille. |6
Encore une fois... inspire... |4
Et souffle... tout doucement. |6
Remarque trois choses que tu entends, en ce moment... |10
Puis les points d'appui de ton corps... tes pieds, tes cuisses, ton dos. |8
Tu es là... tout simplement. |6
Ce qui t'attend peut attendre encore une minute. |6
Demande-toi : de quoi ai-je vraiment besoin, maintenant ? |8
Laisse venir la réponse... sans la juger. |10
Prends une dernière respiration ample... |5
Et reviens à ta journée... un peu plus léger, un peu plus présent. |5
@bell
""",
    ),

    # ================================================================== RESPIRATION
    dict(
        id="breath_box", cat="respiration", art="breath", title="Respiration carrée",
        desc="Quatre temps égaux : inspirer, retenir, expirer, retenir. Pour retrouver calme et concentration.",
        amb=[("pads", 0.25)], pause=4.0, tail=20,
        script="""
@bell
~3
Bienvenue... Nous allons pratiquer la respiration carrée.
Elle a quatre côtés égaux : tu inspires sur quatre temps... tu retiens sur quatre temps... tu expires sur quatre temps... et tu restes poumons vides sur quatre temps.
C'est une respiration qui stabilise... elle calme le corps tout en gardant l'esprit clair.
Assieds-toi, le dos droit mais détendu... et relâche les épaules. |5
Si les pauses te semblent trop longues, raccourcis-les... le confort passe avant tout. |4
Nous commençons. |3
@cycles 4 4 4 4 3 count
Très bien... continue, je te donne seulement les repères. |2
@cycles 4 4 4 4 6 cue
Encore quelques cycles, en silence... dessine le carré dans ton esprit. |2
@cycles 4 4 4 4 4 silent
@bell
Laisse maintenant ta respiration reprendre son rythme naturel... |6
Remarque comme ton esprit est posé... stable. |8
Tu peux utiliser cette respiration avant un moment important... ou chaque fois que tu en ressens le besoin. |4
""",
    ),
    dict(
        id="breath_wave", cat="respiration", art="ocean", title="Respiration en vague",
        desc="Un souffle ample et rond, calqué sur le va-et-vient de la mer, pour apaiser le système nerveux.",
        amb=[("waves", 0.4), ("pads", 0.12)], pause=4.5, tail=20,
        script="""
@bell
~3
Bienvenue... Nous allons respirer comme la mer respire.
L'inspiration est une vague qui monte sur le sable... l'expiration, une vague qui se retire.
Entre les deux, il y a un tout petit instant suspendu... où rien ne bouge.
Installe-toi... pose une main sur ton ventre si tu le souhaites... et ferme les yeux. |6
Écoute les vagues un instant... |8
Inspire sur quatre temps... garde l'air deux temps... expire sur six temps... et reste vide deux temps. |3
@cycles 4 2 6 2 2 count
C'est cela... laisse le souffle devenir rond, sans à-coups. |2
@cycles 4 2 6 2 7 cue
Continue seul... porté par le bruit de la mer. |2
@cycles 4 2 6 2 6 silent
Relâche maintenant le rythme... et laisse la mer respirer pour toi. |8
Sens le calme qui s'est installé... large, comme l'horizon. |10
@bell
""",
    ),

    # ================================================================== MÉDITATION
    dict(
        id="med_kindness", cat="meditation", art="heart", title="Bienveillance envers soi",
        desc="Une méditation d'amour bienveillant : s'offrir à soi-même, puis aux autres, des souhaits de paix.",
        amb=[("ambient", 0.3), ("pads", 0.12)], pause=5.5, tail=30,
        script="""
@bell
~4
Bienvenue...
Cette méditation est un moment de douceur... envers toi, d'abord.
Nous sommes souvent plus durs avec nous-mêmes qu'avec n'importe qui d'autre... ce soir, nous allons faire autrement.
Installe-toi... et ferme les yeux. |6
@pool breath 0-4 5
Pose une main sur ton cœur, si tu veux... et sens sa chaleur. |7
Imagine que tu te regardes avec les yeux de quelqu'un qui t'aime profondément. |8
Quelqu'un qui connaît tes efforts... tes fatigues... et qui ne te demande rien. |9
Répète intérieurement, après moi... |4
Que je sois en paix. |9
Que je sois en bonne santé. |9
Que je me sente en sécurité. |9
Que je sois doux avec moi-même. |10
~8
Pense maintenant à une personne que tu aimes... vois son visage. |7
Et offre-lui les mêmes souhaits... |4
Que tu sois en paix. |9
Que tu sois en bonne santé. |9
Que tu te sentes en sécurité. |9
Que ta vie soit douce. |10
~8
Élargis encore... à tous ceux qui, ce soir, ont besoin de douceur. |8
Que chacun trouve le repos... |9
Que chacun se sente aimé... |10
Et toi aussi, tu fais partie de ce « chacun ». |10
~12
Sens la chaleur sous ta main... elle est à toi, et elle ne s'épuise pas. |10
@pool close.soft 0-6 5
@bell
""",
    ),
    dict(
        id="med_thoughts", cat="meditation", art="clouds", title="Observer ses pensées",
        desc="Regarder passer les pensées comme des feuilles sur une rivière, sans s'y accrocher.",
        amb=[("stream", 0.35), ("ambient", 0.15)], pause=5.5, tail=30,
        script="""
@bell
~4
Bienvenue...
Nous n'allons pas essayer d'arrêter les pensées... ce serait comme vouloir arrêter une rivière avec les mains.
Nous allons simplement nous asseoir sur la rive... et les regarder passer.
Installe-toi... le dos droit, sans raideur... et ferme les yeux. |6
@pool breath 0-4 5
Imagine une rivière tranquille, qui coule devant toi... |7
Sur l'eau, de temps en temps, une feuille passe... portée par le courant. |8
Chaque fois qu'une pensée apparaît, dépose-la sur une feuille... |8
Et regarde-la s'éloigner... à son rythme. |10
Tu n'as pas à la pousser... ni à la retenir. |10
~12
Certaines pensées sont lourdes... elles passent lentement. Laisse-les passer lentement. |10
D'autres reviennent, encore et encore... dépose-les, encore et encore. |10
~15
Peut-être que tu t'es laissé emporter par une feuille... c'est normal. |7
Dès que tu le remarques, tu es déjà revenu sur la rive. |9
Et ce retour, c'est exactement cela, méditer. |10
~20
Remarque l'espace entre deux pensées... même très court. |10
Dans cet espace, il y a du calme... il a toujours été là. |12
~20
Tu n'es pas tes pensées... tu es celui, ou celle, qui les regarde passer. |10
@pool close.soft 0-6 5
@bell
""",
    ),
    dict(
        id="med_mountain", cat="meditation", art="mountain", title="La montagne intérieure",
        desc="Retrouver stabilité et solidité en s'identifiant à une montagne que rien n'ébranle.",
        amb=[("wind_trees", 0.3), ("pads", 0.18)], pause=5.5, tail=30,
        script="""
@bell
~4
Bienvenue...
Quand tout bouge autour de nous, nous pouvons retrouver en nous quelque chose de stable... comme une montagne.
Assieds-toi, le dos droit et digne... les mains posées sur les cuisses... et ferme les yeux. |7
@pool breath 0-4 5
Fais apparaître dans ton esprit une montagne... la plus belle que tu puisses imaginer. |8
Sa base est large, profondément ancrée dans la terre... son sommet s'élève dans le ciel. |9
Regarde-la... immense, immobile, tranquille. |9
Maintenant, laisse cette montagne entrer en toi... jusqu'à ce que ton corps et elle ne fassent plus qu'un. |9
Ton bassin et tes jambes sont sa base... solide, enracinée. |8
Ton dos est son versant... et ta tête, son sommet. |9
Tu es cette montagne... assise, stable, entière. |10
~10
Le jour se lève sur la montagne... puis la nuit tombe. Elle reste là. |9
Les saisons passent... la neige, les fleurs, la chaleur de l'été, les feuilles d'automne. |9
Elle ne s'accroche à aucune saison... et n'en repousse aucune. |10
Parfois, une tempête se lève... le vent hurle, la pluie frappe ses flancs. |8
La montagne ne prend pas la tempête pour elle... elle la laisse passer. |10
Et sous les nuages, elle reste exactement la même. |10
~15
Tes pensées, tes émotions, les événements de ta vie... sont la météo. |9
Toi, tu es la montagne. |12
~20
Tu peux revenir à cette solidité, chaque fois que tu en as besoin... elle ne te quitte pas. |9
@pool close.soft 0-6 5
@bell
""",
    ),
    dict(
        id="med_ground", cat="meditation", art="forest", title="Apaiser l'anxiété",
        desc="Un ancrage par les cinq sens pour sortir du tourbillon mental et revenir dans le présent.",
        amb=[("wind_soft", 0.25), ("ambient", 0.25)], pause=5.0, tail=30,
        script="""
@bell
~4
Bienvenue...
@pool intro.anxiety 0-0 5
L'anxiété nous emmène dans le futur... nos sens, eux, nous ramènent toujours ici.
Tu peux garder les yeux ouverts pour le début de cette séance. |5
Sens d'abord tes pieds sur le sol... appuie-les légèrement. |7
Le sol te porte... il est solide. |7
Regarde autour de toi... et nomme intérieurement cinq choses que tu vois. |5
Prends ton temps... une forme, une couleur, une lumière. |14
Maintenant, quatre choses que tu peux toucher... le tissu de tes vêtements, la surface sous tes mains. |5
Sens vraiment leur texture... leur température. |14
Trois sons que tu entends... proches, ou lointains. |14
Deux odeurs... ou simplement la fraîcheur de l'air dans tes narines. |12
Et une chose que tu goûtes... ou la simple sensation de ta bouche. |10
Tu es ici... dans cette pièce, dans ce corps, dans cet instant. |8
Et dans cet instant précis... tu es en sécurité. |9
Tu peux maintenant fermer les yeux, si tu le souhaites. |5
@pool goal.anxiety 0-8 7
Allonge ton expiration... comme si tu soufflais doucement sur une bougie, sans l'éteindre. |8
Encore... |8
Ce que tu ressens est une vague... et toutes les vagues finissent par redescendre. |10
~12
Tu as su revenir au calme... et tu sauras le refaire. |8
@pool close.soft 0-6 5
@bell
""",
    ),

    # ================================================================== HISTOIRES
    dict(
        id="story_train", cat="histoires", art="stars", title="Le train de nuit",
        desc="Bercé par le roulis d'un wagon-lit, tu traverses la campagne endormie jusqu'au bout de la nuit.",
        amb=[("brown", 0.22), ("night", 0.2), ("pads", 0.12)], pause=6.0, tail=240,
        script="""
@bell
~4
Bonsoir...
Ce soir, je t'emmène en voyage... à bord d'un train de nuit.
Installe-toi confortablement... ferme les yeux... et laisse-toi conduire. |7
C'est une petite gare de campagne, à la fin d'une soirée d'été. |6
Le quai est presque vide... l'air sent le foin coupé et la pierre encore tiède. |7
Au loin, deux lumières approchent... lentement, sans bruit. |7
Le train s'arrête devant toi dans un long soupir. |7
Tu montes... et le contrôleur, un vieil homme au sourire tranquille, t'indique ta cabine. |7
Elle est petite, lambrissée de bois sombre... éclairée par une lampe à l'abat-jour doré. |7
Le lit est déjà fait... des draps frais, une couverture de laine épaisse, un oreiller profond. |8
Tu poses ton sac... tu n'as rien d'autre à porter ce soir. |7
Par la fenêtre, le quai commence à glisser... tout doucement. |8
Le train a repris sa route... et tu ne l'as presque pas senti partir. |8
Tu t'allonges... et tu sens, sous toi, le roulis régulier des roues sur les rails. |8
Un rythme lent... toujours le même... comme un cœur paisible. |9
Dehors, la campagne défile sous la lune... |8
Des champs argentés... une rivière qui brille un instant... un bois sombre. |9
Un village endormi... avec une seule fenêtre encore allumée. |9
Puis de nouveau les champs... larges et silencieux. |9
Tu n'as pas à savoir où va le train... quelqu'un veille, à l'avant. |9
Les rails sont posés... le chemin est tracé... tu n'as qu'à te laisser porter. |10
Le wagon se balance légèrement... à gauche... à droite... |10
La lampe diffuse une lumière de miel... puis tu l'éteins. |9
Maintenant, il n'y a plus que la lune sur le plafond... et le bruit sourd du train. |10
Chaque tour de roue t'éloigne un peu plus de la journée. |10
Tes soucis sont restés sur le quai... de plus en plus petits... puis invisibles. |10
Tes paupières sont lourdes... ton corps suit le balancement. |10
Quelque part, le train traverse un pont... et le son devient plus creux, plus doux. |10
Puis un tunnel de forêt... où tout devient sombre et feutré. |12
~15
Le train roule... et roulera toute la nuit. |12
Demain, il arrivera au bord de la mer... mais tu as tout le temps. |12
~25
Pour l'instant... tu dors déjà presque. |14
~35
Bon voyage... et bonne nuit... |5
""",
    ),
    dict(
        id="story_lighthouse", cat="histoires", art="ocean", title="Le phare du bout du monde",
        desc="Une nuit dans la chambre ronde d'un vieux phare, tandis que la mer veille au pied des rochers.",
        amb=[("waves", 0.4), ("wind_soft", 0.18), ("fire", 0.12)], pause=6.0, tail=240,
        script="""
@bell
~4
Bonsoir...
Ce soir, nous allons passer la nuit dans un phare... tout au bout d'une île.
Allonge-toi... ferme les yeux... et écoute. |7
Le sentier longe la falaise, dans la lumière bleue du crépuscule. |7
L'herbe est rase et douce... le vent sent le sel et la bruyère. |7
Devant toi, le phare se dresse... blanc, solide, posé là depuis plus de cent ans. |8
Sa porte est lourde... elle s'ouvre sur un escalier de pierre qui tourne. |7
Tu montes sans te presser... une marche après l'autre. |8
À chaque palier, une petite fenêtre... et la mer, un peu plus bas. |8
Tout en haut, juste sous la lanterne, il y a une chambre ronde. |7
Un poêle y ronronne doucement... une bouilloire attend sur le côté. |8
Le lit est installé contre le mur courbe... garni d'édredons et de couvertures de marin. |8
Tu te glisses dessous... et tu sens la chaleur te gagner peu à peu. |8
Au-dessus de toi, la grande lampe tourne... lentement. |8
Son faisceau balaie la mer... s'éloigne... puis revient. |9
Il s'éloigne... puis revient... |10
Toujours au même rythme... depuis toujours. |10
En bas, les vagues se brisent sur les rochers... puis se retirent en chuchotant. |10
Elles ne peuvent pas t'atteindre ici... les murs sont épais comme des bras. |10
Le phare a traversé toutes les tempêtes... il veille. |9
Ce soir, tu n'as pas besoin de veiller... il le fait pour toi. |10
Au large, un bateau voit la lumière... et sait qu'il est sur la bonne route. |10
Tout est à sa place... la mer, la lampe, la nuit. |10
Le poêle craque un peu... la chaleur est douce sur ton visage. |10
Ta respiration s'accorde au passage de la lumière... |10
Elle passe... et tu inspires... |8
Elle s'éloigne... et tu expires... |10
~15
La lumière tourne... |12
La mer respire... |14
~25
Et toi, tu glisses dans le sommeil... bien à l'abri. |14
~35
Bonne nuit... |5
""",
    ),
    dict(
        id="story_lavender", cat="histoires", art="garden", title="Un soir en Provence",
        desc="La chaleur retombe sur les champs de lavande. Une vieille maison de pierre t'attend pour la nuit.",
        amb=[("night", 0.35), ("wind_soft", 0.2), ("piano", 0.1)], pause=6.0, tail=240,
        script="""
@bell
~4
Bonsoir...
Ce soir, je t'emmène dans le sud... à l'heure où la chaleur retombe.
Installe-toi... ferme les yeux... et respire lentement. |7
Tu marches sur un chemin de terre claire, entre deux champs de lavande. |7
Le soleil vient de passer derrière la colline... le ciel est rose, puis mauve. |8
Les rangées de fleurs s'étirent jusqu'à l'horizon... comme de longues vagues violettes. |8
Leur parfum monte dans l'air tiède... doux, propre, apaisant. |8
Tu laisses ta main frôler les épis en marchant... |8
Les dernières abeilles rentrent... et les grillons commencent leur chant. |8
Au bout du chemin, il y a une vieille maison de pierre, aux volets bleus. |7
Devant la porte, un grand tilleul... et sous le tilleul, un banc. |8
Tu t'assieds un moment... la pierre du mur est encore chaude dans ton dos. |8
Quelque part, une fontaine coule... un filet d'eau, régulier, tranquille. |9
Le ciel s'assombrit lentement... et la première étoile apparaît. |9
Puis une autre... et encore une autre. |9
L'air fraîchit un peu... juste ce qu'il faut. |8
Tu entres dans la maison... elle sent la cire, le linge propre et la lavande séchée. |8
L'escalier de bois craque gentiment sous tes pas. |8
Dans la chambre, la fenêtre est ouverte sur la nuit... un rideau blanc bouge à peine. |9
Les draps sont en lin... frais, un peu rêches, merveilleusement propres. |9
Tu t'allonges... et tout ton corps soupire. |10
Par la fenêtre entre le chant des grillons... et le parfum des champs. |10
Tu n'as rien à faire demain matin... rien de pressé. |10
Les collines dorment... les champs dorment... la maison dort. |10
Le chant des grillons devient lointain... comme une berceuse très ancienne. |12
~15
Ta respiration est lente... parfumée... |12
Tes pensées se dispersent, comme des graines dans le vent du soir... |14
~25
Il ne reste que la douceur de la nuit... |14
~35
Dors bien... |8
Bonne nuit... |5
""",
    ),

    # ================================================================== RÉVEIL
    dict(
        id="wake_energy", cat="reveil", art="sun", title="Élan du matin",
        desc="Respiration tonique, étirements et intention : cinq minutes pour démarrer la journée avec entrain.",
        amb=[("stream", 0.25), ("piano", 0.3)], pause=4.0, tail=15,
        script="""
@bell
~4
Bonjour...
Une nouvelle journée commence... et elle n'a encore jamais été vécue par personne.
Assieds-toi au bord du lit, ou tiens-toi debout... les pieds bien à plat. |5
Étire tes bras vers le plafond... grandis-toi, comme si un fil tirait le sommet de ta tête. |6
Et relâche... en soufflant. |5
Fais rouler doucement tes épaules vers l'arrière... |6
Incline la tête d'un côté... puis de l'autre. |7
Maintenant, réveillons le souffle... inspire par le nez, largement... |4
Et expire d'un seul coup, par la bouche. |4
Encore... inspire, en ouvrant la poitrine... |4
Et souffle... |4
Une dernière fois... inspire toute la lumière du matin... |4
Et laisse partir ce qu'il reste de la nuit. |6
Sens l'énergie qui circule... dans tes mains, dans tes jambes, derrière tes yeux. |7
Pense à une chose, même toute petite, qui te fait plaisir aujourd'hui. |8
Puis choisis un mot pour ta journée... calme, courage, joie, patience... celui qui te vient. |9
Garde-le avec toi... comme un caillou lisse au fond de ta poche. |6
@pool close.wake 2-6 4
Belle journée à toi. |3
@bell
""",
    ),
]
_CAT_ORDER = [c[0] for c in CATEGORIES]
TRACKS.sort(key=lambda t: _CAT_ORDER.index(t["cat"]))

# Citations apaisantes (affichées dans l'application).
QUOTES = [
    ("La nuit porte conseil.", "Proverbe français"),
    ("Le calme est la plus haute forme de la force.", "Proverbe"),
    ("Respire. Tu es exactement là où tu dois être.", "NOCTÉA"),
    ("Il faut du temps pour que la neige fonde, et de la douceur pour que le cœur s'ouvre.", "NOCTÉA"),
    ("Ce n'est pas parce que les choses sont difficiles que nous n'osons pas, c'est parce que nous n'osons pas qu'elles sont difficiles.", "Sénèque"),
    ("Hâte-toi de bien vivre et songe que chaque jour est à lui seul une vie.", "Sénèque"),
    ("Le bonheur est parfois caché dans l'inconnu.", "Victor Hugo"),
    ("Même la nuit la plus sombre prendra fin et le soleil se lèvera.", "Victor Hugo"),
    ("La patience est amère, mais son fruit est doux.", "Jean-Jacques Rousseau"),
    ("Il y a toujours des fleurs pour ceux qui veulent les voir.", "Henri Matisse"),
    ("On ne voit bien qu'avec le cœur.", "Antoine de Saint-Exupéry"),
    ("Chaque soir, déposer la journée. Chaque matin, accueillir le jour.", "NOCTÉA"),
    ("La douceur est invincible.", "Marc Aurèle"),
    ("Tu as le pouvoir sur ton esprit, pas sur les événements. Prends-en conscience, et tu trouveras la force.", "Marc Aurèle"),
    ("Le sommeil est la meilleure des méditations.", "Dalaï-lama"),
    ("Sois doux avec toi-même : tu fais de ton mieux.", "NOCTÉA"),
    ("Les étoiles ne brillent que dans l'obscurité.", "Proverbe"),
    ("Le silence est un ami qui ne trahit jamais.", "Confucius"),
    ("Rien n'est permanent, sauf le changement.", "Héraclite"),
    ("Là où tu poses ton attention, ta vie fleurit.", "NOCTÉA"),
]
