#!/usr/bin/env python3
"""LOST KINGDOM 3.2.0 — writes the three big mods shipped with the game (mods 2.0: new creatures and new items).

  assets/mods/bestiaire-legendaire/   30 new creatures in every region, each with its colour, size, loot and lore
  assets/mods/arsenal-des-legendes/   24 new weapons, each dropped by a creature of the bestiary or the game
  assets/mods/invasion-des-ombres/    shadow versions of the creatures roam every region at night; rich rewards

Usage: python3 -I tools/make_big_mods.py   (then the mods appear, disabled, in the game's Mods menu)
"""
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "mods")

# id, base, name, region, tint, scale, hp, atk, level, spawn, night, description
BEASTS = [
    ("loup_cendre", "wolf", "Loup de cendre", "ashen", "#ff7030", 1.15, 140, 18, 12, 1.5, 0, "Né dans les braises des Ashen Lands, il laisse des empreintes fumantes."),
    ("loup_spectral", "snow_wolf", "Loup spectral", "ombres", "#8aa0ff", 1.2, 160, 20, 11, 1.2, 1, "Il ne hurle pas : c'est le silence qui l'annonce."),
    ("loup_jade", "steppe_wolf", "Loup de jade", "jungle", "#40e090", 1.25, 300, 34, 21, 1.0, 0, "Son pelage a la couleur des feuilles mouillées."),
    ("sanglier_titan", "boar", "Sanglier titanesque", "greenhaven", "#8a6040", 2.1, 260, 22, 6, 0.4, 0, "Les paysans de Brindlemere jurent qu'il a renversé un moulin."),
    ("sanglier_braise", "boar", "Sanglier de braise", "ashen", "#ff5020", 1.3, 180, 22, 11, 1.0, 0, "Ses défenses rougeoient comme du fer à la forge."),
    ("ours_glacier", "ash_bear", "Ours des glaciers", "frostheim", "#e8f4ff", 1.35, 260, 26, 9, 0.9, 0, "Blanc comme la neige qu'il secoue de ses épaules."),
    ("ours_mousse", "ash_bear", "Ours moussu", "elderwood", "#5a8a3a", 1.4, 240, 24, 10, 0.8, 0, "De la mousse pousse sur son dos. Des champignons aussi."),
    ("araignee_cristal", "giant_spider", "Araignée de cristal", "highcrowns", "#a0f0ff", 1.3, 220, 26, 14, 0.8, 0, "Sa toile chante quand le vent la traverse."),
    ("araignee_sang", "forest_spider", "Veuve de sang", "ombres", "#c01030", 1.4, 200, 28, 10, 0.9, 1, "On ne la voit qu'à la lueur de ses yeux."),
    ("golem_ambre", "stone_golem", "Golem d'ambre", "ambre", "#ffb030", 1.3, 520, 34, 17, 0.6, 0, "Un scarabée dort au cœur de sa poitrine."),
    ("golem_obsidienne", "stone_golem", "Golem d'obsidienne", "ashen", "#201820", 1.4, 560, 36, 14, 0.6, 0, "Ses coups font jaillir des étincelles de verre noir."),
    ("golem_aurore", "crystal_golem", "Golem d'aurore", "aurore", "#ff80e0", 1.25, 640, 42, 23, 0.7, 0, "Il change de couleur au rythme des aurores."),
    ("chevalier_fantome", "dark_knight", "Chevalier fantôme", "ombres", "#a0b0ff", 1.1, 360, 32, 12, 0.7, 1, "Il garde encore un château qui n'existe plus."),
    ("chevalier_soleil", "sun_guard", "Champion du Soleil", "ambre", "#ffd060", 1.2, 480, 40, 19, 0.6, 0, "Le dernier garde de Sekhmar à ne pas avoir fui."),
    ("seigneur_gobelin", "goblin", "Seigneur gobelin", "greenhaven", "#4a8a2a", 1.5, 160, 16, 5, 0.5, 0, "Il porte une couronne volée. Elle lui va mal."),
    ("chaman_ombre", "goblin_shaman", "Chaman des ombres", "ombres", "#6030a0", 1.2, 180, 22, 9, 0.8, 1, "Ses fétiches murmurent des noms."),
    ("troll_lave", "frost_troll", "Troll de lave", "ashen", "#ff4010", 1.4, 600, 38, 15, 0.6, 0, "Il se baigne dans la lave pour se réchauffer."),
    ("troll_marais", "frost_troll", "Troll des marais", "ombres", "#3a5a2a", 1.3, 520, 32, 11, 0.7, 0, "Il sent la vase et la vieille soupe."),
    ("squelette_roi", "skeleton", "Squelette couronné", "ombres", "#ffd870", 1.3, 260, 26, 12, 0.5, 1, "Un ancien roi de Morneval, toujours en guerre."),
    ("momie_reine", "mummy", "Reine momifiée", "ambre", "#d8c088", 1.2, 420, 36, 18, 0.5, 1, "Ses bandelettes sont cousues de fil d'or."),
    ("salamandre_geante", "salamander", "Salamandre géante", "ashen", "#ff9020", 1.8, 340, 30, 13, 0.6, 0, "Longue comme un chariot. Rapide comme un chat."),
    ("scorpion_empereur", "sand_scorpion", "Scorpion empereur", "ambre", "#3a2010", 1.7, 420, 38, 17, 0.6, 0, "Son dard perce l'acier comme du parchemin."),
    ("crabe_titan", "sea_crab", "Crabe titan", "corail", "#e05030", 2.0, 480, 34, 14, 0.5, 0, "Sa carapace sert de toit aux mouettes."),
    ("sirene_abysses", "siren", "Sirène des abysses", "corail", "#2040a0", 1.15, 300, 30, 16, 0.7, 1, "Son chant monte des profondeurs, jamais de la surface."),
    ("wyverne_givre", "drakeling", "Wyverne de givre", "frostheim", "#a8e0ff", 1.4, 380, 34, 13, 0.5, 0, "Elle couve ses œufs dans les glaciers."),
    ("wyverne_jungle", "drakeling", "Wyverne émeraude", "jungle", "#30c060", 1.5, 520, 42, 22, 0.5, 0, "Elle chasse les jaguars. Les jaguars le savent."),
    ("cerf_aurore", "spirit_stag", "Cerf d'aurore", "aurore", "#ffe0ff", 1.3, 420, 36, 22, 0.7, 1, "Ses bois portent des étoiles."),
    ("cavalier_cendre", "steppe_rider", "Cavalier de cendre", "steppes", "#504040", 1.15, 460, 40, 19, 0.6, 1, "Il chevauche la fumée des feux de camp éteints."),
    ("aurochs_titan", "aurochs", "Aurochs titanesque", "steppes", "#5a3a20", 1.8, 820, 46, 21, 0.4, 0, "Le sol tremble trois secondes avant qu'il n'apparaisse."),
    ("geant_cimes", "rock_troll", "Géant des cimes", "titans", "#8a8a9a", 1.6, 1200, 58, 27, 0.4, 0, "Un cousin du Roi-Géant. Il ne parle à personne."),
]

# id, base, name, rarity, atk, crit, elem_dmg, colour, dropped by, chance, description
ARMS = [
    ("lame_cendre", "sword_ember", "Lame de cendre", "epique", 32, 0.09, 12, "#ff7030", "loup_cendre", 0.08, "Elle fume encore quand on la sort du fourreau."),
    ("croc_spectral", "dagger_shadow", "Croc spectral", "legendaire", 30, 0.24, 14, "#8aa0ff", "loup_spectral", 0.06, "Une dague taillée dans un rayon de lune."),
    ("hache_glacier", "axe_nordic", "Hache des glaciers", "epique", 34, 0.08, 12, "#e8f4ff", "ours_glacier", 0.08, "Le froid mord avant la lame."),
    ("marteau_mousse", "hammer_iron", "Marteau moussu", "rare", 30, 0.05, 0, "#5a8a3a", "ours_mousse", 0.1, "Lourd, humide, et terriblement efficace."),
    ("arc_cristal", "bow_elder", "Arc de cristal", "legendaire", 34, 0.12, 14, "#a0f0ff", "araignee_cristal", 0.06, "Ses flèches sifflent une note pure."),
    ("dague_sang", "dagger_viper", "Dague de la veuve", "legendaire", 32, 0.26, 16, "#c01030", "araignee_sang", 0.06, "Une goutte suffit."),
    ("poing_ambre", "hammer_titan", "Poing d'ambre", "legendaire", 54, 0.06, 16, "#ffb030", "golem_ambre", 0.05, "Un scarabée dort dans la tête du marteau."),
    ("lame_obsidienne", "sword_steel", "Lame d'obsidienne", "epique", 36, 0.1, 0, "#201820", "golem_obsidienne", 0.08, "Coupante comme du verre, solide comme la nuit."),
    ("sceptre_aurore", "staff_aurora", "Sceptre d'aurore vive", "mythique", 44, 0.1, 24, "#ff80e0", "golem_aurore", 0.05, "Il projette des aurores sur les murs."),
    ("epee_fantome", "longsword_fallen", "Épée du chevalier fantôme", "mythique", 52, 0.11, 18, "#a0b0ff", "chevalier_fantome", 0.05, "Elle traverse les boucliers comme de la brume."),
    ("lance_soleil", "spear_crown", "Lance du Champion", "legendaire", 44, 0.1, 16, "#ffd060", "chevalier_soleil", 0.06, "Elle brille même à minuit."),
    ("couronne_gobeline", "dagger_iron", "Couteau du Seigneur gobelin", "rare", 18, 0.18, 0, "#4a8a2a", "seigneur_gobelin", 0.2, "Rouillé, ébréché, mais il porte bonheur."),
    ("baton_ombre", "staff_ember", "Bâton des ombres", "epique", 32, 0.08, 14, "#6030a0", "chaman_ombre", 0.08, "Les fétiches qui y pendent murmurent."),
    ("massue_lave", "hammer_titan", "Massue de lave", "legendaire", 56, 0.05, 18, "#ff4010", "troll_lave", 0.06, "Elle laisse des traces de roche fondue."),
    ("sceptre_couronne", "staff_abyss", "Sceptre du roi squelette", "mythique", 48, 0.1, 22, "#ffd870", "squelette_roi", 0.05, "Il commande encore à des armées endormies."),
    ("fouet_reine", "dagger_tide", "Ruban de la reine momifiée", "legendaire", 34, 0.22, 16, "#d8c088", "momie_reine", 0.06, "Des bandelettes d'or tressées en lame."),
    ("lame_salamandre", "scimitar_dune", "Cimeterre de la salamandre", "epique", 38, 0.1, 14, "#ff9020", "salamandre_geante", 0.08, "Il ondule comme une flamme."),
    ("dard_empereur", "spear_trident", "Dard de l'empereur", "legendaire", 46, 0.14, 18, "#3a2010", "scorpion_empereur", 0.06, "Le dard du Scorpion empereur, monté sur une hampe."),
    ("pince_titan", "axe_steel", "Pince du crabe titan", "epique", 40, 0.07, 0, "#e05030", "crabe_titan", 0.08, "Il faut deux mains. Et du courage."),
    ("harpe_abysses", "bow_coral", "Arc des abysses", "legendaire", 40, 0.12, 18, "#2040a0", "sirene_abysses", 0.06, "Sa corde chante comme une sirène."),
    ("aile_givre", "sword_aurora", "Aile de givre", "legendaire", 46, 0.12, 18, "#a8e0ff", "wyverne_givre", 0.06, "Taillée dans une écaille de wyverne."),
    ("bois_aurore", "bow_aurora", "Bois d'aurore", "mythique", 48, 0.13, 24, "#ffe0ff", "cerf_aurore", 0.05, "Un arc fait d'un bois de cerf d'aurore."),
    ("faux_cendre", "axe_warlord", "Faux du cavalier de cendre", "mythique", 56, 0.1, 20, "#504040", "cavalier_cendre", 0.05, "Elle fauche la fumée et ce qu'elle cache."),
    ("massue_geant", "hammer_giant", "Massue du géant des cimes", "ancestral", 72, 0.07, 24, "#8a8a9a", "geant_cimes", 0.05, "Personne n'avait réussi à la soulever. Jusqu'à toi."),
]

# shadow invasion: id, base, name, region
SHADOWS = [
    ("ombre_loup", "wolf", "Loup des ombres", "greenhaven"), ("ombre_bandit", "bandit", "Bandit des ombres", "greenhaven"),
    ("ombre_draugr", "draugr", "Draugr des ombres", "frostheim"), ("ombre_troll", "frost_troll", "Troll des ombres", "frostheim"),
    ("ombre_chevalier", "ash_knight", "Chevalier des ombres", "ashen"), ("ombre_salamandre", "salamander", "Salamandre des ombres", "ashen"),
    ("ombre_sylvain", "blight_treant", "Sylvain des ombres", "elderwood"), ("ombre_cerf", "spirit_stag", "Cerf des ombres", "elderwood"),
    ("ombre_spectre", "specter", "Spectre noir", "ombres"), ("ombre_golem", "stone_golem", "Golem des ombres", "highcrowns"),
    ("ombre_pilleur", "dust_raider", "Pilleur des ombres", "ambre"), ("ombre_noye", "drowned", "Noyé des ombres", "corail"),
    ("ombre_cavalier", "steppe_rider", "Cavalier des ombres", "steppes"), ("ombre_jaguar", "jaguar", "Jaguar des ombres", "jungle"),
    ("ombre_esprit", "aurora_wraith", "Esprit des ombres", "aurore"), ("ombre_wyverne", "mountain_wyvern", "Wyverne des ombres", "titans"),
]


def write(mod, name, desc, lines):
    d = os.path.join(ROOT, mod); os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "mod.txt"), "w").write(f"name = {name}\nauthor = Superessence Studio\nversion = 2.0\ndescription = {desc}\n")
    open(os.path.join(d, "data.txt"), "w").write("\n".join(lines) + "\n")
    print(f"{mod}: {len([l for l in lines if l and not l.startswith('#')])} lines")


def main():
    L = ["# Bestiaire légendaire (mod livré avec LOST KINGDOM 3.2.0, format 2.0)", "# 30 créatures nouvelles dans toutes les régions, chacune avec sa couleur, sa taille et son butin."]
    for (i, base, nm, reg, tint, sc, hp, atk, lvl, sp, night, desc) in BEASTS:
        L += [f"enemy.add = {i} | {base} | {nm} | {desc}", f"enemy.{i}.region = {reg}", f"enemy.{i}.tint = {tint}", f"enemy.{i}.scale = {sc}",
              f"enemy.{i}.hp = {hp}", f"enemy.{i}.atk = {atk}", f"enemy.{i}.level = {lvl}", f"enemy.{i}.spawn = {sp}",
              f"enemy.{i}.xp = {int(8 + lvl * lvl * 3.6)}", f"enemy.{i}.gold = {2 + lvl * 3}", f"enemy.{i}.loot = star_shard | 0.12"]
        if night: L.append(f"enemy.{i}.night = 1")
        L.append("")
    L += ["lore.add = bestiaire_legendaire | Le Bestiaire légendaire | Depuis le réveil des totems, des créatures qu'aucun chasseur n'avait décrites parcourent le royaume. Certaines sont des cousines de bêtes connues, d'autres semblent nées des régions elles-mêmes : la cendre, le givre, l'ambre, l'aurore."]
    write("bestiaire-legendaire", "Bestiaire légendaire", "30 créatures nouvelles dans toutes les régions : loups de cendre, golems d'ambre, wyvernes, géants des cimes... Chacune a sa couleur, sa taille et son butin.", L)

    A = ["# Arsenal des légendes (mod livré avec LOST KINGDOM 3.2.0, format 2.0)", "# 24 armes nouvelles. Chacune tombe d'une créature précise (activez aussi le Bestiaire légendaire pour toutes les obtenir).",
         ""]
    for (i, base, nm, rar, atk, crit, el, col, drop, ch, desc) in ARMS:
        A += [f"item.add = {i} | {base} | {nm} | {desc}", f"item.{i}.rarity = {rar}", f"item.{i}.atk = {atk}", f"item.{i}.crit = {crit}",
              f"item.{i}.elem_dmg = {el}", f"item.{i}.color = {col}", f"item.{i}.value = {atk * 40}"]
        A.append(f"enemy.{drop}.loot = {i} | {ch}")
        A.append("")
    # without the bestiary, the arms still drop from the game's own creatures
    fallback = {"loup_cendre": "ember_elemental", "loup_spectral": "specter", "ours_glacier": "frost_troll", "ours_mousse": "blight_treant",
                "araignee_cristal": "giant_spider", "araignee_sang": "forest_spider", "golem_ambre": "sandstone_golem", "golem_obsidienne": "ash_knight",
                "golem_aurore": "crystal_golem", "chevalier_fantome": "dark_knight", "chevalier_soleil": "sun_guard", "seigneur_gobelin": "goblin",
                "chaman_ombre": "goblin_shaman", "troll_lave": "ember_elemental", "squelette_roi": "skeleton", "momie_reine": "mummy",
                "salamandre_geante": "salamander", "scorpion_empereur": "sand_scorpion", "crabe_titan": "sea_crab", "sirene_abysses": "siren",
                "wyverne_givre": "drakeling", "cerf_aurore": "aurora_wraith", "cavalier_cendre": "steppe_rider", "geant_cimes": "rock_troll"}
    A = [l if not (l.startswith("enemy.") and ".loot" in l) else l for l in A]
    out = []
    for l in A:
        if l.startswith("enemy.") and ".loot = " in l:
            enemy = l.split(".")[1]
            item, chance = l.split("= ")[1].split(" | ")
            out.append(f"enemy.{fallback[enemy]}.loot = {item} | {float(chance) / 3:.3f}")
        else:
            out.append(l)
    out.append("# a gift to start with (after its item.add)")
    out.append("game.start_item = couronne_gobeline | 1")
    write("arsenal-des-legendes", "Arsenal des légendes", "24 armes nouvelles, de rares à ancestrales, chacune lâchée par une créature précise. Commence avec le Couteau du Seigneur gobelin.", out)

    S = ["# Invasion des ombres (mod livré avec LOST KINGDOM 3.2.0, format 2.0)", "# La nuit, des versions d'ombre des créatures parcourent chaque région. Plus dures, mais riches.",
         "game.xp = 1.4", "game.gold = 1.6", ""]
    for (i, base, nm, reg) in SHADOWS:
        S += [f"enemy.add = {i} | {base} | {nm} | Une ombre de {nm.split(' des ombres')[0].lower()}, venue avec la nuit.", f"enemy.{i}.region = {reg}",
              f"enemy.{i}.tint = #3a2a5a", f"enemy.{i}.scale = 1.2", f"enemy.{i}.spawn = 2", f"enemy.{i}.night = 1",
              f"enemy.{i}.loot = star_shard | 0.25", f"enemy.{i}.loot = elixir_titan | 0.05", ""]
    S += ["lore.add = invasion_ombres | L'Invasion des ombres | Chaque nuit, les ombres prennent la forme de ce qu'elles touchent. Les veilleurs de Gravemoor disent qu'elles cherchent les totems endormis."]
    write("invasion-des-ombres", "Invasion des ombres", "La nuit, 16 créatures d'ombre parcourent toutes les régions. Plus dures, mais elles lâchent des éclats d'étoile et des élixirs. +40 % d'XP, +60 % d'or.", S)

    idx = os.path.join(ROOT, "index.txt")
    names = [l.strip() for l in open(idx) if l.strip()]
    for m in ("bestiaire-legendaire", "arsenal-des-legendes", "invasion-des-ombres"):
        if m not in names: names.append(m)
    open(idx, "w").write("\n".join(names) + "\n")


if __name__ == "__main__":
    main()
