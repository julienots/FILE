// LOST KINGDOM — content database. Text is in French (game language).
#include "Data.h"
#include <unordered_map>
#include <mutex>
#include <cmath>
#include <cstdlib>

namespace lk {

const char* elementName(Element e) {
    static const char* n[] = {"Physique", "Feu", "Glace", "Foudre", "Nature", "Ombre", "Lumière", "Arcane"};
    return n[(int)e < 8 ? (int)e : 0];
}
vec3 elementColor(Element e) {
    switch (e) {
    case Element::Fire: return {1.f, 0.45f, 0.12f};
    case Element::Ice: return {0.45f, 0.8f, 1.f};
    case Element::Lightning: return {0.75f, 0.75f, 1.f};
    case Element::Nature: return {0.45f, 1.f, 0.4f};
    case Element::Shadow: return {0.6f, 0.25f, 0.9f};
    case Element::Light: return {1.f, 0.9f, 0.55f};
    case Element::Arcane: return {0.8f, 0.45f, 1.f};
    default: return {1.f, 0.85f, 0.6f};
    }
}
const char* rarityName(Rarity r) {
    static const char* n[] = {"Commun", "Rare", "Épique", "Légendaire", "Mythique", "Ancestral"};
    return n[(int)r];
}
vec3 rarityColor(Rarity r) {
    static const uint32_t c[] = {0xd8d2c4, 0x4f9cf0, 0xb36cf0, 0xf2a03a, 0xf04a4a, 0x3fe0c8};
    return hexColor(c[(int)r]);
}
float rarityMult(Rarity r) { static const float m[] = {1.f, 1.18f, 1.4f, 1.7f, 2.05f, 2.5f}; return m[(int)r]; }
const char* slotName(int s) {
    static const char* n[] = {"Arme", "Main gauche", "Tête", "Torse", "Mains", "Pieds", "Anneau", "Amulette"};
    return s >= 0 && s < SL_COUNT ? n[s] : "";
}
int slotForKind(ItemKind k) {
    switch (k) {
    case ItemKind::Weapon: return SL_WEAPON;
    case ItemKind::Shield: return SL_OFFHAND;
    case ItemKind::Head: return SL_HEAD;
    case ItemKind::Chest: return SL_CHEST;
    case ItemKind::Hands: return SL_HANDS;
    case ItemKind::Feet: return SL_FEET;
    case ItemKind::Ring: return SL_RING;
    case ItemKind::Amulet: return SL_AMULET;
    default: return -1;
    }
}
const char* stationName(int s) {
    static const char* n[] = {"Partout", "Forge", "Table d'alchimie", "Feu de camp", "Établi"};
    return n[s];
}
const char* branchName(int b) {
    static const char* n[] = {"Guerrier", "Archer", "Assassin", "Mage", "Paladin", "Chasseur"};
    return n[b];
}
const char* factionName(int f) {
    static const char* n[] = {"Gardiens de Greenhaven", "Clans de Frostheim", "Cercle d'Elderwood", "Guilde des Mineurs", "Ordre de Highspire", "Veilleurs de Gravemoor"};
    return f >= 0 && f < FA_COUNT ? n[f] : "";
}

// ============================================================================================ items
static std::vector<ItemDef> gItems;
static std::unordered_map<std::string, int> gItemIdx;
static std::once_flag gItemsOnce;

static ItemDef& addItem(const std::string& id, const std::string& name, ItemKind k, Rarity r, int value, const std::string& desc) {
    ItemDef d; d.id = id; d.name = name; d.kind = k; d.rarity = r; d.value = value; d.desc = desc;
    gItems.push_back(d);
    gItemIdx[id] = (int)gItems.size() - 1;
    return gItems.back();
}

static void buildItems() {
    // ---- weapons -----------------------------------------------------------------------------
    struct W { const char* id; const char* name; WeaponType t; int tier; Rarity r; float atk, crit, speed; Element e; float ed; const char* desc; };
    const W ws[] = {
        {"sword_rusty", "Épée rouillée", WeaponType::Sword, 0, Rarity::Common, 8, 0.03f, 0, Element::Physical, 0, "Une lame fatiguée, trouvée sur la grève."},
        {"sword_iron", "Épée de fer", WeaponType::Sword, 1, Rarity::Common, 13, 0.05f, 0, Element::Physical, 0, "Forgée à Brindlemere. Équilibrée et fiable."},
        {"sword_knight", "Lame du chevalier", WeaponType::Sword, 2, Rarity::Rare, 19, 0.07f, 0, Element::Physical, 0, "Gravée aux armes de Fort Aldren."},
        {"sword_ember", "Épée de braise", WeaponType::Sword, 3, Rarity::Epic, 26, 0.08f, 0, Element::Fire, 8, "Le métal garde la chaleur des Ashen Lands."},
        {"sword_aldric", "Aube d'Aldric", WeaponType::Sword, 5, Rarity::Ancestral, 44, 0.12f, 0.05f, Element::Light, 16, "L'épée du Haut-Roi. Elle se souvient de la Couronne."},
        {"longsword_iron", "Épée longue", WeaponType::Longsword, 1, Rarity::Common, 20, 0.05f, -0.1f, Element::Physical, 0, "Deux mains, deux fois plus de conviction."},
        {"longsword_frost", "Croc-de-givre", WeaponType::Longsword, 3, Rarity::Epic, 34, 0.07f, -0.08f, Element::Ice, 10, "Forgée par Sigrun dans un bloc de glace éternelle."},
        {"longsword_fallen", "Serment brisé", WeaponType::Longsword, 4, Rarity::Mythic, 46, 0.1f, -0.05f, Element::Shadow, 14, "La lame du Chevalier Déchu. Elle pleure encore."},
        {"dagger_iron", "Dague de fer", WeaponType::Dagger, 1, Rarity::Common, 9, 0.12f, 0.2f, Element::Physical, 0, "Rapide, discrète, mortelle dans le dos."},
        {"dagger_shadow", "Murmure d'ombre", WeaponType::Dagger, 3, Rarity::Epic, 18, 0.22f, 0.25f, Element::Shadow, 7, "Elle frappe avant que l'on entende le pas."},
        {"dagger_viper", "Crochet de la Reine", WeaponType::Dagger, 4, Rarity::Legendary, 24, 0.25f, 0.25f, Element::Nature, 10, "Taillé dans un croc de la Reine des Araignées."},
        {"axe_iron", "Hache de fer", WeaponType::Axe, 1, Rarity::Common, 15, 0.06f, -0.03f, Element::Physical, 0, "Bonne pour le bois, meilleure pour les gobelins."},
        {"axe_nordic", "Hache du Jarl", WeaponType::Axe, 3, Rarity::Epic, 27, 0.08f, 0, Element::Ice, 6, "Le Jarl Bjorn la portait à vingt ans."},
        {"hammer_iron", "Marteau de guerre", WeaponType::Hammer, 1, Rarity::Common, 24, 0.04f, -0.15f, Element::Physical, 0, "Lourd. Très lourd."},
        {"hammer_titan", "Poing du Titan", WeaponType::Hammer, 4, Rarity::Legendary, 48, 0.06f, -0.12f, Element::Lightning, 12, "Un éclat du Titan de Pierre serti dans l'acier."},
        {"spear_iron", "Lance de fer", WeaponType::Spear, 1, Rarity::Common, 16, 0.06f, 0, Element::Physical, 0, "Garde l'ennemi à distance."},
        {"spear_crown", "Lance des Cimes", WeaponType::Spear, 3, Rarity::Epic, 29, 0.09f, 0.02f, Element::Lightning, 9, "Arme cérémonielle de l'Ordre de Highspire."},
        {"bow_hunter", "Arc de chasse", WeaponType::Bow, 1, Rarity::Common, 12, 0.08f, 0, Element::Physical, 0, "Bois d'if et corde de lin."},
        {"bow_elder", "Arc sylvestre", WeaponType::Bow, 3, Rarity::Epic, 24, 0.14f, 0.05f, Element::Nature, 8, "Courbé par les druides de Sylvarin."},
        {"bow_ancestral", "Chant des Rois", WeaponType::Bow, 5, Rarity::Ancestral, 40, 0.2f, 0.08f, Element::Light, 15, "Chaque flèche porte un nom oublié."},
        {"crossbow_iron", "Arbalète", WeaponType::Crossbow, 2, Rarity::Rare, 22, 0.1f, -0.1f, Element::Physical, 0, "Mécanique des mineurs de Cinderwatch."},
        {"staff_apprentice", "Bâton d'apprenti", WeaponType::Staff, 1, Rarity::Common, 8, 0.05f, 0, Element::Arcane, 6, "Il canalise la volonté."},
        {"staff_ember", "Bâton de la fournaise", WeaponType::Staff, 3, Rarity::Epic, 14, 0.07f, 0, Element::Fire, 16, "Son cœur brûle d'un feu qui ne s'éteint pas."},
        {"staff_abyss", "Sceptre de l'Abîme", WeaponType::Staff, 5, Rarity::Ancestral, 22, 0.12f, 0, Element::Shadow, 30, "Arraché au Seigneur des Abysses."},
    };
    for (auto& w : ws) {
        ItemDef& d = addItem(w.id, w.name, ItemKind::Weapon, w.r, 20 + w.tier * w.tier * 60 + (int)w.r * 80, w.desc);
        d.weapon = w.t; d.tier = w.tier; d.atk = w.atk; d.crit = w.crit; d.speed = w.speed; d.element = w.e; d.elemDmg = w.ed;
        d.icon = w.t == WeaponType::Bow || w.t == WeaponType::Crossbow ? "i_cl_archer" : (w.t == WeaponType::Staff ? "i_cl_mage" : "i_attack");
    }
    struct S { const char* id; const char* name; int tier; Rarity r; float def; vec3 c; const char* desc; };
    const S ss[] = {
        {"shield_wood", "Bouclier de bois", 0, Rarity::Common, 6, hexColor(0x8e2a24), "Quelques planches et beaucoup d'espoir."},
        {"shield_iron", "Écu de fer", 2, Rarity::Rare, 14, hexColor(0x2a4f7a), "Aux couleurs des Gardiens de Greenhaven."},
        {"shield_aegis", "Égide de Highspire", 4, Rarity::Legendary, 26, hexColor(0xe8dcc0), "Bénie par Sœur Odile. Elle repousse l'ombre."},
    };
    for (auto& s : ss) { ItemDef& d = addItem(s.id, s.name, ItemKind::Shield, s.r, 30 + s.tier * 120, s.desc); d.weapon = WeaponType::Shield; d.def = s.def; d.tier = s.tier; d.color = s.c; d.icon = "i_block"; }

    // ---- armour sets (head, chest, hands, feet) ------------------------------------------------
    struct A { const char* base; const char* setName; int set; int tier; Rarity r; Helm helm; Body body; vec3 c; float def; bool pauld; Element e; };
    const A as[] = {
        {"leather", "du rôdeur", 0, 1, Rarity::Common, Helm::Hood, Body::Leather, hexColor(0x5a3a22), 4, false, Element::Physical},
        {"chain", "de la garde", 1, 2, Rarity::Rare, Helm::Iron, Body::Chain, hexColor(0x2a4f7a), 7, true, Element::Physical},
        {"frost", "de Frostheim", 2, 3, Rarity::Epic, Helm::Horned, Body::Fur, hexColor(0x9ab8d0), 10, true, Element::Ice},
        {"ember", "de braise", 3, 3, Rarity::Epic, Helm::Great, Body::Plate, hexColor(0x8a3a22), 12, true, Element::Fire},
        {"sylvan", "sylvestre", 4, 3, Rarity::Epic, Helm::Circlet, Body::Leather, hexColor(0x2f7a5a), 8, false, Element::Nature},
        {"shadow", "du voile", 5, 4, Rarity::Legendary, Helm::Hood, Body::Leather, hexColor(0x2a2230), 11, false, Element::Shadow},
        {"crown", "ancestral", 6, 5, Rarity::Ancestral, Helm::Crown, Body::Plate, hexColor(0xd8b060), 18, true, Element::Light},
    };
    const char* pieceName[4] = {"Casque", "Plastron", "Gants", "Bottes"};
    const ItemKind pieceKind[4] = {ItemKind::Head, ItemKind::Chest, ItemKind::Hands, ItemKind::Feet};
    const float pieceMul[4] = {0.7f, 1.4f, 0.5f, 0.6f};
    for (auto& a : as)
        for (int p = 0; p < 4; p++) {
            std::string id = std::string(a.base) + "_" + (p == 0 ? "head" : p == 1 ? "chest" : p == 2 ? "hands" : "feet");
            std::string nm = std::string(pieceName[p]) + " " + a.setName;
            if (p == 0 && (a.helm == Helm::Hood)) nm = std::string("Capuche ") + a.setName;
            if (p == 0 && a.helm == Helm::Circlet) nm = std::string("Diadème ") + a.setName;
            if (p == 0 && a.helm == Helm::Crown) nm = std::string("Couronne ") + a.setName;
            if (p == 1 && a.body == Body::Leather) nm = std::string("Pourpoint ") + a.setName;
            if (p == 1 && a.body == Body::Fur) nm = std::string("Manteau ") + a.setName;
            if (p == 1 && a.body == Body::Chain) nm = std::string("Cotte de mailles ") + a.setName;
            ItemDef& d = addItem(id, nm, pieceKind[p], a.r, (int)(25 * (a.tier + 1) * (a.tier + 1) * pieceMul[p] + 10), "Pièce d'ensemble.");
            d.set = a.set; d.tier = a.tier; d.def = a.def * pieceMul[p]; d.color = a.c; d.helm = a.helm; d.body = a.body;
            d.pauldrons = a.pauld && p == 1; d.gauntlets = p == 2 && a.tier >= 2; d.greaves = p == 3 && a.tier >= 2;
            d.hp = p == 1 ? a.tier * 12.f : 0; d.element = a.e; d.cape = p == 1 && a.tier >= 3;
            d.icon = p == 0 ? "i_character" : (p == 1 ? "i_block" : (p == 2 ? "i_interact" : "i_dodge"));
        }
    // ---- trinkets ------------------------------------------------------------------------------
    struct T { const char* id; const char* name; ItemKind k; Rarity r; float hp, mp, crit, atk, def, speed; const char* desc; };
    const T ts[] = {
        {"ring_copper", "Anneau de cuivre", ItemKind::Ring, Rarity::Common, 10, 0, 0, 0, 1, 0, "Modeste, mais porte chance."},
        {"ring_sapphire", "Anneau de saphir", ItemKind::Ring, Rarity::Rare, 0, 25, 0.02f, 0, 0, 0, "Le saphir apaise l'esprit."},
        {"ring_wolf", "Chevalière du loup", ItemKind::Ring, Rarity::Epic, 20, 0, 0.06f, 4, 0, 0.03f, "Les chasseurs de Frostheim se la transmettent."},
        {"ring_abyss", "Anneau du vide", ItemKind::Ring, Rarity::Mythic, 0, 40, 0.1f, 8, 0, 0.05f, "On dit qu'il murmure la nuit."},
        {"amulet_oak", "Amulette de chêne", ItemKind::Amulet, Rarity::Common, 15, 10, 0, 0, 1, 0, "Un gland sculpté par un enfant de Brindlemere."},
        {"amulet_ember", "Cœur de braise", ItemKind::Amulet, Rarity::Rare, 25, 0, 0.03f, 3, 0, 0, "Il bat comme un cœur."},
        {"amulet_star", "Larme d'étoile", ItemKind::Amulet, Rarity::Legendary, 30, 50, 0.05f, 0, 3, 0, "Tombée du ciel sur les High Crowns."},
        {"amulet_crown", "Sceau d'Aldric", ItemKind::Amulet, Rarity::Ancestral, 60, 60, 0.08f, 10, 6, 0.05f, "Le sceau royal. Il réchauffe la peau de l'Errant."},
    };
    for (auto& t : ts) {
        ItemDef& d = addItem(t.id, t.name, t.k, t.r, 40 + (int)t.r * 150, t.desc);
        d.hp = t.hp; d.mp = t.mp; d.crit = t.crit; d.atk = t.atk; d.def = t.def; d.speed = t.speed; d.icon = t.k == ItemKind::Ring ? "i_coin" : "i_relic";
    }
    // ---- consumables -------------------------------------------------------------------------
    auto cons = [&](const char* id, const char* nm, Rarity r, int v, float heal, float mana, float stam, int buff, float bt, const char* icon, const char* desc) {
        ItemDef& d = addItem(id, nm, ItemKind::Consumable, r, v, desc);
        d.heal = heal; d.mana = mana; d.stam = stam; d.buff = buff; d.buffTime = bt; d.stack = 20; d.icon = icon;
    };
    cons("potion_minor", "Petite potion de soin", Rarity::Common, 15, 40, 0, 0, 0, 0, "i_potion", "Rend 40 points de vie.");
    cons("potion_heal", "Potion de soin", Rarity::Rare, 40, 110, 0, 0, 0, 0, "i_potion", "Rend 110 points de vie.");
    cons("potion_great", "Élixir de vie", Rarity::Epic, 120, 300, 0, 0, 0, 0, "i_potion", "Rend 300 points de vie.");
    cons("potion_mana", "Potion de mana", Rarity::Common, 20, 0, 60, 0, 0, 0, "i_potion_mana", "Rend 60 points de mana.");
    cons("potion_stamina", "Tonique d'endurance", Rarity::Common, 20, 0, 0, 100, 1, 60, "i_potion_stamina", "Endurance pleine et récupération accrue (60 s).");
    cons("elixir_strength", "Élixir de force", Rarity::Rare, 60, 0, 0, 0, 2, 120, "i_potion", "+20 % de dégâts pendant 2 minutes.");
    cons("elixir_frost", "Élixir de résistance au froid", Rarity::Rare, 50, 0, 0, 0, 3, 300, "i_potion_mana", "Protège du froid mordant de Frostheim.");
    cons("bread", "Pain de Brindlemere", Rarity::Common, 4, 20, 0, 20, 0, 0, "i_heart", "Encore tiède.");
    cons("stew", "Ragoût du chasseur", Rarity::Rare, 18, 70, 0, 50, 4, 180, "i_heart", "Rend 70 PV et accélère la régénération.");
    cons("fish_grilled", "Poisson grillé", Rarity::Common, 10, 45, 10, 0, 0, 0, "i_heart", "Cuit sur un feu de camp.");
    cons("berries", "Baies sauvages", Rarity::Common, 2, 12, 5, 0, 0, 0, "i_heart", "Sucrées et acidulées.");
    // ---- materials -----------------------------------------------------------------------------
    auto mat = [&](const char* id, const char* nm, Rarity r, int v, const char* icon, const char* desc) {
        ItemDef& d = addItem(id, nm, ItemKind::Material, r, v, desc); d.stack = 99; d.icon = icon;
    };
    mat("wood", "Bois", Rarity::Common, 2, "i_woodaxe", "Bûches de chêne et de pin.");
    mat("stone", "Pierre", Rarity::Common, 1, "i_pickaxe", "Moellons solides.");
    mat("iron_ore", "Minerai de fer", Rarity::Common, 5, "i_pickaxe", "Se fond à la forge.");
    mat("copper_ore", "Minerai de cuivre", Rarity::Common, 4, "i_pickaxe", "Reflets orangés.");
    mat("iron_bar", "Lingot de fer", Rarity::Common, 14, "i_anvil", "Base de toute bonne arme.");
    mat("copper_bar", "Lingot de cuivre", Rarity::Common, 11, "i_anvil", "Conduit bien la magie.");
    mat("rare_ore", "Minerai d'étoile", Rarity::Rare, 30, "i_crystal", "Un métal qui scintille dans le noir.");
    mat("star_bar", "Lingot d'étoile", Rarity::Epic, 90, "i_anvil", "Plus léger que l'air, plus dur que l'acier.");
    mat("herb", "Herbe médicinale", Rarity::Common, 3, "i_sickle", "Feuilles amères et apaisantes.");
    mat("frost_lily", "Lys de givre", Rarity::Rare, 12, "i_sickle", "Ne fleurit que sous la neige.");
    mat("glowcap", "Champignon lumineux", Rarity::Common, 6, "i_sickle", "Brille doucement dans la forêt d'Elderwood.");
    mat("crystal", "Cristal", Rarity::Rare, 20, "i_crystal", "Résonne au contact de la magie.");
    mat("frost_crystal", "Cristal de givre", Rarity::Rare, 22, "i_crystal", "Froid même au soleil.");
    mat("ember_core", "Noyau de braise", Rarity::Rare, 25, "i_crystal", "Le cœur d'un élémentaire de feu.");
    mat("leather", "Cuir", Rarity::Common, 6, "i_hunt", "Tanné par les chasseurs.");
    mat("fur", "Fourrure épaisse", Rarity::Common, 9, "i_hunt", "Chaude et douce.");
    mat("bone", "Os ancien", Rarity::Common, 3, "i_bestiary", "Ce qu'il reste des morts sans repos.");
    mat("spider_silk", "Soie d'araignée", Rarity::Rare, 15, "i_hunt", "Plus solide qu'une corde.");
    mat("troll_hide", "Peau de troll", Rarity::Rare, 28, "i_hunt", "Elle se régénère encore.");
    mat("shadow_essence", "Essence d'ombre", Rarity::Epic, 45, "i_relic", "Une fumée froide qui refuse de se dissiper.");
    mat("fish", "Poisson", Rarity::Common, 5, "i_fishing", "Encore frétillant.");
    mat("meat", "Viande crue", Rarity::Common, 4, "i_hunt", "À cuire sur un feu de camp.");
    mat("dragon_scale", "Écaille de dragon", Rarity::Legendary, 200, "i_relic", "Brûlante au toucher.");
    // ---- quest items & relics -------------------------------------------------------------------
    auto quest = [&](const char* id, const char* nm, Rarity r, const char* desc, const char* icon = "i_relic") {
        ItemDef& d = addItem(id, nm, ItemKind::Quest, r, 0, desc); d.icon = icon;
    };
    quest("echo_stone", "Pierre d'écho", Rarity::Rare, "Elle répète des voix d'il y a trois siècles.");
    quest("crown_shard", "Fragment de Couronne", Rarity::Legendary, "L'un des six fragments de la Couronne-Mère.");
    quest("mother_crown", "Couronne-Mère", Rarity::Ancestral, "Reforgée à Highspire. Elle peut sceller l'Abîme.");
    quest("miners_badge", "Insigne du contremaître", Rarity::Common, "L'insigne de cuivre de la Guilde des Mineurs.");
    quest("grave_lantern", "Lanterne des Veilleurs", Rarity::Rare, "Sa flamme guide les âmes perdues.");
    quest("star_chart", "Carte céleste", Rarity::Rare, "Les constellations de l'astronome Ilian.");
    quest("treasure_map", "Carte au trésor", Rarity::Rare, "Un X griffonné près d'un repère.", "i_map");
    quest("letter_maelle", "Lettre de Maëlle", Rarity::Common, "Scellée à la cire verte.", "i_quest");
    auto relic = [&](const char* id, const char* nm, const char* desc) { ItemDef& d = addItem(id, nm, ItemKind::Relic, Rarity::Legendary, 150, desc); d.icon = "i_relic"; };
    relic("relic_elven_harp", "Harpe elfique", "Ses cordes vibrent seules quand le vent souffle d'Elderwood.");
    relic("relic_jarl_horn", "Corne du premier Jarl", "Elle appela jadis les clans au combat.");
    relic("relic_miner_lamp", "Lampe du premier mineur", "Elle n'a jamais manqué d'huile.");
    relic("relic_kings_seal", "Sceau des rois de Morneval", "Cire noire et couronne brisée.");
    relic("relic_star_glass", "Lentille d'étoile", "À travers elle, la nuit devient jour.");
    relic("relic_aldric_ring", "Alliance d'Aldric", "Gravée : « Pour Elsa, au-delà de l'Abîme ».");
    // ---- tools & mount tokens --------------------------------------------------------------------
    auto tool = [&](const char* id, const char* nm, WeaponType w, const char* icon, const char* desc) { ItemDef& d = addItem(id, nm, ItemKind::Tool, Rarity::Common, 15, desc); d.weapon = w; d.icon = icon; };
    tool("tool_pickaxe", "Pioche", WeaponType::Pickaxe, "i_pickaxe", "Pour miner pierre et minerais.");
    tool("tool_woodaxe", "Hachette", WeaponType::WoodAxe, "i_woodaxe", "Pour couper du bois.");
    tool("tool_rod", "Canne à pêche", WeaponType::FishingRod, "i_fishing", "Patience et poignet souple.");
    auto mtok = [&](const char* id, const char* nm, int m, Rarity r, const char* desc) { ItemDef& d = addItem(id, nm, ItemKind::MountToken, r, 0, desc); d.mount = m; d.icon = "i_mount"; };
    mtok("mount_horse", "Sifflet : Cheval de Brindlemere", 0, Rarity::Rare, "Appelle votre cheval.");
    mtok("mount_wolf", "Sifflet : Loup géant", 1, Rarity::Epic, "Appelle Fenra, le loup géant d'Eira.");
    mtok("mount_elk", "Sifflet : Cerf sylvain", 2, Rarity::Epic, "Appelle le grand cerf d'Elderwood.");
    mtok("mount_boar", "Sifflet : Sanglier de guerre", 3, Rarity::Rare, "Appelle un sanglier caparaçonné.");
    mtok("mount_spectral", "Sifflet : Destrier spectral", 4, Rarity::Mythic, "Le destrier du Chevalier Déchu, libéré.");
    // ---- cosmetics (dyes) ---------------------------------------------------------------------------
    auto dye = [&](const char* id, const char* nm, uint32_t c, Rarity r) { ItemDef& d = addItem(id, nm, ItemKind::Cosmetic, r, 60, "Teinture d'armure (apparence)."); d.color = hexColor(c); d.icon = "i_star"; };
    dye("dye_royal", "Teinture bleu royal", 0x2a4f9a, Rarity::Common);
    dye("dye_crimson", "Teinture pourpre", 0x8e2a24, Rarity::Common);
    dye("dye_forest", "Teinture forestière", 0x2f6a3a, Rarity::Common);
    dye("dye_ivory", "Teinture ivoire", 0xe8dcc0, Rarity::Rare);
    dye("dye_night", "Teinture nuit", 0x1c1c28, Rarity::Rare);
    dye("dye_gold", "Teinture d'or", 0xc9a45c, Rarity::Epic);
}

const std::vector<ItemDef>& allItems() { std::call_once(gItemsOnce, buildItems); return gItems; }
const ItemDef* item(const std::string& id) {
    allItems();
    auto it = gItemIdx.find(id);
    return it == gItemIdx.end() ? nullptr : &gItems[it->second];
}

const std::vector<SetDef>& sets() {
    static std::vector<SetDef> s = {
        {"Ensemble du rôdeur", "+5 attaque", "+6 % critique, +5 % vitesse", 5, 0, 0, 0.06f, 0, 0, 0, 0.05f},
        {"Ensemble de la garde", "+8 défense", "+60 PV, +5 défense", 0, 8, 0, 0, 0, 0, 5, 0},
        {"Ensemble de Frostheim", "+40 PV", "Attaques de glace (+10)", 0, 0, 40, 0, 0, 0, 0, 0, Element::Ice, 10},
        {"Ensemble de braise", "+10 attaque", "Attaques de feu (+14)", 10, 0, 0, 0, 0, 0, 0, 0, Element::Fire, 14},
        {"Ensemble sylvestre", "+30 mana", "+10 % critique, attaques de nature (+8)", 0, 0, 0, 0.1f, 0, 30, 0, 0, Element::Nature, 8},
        {"Ensemble du voile", "+8 % critique", "Attaques d'ombre (+15)", 0, 0, 0, 0.08f, 6, 0, 0, 0.06f, Element::Shadow, 15},
        {"Ensemble ancestral", "+20 attaque, +10 défense", "+120 PV, attaques de lumière (+20)", 20, 10, 0, 0.05f, 10, 50, 10, 0.05f, Element::Light, 20},
    };
    return s;
}

const std::vector<Recipe>& recipes() {
    static std::vector<Recipe> r = {
        {"iron_bar", 1, {{"iron_ore", 2}}, ST_FORGE, 1},
        {"copper_bar", 1, {{"copper_ore", 2}}, ST_FORGE, 1},
        {"star_bar", 1, {{"rare_ore", 3}, {"crystal", 1}}, ST_FORGE, 8},
        {"sword_iron", 1, {{"iron_bar", 3}, {"leather", 1}}, ST_FORGE, 2},
        {"longsword_iron", 1, {{"iron_bar", 5}, {"leather", 2}}, ST_FORGE, 3},
        {"dagger_iron", 1, {{"iron_bar", 2}, {"leather", 1}}, ST_FORGE, 2},
        {"axe_iron", 1, {{"iron_bar", 3}, {"wood", 2}}, ST_FORGE, 2},
        {"hammer_iron", 1, {{"iron_bar", 6}, {"wood", 2}}, ST_FORGE, 4},
        {"spear_iron", 1, {{"iron_bar", 2}, {"wood", 4}}, ST_FORGE, 2},
        {"bow_hunter", 1, {{"wood", 5}, {"leather", 2}}, ST_WORKBENCH, 1},
        {"crossbow_iron", 1, {{"wood", 4}, {"iron_bar", 3}, {"spider_silk", 2}}, ST_WORKBENCH, 6},
        {"staff_apprentice", 1, {{"wood", 4}, {"crystal", 1}}, ST_WORKBENCH, 2},
        {"shield_wood", 1, {{"wood", 6}}, ST_WORKBENCH, 1},
        {"shield_iron", 1, {{"iron_bar", 4}, {"wood", 2}, {"leather", 2}}, ST_FORGE, 5},
        {"sword_ember", 1, {{"star_bar", 2}, {"ember_core", 3}, {"iron_bar", 2}}, ST_FORGE, 12},
        {"longsword_frost", 1, {{"star_bar", 2}, {"frost_crystal", 4}, {"iron_bar", 3}}, ST_FORGE, 12},
        {"bow_elder", 1, {{"wood", 8}, {"spider_silk", 3}, {"glowcap", 4}}, ST_WORKBENCH, 10},
        {"staff_ember", 1, {{"wood", 6}, {"ember_core", 4}, {"crystal", 2}}, ST_WORKBENCH, 11},
        {"leather_head", 1, {{"leather", 3}}, ST_WORKBENCH, 1}, {"leather_chest", 1, {{"leather", 6}}, ST_WORKBENCH, 1},
        {"leather_hands", 1, {{"leather", 2}}, ST_WORKBENCH, 1}, {"leather_feet", 1, {{"leather", 3}}, ST_WORKBENCH, 1},
        {"chain_head", 1, {{"iron_bar", 3}}, ST_FORGE, 4}, {"chain_chest", 1, {{"iron_bar", 6}, {"leather", 2}}, ST_FORGE, 4},
        {"chain_hands", 1, {{"iron_bar", 2}, {"leather", 1}}, ST_FORGE, 4}, {"chain_feet", 1, {{"iron_bar", 3}, {"leather", 1}}, ST_FORGE, 4},
        {"frost_head", 1, {{"fur", 3}, {"frost_crystal", 1}, {"iron_bar", 2}}, ST_FORGE, 9}, {"frost_chest", 1, {{"fur", 6}, {"frost_crystal", 2}, {"iron_bar", 3}}, ST_FORGE, 9},
        {"frost_hands", 1, {{"fur", 2}, {"frost_crystal", 1}}, ST_FORGE, 9}, {"frost_feet", 1, {{"fur", 3}, {"frost_crystal", 1}}, ST_FORGE, 9},
        {"ember_head", 1, {{"star_bar", 1}, {"ember_core", 2}}, ST_FORGE, 12}, {"ember_chest", 1, {{"star_bar", 2}, {"ember_core", 3}}, ST_FORGE, 12},
        {"ember_hands", 1, {{"star_bar", 1}, {"ember_core", 1}}, ST_FORGE, 12}, {"ember_feet", 1, {{"star_bar", 1}, {"ember_core", 1}}, ST_FORGE, 12},
        {"sylvan_head", 1, {{"leather", 2}, {"glowcap", 3}, {"crystal", 1}}, ST_WORKBENCH, 10}, {"sylvan_chest", 1, {{"leather", 5}, {"glowcap", 4}, {"spider_silk", 2}}, ST_WORKBENCH, 10},
        {"sylvan_hands", 1, {{"leather", 2}, {"glowcap", 2}}, ST_WORKBENCH, 10}, {"sylvan_feet", 1, {{"leather", 3}, {"glowcap", 2}}, ST_WORKBENCH, 10},
        {"ring_copper", 1, {{"copper_bar", 2}}, ST_FORGE, 2},
        {"potion_minor", 1, {{"herb", 2}}, ST_ALCHEMY, 1},
        {"potion_heal", 1, {{"herb", 3}, {"glowcap", 1}}, ST_ALCHEMY, 4},
        {"potion_great", 1, {{"herb", 4}, {"frost_lily", 1}, {"crystal", 1}}, ST_ALCHEMY, 10},
        {"potion_mana", 1, {{"glowcap", 2}, {"crystal", 1}}, ST_ALCHEMY, 3},
        {"potion_stamina", 1, {{"herb", 1}, {"berries", 3}}, ST_ALCHEMY, 2},
        {"elixir_strength", 1, {{"troll_hide", 1}, {"herb", 2}, {"ember_core", 1}}, ST_ALCHEMY, 8},
        {"elixir_frost", 1, {{"frost_lily", 2}, {"fur", 1}}, ST_ALCHEMY, 6},
        {"fish_grilled", 1, {{"fish", 1}}, ST_CAMPFIRE, 1},
        {"stew", 1, {{"meat", 2}, {"herb", 1}}, ST_CAMPFIRE, 2},
        {"bread", 2, {{"herb", 1}}, ST_CAMPFIRE, 1},
    };
    return r;
}

// ============================================================================================ spells & skills
const std::vector<SpellDef>& spells() {
    static std::vector<SpellDef> s = {
        {"fireball", "Boule de feu", "Projectile qui explose à l'impact.", "i_sp_fireball", Element::Fire, SpellKind::Projectile, 18, 2.5f, 1.6f, 3.f, 0},
        {"flamewave", "Vague ardente", "Une vague de flammes devant vous.", "i_sp_flamewave", Element::Fire, SpellKind::Area, 30, 8, 2.2f, 6.f, 0},
        {"icespike", "Lance de givre", "Transperce et ralentit les ennemis.", "i_sp_icespike", Element::Ice, SpellKind::Projectile, 16, 2.f, 1.4f, 1.f, 3},
        {"frostnova", "Nova glaciale", "Gèle les ennemis proches.", "i_sp_frostnova", Element::Ice, SpellKind::Area, 32, 12, 1.2f, 7.f, 2.5f},
        {"chain", "Éclair en chaîne", "La foudre rebondit entre les ennemis.", "i_sp_chain", Element::Lightning, SpellKind::Chain, 24, 5, 1.5f, 12.f, 0},
        {"thunder", "Frappe du tonnerre", "La foudre s'abat autour de vous.", "i_sp_thunder", Element::Lightning, SpellKind::Rain, 34, 10, 2.6f, 5.f, 0},
        {"roots", "Racines", "Des racines immobilisent les ennemis.", "i_sp_roots", Element::Nature, SpellKind::Area, 22, 10, 0.8f, 6.f, 3},
        {"regrowth", "Régénération", "Soigne au fil du temps.", "i_sp_regrowth", Element::Nature, SpellKind::Self, 26, 14, 1.6f, 0, 8},
        {"shadowstep", "Pas de l'ombre", "Traverse les ennemis en les lacérant.", "i_sp_shadowstep", Element::Shadow, SpellKind::Dash, 18, 6, 1.8f, 8.f, 0},
        {"souldrain", "Drain d'âme", "Vole la vie de l'ennemi.", "i_sp_souldrain", Element::Shadow, SpellKind::Beam, 20, 7, 1.3f, 10.f, 2},
        {"holylight", "Lumière sacrée", "Soigne et brûle les morts-vivants proches.", "i_sp_holylight", Element::Light, SpellKind::Self, 30, 15, 2.f, 5.f, 0},
        {"aegis", "Égide radieuse", "Un bouclier absorbe les dégâts.", "i_sp_radiantshield", Element::Light, SpellKind::Shield, 28, 18, 1.f, 0, 8},
        {"missiles", "Projectiles arcaniques", "Trois projectiles à tête chercheuse.", "i_sp_missiles", Element::Arcane, SpellKind::Projectile, 20, 3, 0.9f, 1.f, 0},
        {"arcanenova", "Nova arcanique", "Repousse violemment les ennemis.", "i_sp_arcanenova", Element::Arcane, SpellKind::Area, 28, 9, 1.4f, 6.f, 0},
    };
    return s;
}
int spellIndex(const std::string& id) { auto& s = spells(); for (int i = 0; i < (int)s.size(); i++) if (s[i].id == id) return i; return -1; }

const std::vector<SkillNode>& skillTree() {
    static std::vector<SkillNode> t;
    static std::once_flag once;
    std::call_once(once, [] {
        auto N = [&](const char* id, const char* name, const char* desc, int br, int tier, int cost, const char* req) -> SkillNode& {
            SkillNode n; n.id = id; n.name = name; n.desc = desc; n.branch = br; n.tier = tier; n.cost = cost; n.requires_ = req; t.push_back(n); return t.back();
        };
        N("w1", "Force brute", "+10 % de dégâts physiques.", BR_WARRIOR, 0, 1, "").atk = 0.1f;
        N("w2", "Endurance", "+25 endurance maximale.", BR_WARRIOR, 1, 1, "w1").stamina = 25;
        N("w3", "Frappe du tonnerre", "Débloque le sort Frappe du tonnerre.", BR_WARRIOR, 2, 2, "w2").spell = "thunder";
        N("w4", "Peau de fer", "+15 défense.", BR_WARRIOR, 3, 2, "w3").def = 15;
        { auto& n = N("w5", "Fureur du champion", "+15 % de dégâts, +25 % de dégâts critiques.", BR_WARRIOR, 4, 3, "w4"); n.atk = 0.15f; n.critDmg = 0.25f; }
        N("a1", "Œil de faucon", "+20 % de dégâts à l'arc.", BR_ARCHER, 0, 1, "").bowDmg = 0.2f;
        N("a2", "Pas léger", "+6 % de vitesse de déplacement.", BR_ARCHER, 1, 1, "a1").speed = 0.06f;
        N("a3", "Éclair en chaîne", "Débloque le sort Éclair en chaîne.", BR_ARCHER, 2, 2, "a2").spell = "chain";
        N("a4", "Tir précis", "+8 % de chance de critique.", BR_ARCHER, 3, 2, "a3").crit = 0.08f;
        N("a5", "Projectiles arcaniques", "Débloque le sort Projectiles arcaniques.", BR_ARCHER, 4, 3, "a4").spell = "missiles";
        N("s1", "Lame sournoise", "+20 % de dégâts aux dagues.", BR_ASSASSIN, 0, 1, "").daggerDmg = 0.2f;
        N("s2", "Esquive féline", "-20 % d'endurance pour esquiver.", BR_ASSASSIN, 1, 1, "s1").staminaCost = 0.2f;
        N("s3", "Pas de l'ombre", "Débloque le sort Pas de l'ombre.", BR_ASSASSIN, 2, 2, "s2").spell = "shadowstep";
        { auto& n = N("s4", "Coup fatal", "+10 % de critique, +30 % de dégâts critiques.", BR_ASSASSIN, 3, 2, "s3"); n.crit = 0.1f; n.critDmg = 0.3f; }
        N("s5", "Drain d'âme", "Débloque le sort Drain d'âme.", BR_ASSASSIN, 4, 3, "s4").spell = "souldrain";
        N("m1", "Érudition", "+40 mana maximal.", BR_MAGE, 0, 1, "").mp = 40;
        N("m2", "Lance de givre", "Débloque le sort Lance de givre.", BR_MAGE, 1, 1, "m1").spell = "icespike";
        N("m3", "Vague ardente", "Débloque le sort Vague ardente.", BR_MAGE, 2, 2, "m2").spell = "flamewave";
        N("m4", "Nova glaciale", "Débloque le sort Nova glaciale.", BR_MAGE, 3, 2, "m3").spell = "frostnova";
        { auto& n = N("m5", "Archimage", "+30 % de puissance des sorts, débloque Nova arcanique.", BR_MAGE, 4, 3, "m4"); n.spellDmg = 0.3f; n.spell = "arcanenova"; }
        N("p1", "Foi", "+50 points de vie maximum.", BR_PALADIN, 0, 1, "").hp = 50;
        N("p2", "Lumière sacrée", "Débloque le sort Lumière sacrée.", BR_PALADIN, 1, 1, "p1").spell = "holylight";
        N("p3", "Parade bénie", "Fenêtre de parade allongée.", BR_PALADIN, 2, 2, "p2").parryWindow = 0.08f;
        N("p4", "Égide radieuse", "Débloque le sort Égide radieuse.", BR_PALADIN, 3, 2, "p3").spell = "aegis";
        { auto& n = N("p5", "Rempart", "+20 défense, +80 PV.", BR_PALADIN, 4, 3, "p4"); n.def = 20; n.hp = 80; }
        N("h1", "Pistage", "+15 % de dégâts contre les bêtes.", BR_HUNTER, 0, 1, "").petDmg = 0.15f;
        N("h2", "Racines", "Débloque le sort Racines.", BR_HUNTER, 1, 1, "h1").spell = "roots";
        N("h3", "Herboriste", "+30 % d'efficacité des soins.", BR_HUNTER, 2, 2, "h2").heal = 0.3f;
        N("h4", "Régénération", "Débloque le sort Régénération.", BR_HUNTER, 3, 2, "h3").spell = "regrowth";
        { auto& n = N("h5", "Maître de la traque", "+10 % vitesse, +30 endurance.", BR_HUNTER, 4, 3, "h4"); n.speed = 0.1f; n.stamina = 30; }
    });
    return t;
}
int skillIndex(const std::string& id) { auto& s = skillTree(); for (int i = 0; i < (int)s.size(); i++) if (s[i].id == id) return i; return -1; }

// ============================================================================================ enemies
static std::vector<EnemyDef> gEnemies;
static std::once_flag gEnemiesOnce;

static HumanoidLook humanLook(Species sp, Body body, uint32_t c1, uint32_t c2, Helm helm, uint32_t seed) {
    HumanoidLook h; h.species = sp; h.body = body; h.c1 = hexColor(c1); h.c2 = hexColor(c2); h.helm = helm; h.seed = seed;
    return h;
}

static void buildEnemies() {
    auto E = [&](const char* id, const char* name, const char* family, int region, int level, float hp, float atk, float def, float speed, Behavior b, const char* desc) -> EnemyDef& {
        EnemyDef e; e.id = id; e.name = name; e.family = family; e.region = region; e.level = level; e.hp = hp; e.atk = atk; e.def = def; e.speed = speed; e.behavior = b; e.desc = desc;
        e.xp = (int)(8 + level * level * 3); e.gold = 2 + level * 2;
        gEnemies.push_back(e); return gEnemies.back();
    };
    auto melee = [](ClipId c, float range, float mult, float cd, float tele) { return AttackDef{c, AttackType::Melee, range, mult, cd, tele, Element::Physical, 0, 0}; };
    // ---- Greenhaven
    { auto& e = E("wolf", "Loup gris", "Loups", R_GREENHAVEN, 2, 45, 7, 1, 6.5f, Behavior::Pack, "Ils chassent en meute à la lisière des forêts.");
      e.arch = Arch::Quadruped; e.clook.c1 = hexColor(0x6a6a70); e.clook.c2 = hexColor(0xb8b4ac); e.clook.mane = true; e.group = 3; e.radius = 0.5f;
      e.attacks = {melee(C_Q_BITE, 1.8f, 1, 1.4f, 0.25f), {C_Q_POUNCE, AttackType::Leap, 6, 1.4f, 5, 0.45f, Element::Physical, 1.5f, 0}, {C_Q_HOWL, AttackType::Summon, 30, 0, 18, 0.2f, Element::Physical, 0, 0}};
      e.loot = {{"leather", 0.6f}, {"meat", 0.5f}, {"fur", 0.2f}}; e.night = true; }
    { auto& e = E("boar", "Sanglier furieux", "Bêtes", R_GREENHAVEN, 2, 70, 9, 3, 6.f, Behavior::Charger, "Il charge tout ce qui bouge. Puis il recommence.");
      e.arch = Arch::Quadruped; e.clook.kind = 1; e.clook.c1 = hexColor(0x5a4030); e.clook.c2 = hexColor(0x7a6050); e.clook.c3 = hexColor(0x2a2018); e.clook.tusks = true; e.clook.girth = 1.25f; e.scale = 1.1f; e.radius = 0.6f;
      e.attacks = {{C_Q_CHARGE, AttackType::Charge, 12, 1.6f, 6, 0.6f, Element::Physical, 1.2f, 0}, melee(C_Q_BITE, 1.8f, 1, 1.6f, 0.3f)};
      e.loot = {{"meat", 0.8f}, {"leather", 0.5f}}; }
    { auto& e = E("bandit", "Bandit coupe-jarret", "Bandits", R_GREENHAVEN, 3, 60, 9, 2, 4.6f, Behavior::Melee, "Les Écorcheurs dépouillent les voyageurs sur les routes.");
      e.hlook = humanLook(Species::Human, Body::Leather, 0x5a3a2a, 0x3a3028, Helm::Hood, 11); e.hlook.skin = hexColor(0xc89a7a); e.weapon = WeaponType::Dagger; e.group = 2;
      e.attacks = {melee(C_DG_A1, 1.9f, 1, 1.2f, 0.3f), melee(C_DG_A2, 1.9f, 1, 1.2f, 0.3f), melee(C_DG_HEAVY, 5.f, 1.6f, 7, 0.5f)};
      e.loot = {{"leather", 0.4f}, {"potion_minor", 0.25f}, {"bread", 0.3f}, {"dagger_iron", 0.05f}}; e.gold = 12; }
    { auto& e = E("bandit_archer", "Bandit archer", "Bandits", R_GREENHAVEN, 3, 45, 8, 1, 4.4f, Behavior::Ranged, "Il préfère tirer depuis les buissons.");
      e.hlook = humanLook(Species::Human, Body::Leather, 0x3a4a2a, 0x3a3028, Helm::Hood, 12); e.weapon = WeaponType::Bow;
      e.attacks = {{C_BOW_SHOT, AttackType::Projectile, 18, 1, 2.2f, 0.6f, Element::Physical, 0, 0}};
      e.loot = {{"wood", 0.4f}, {"bow_hunter", 0.04f}, {"potion_minor", 0.2f}}; e.gold = 10; }
    { auto& e = E("goblin", "Gobelin pillard", "Gobelins", R_GREENHAVEN, 2, 38, 6, 1, 5.2f, Behavior::Swarm, "Petits, vifs, et beaucoup trop nombreux.");
      e.hlook = humanLook(Species::Goblin, Body::Rags, 0x6a4a2a, 0x4a3a2a, Helm::None, 13); e.hlook.skin = hexColor(0x6a8a3a); e.hlook.headSize = 1.35f; e.hlook.hair = Hair::None; e.hlook.eyes = hexColor(0xe0c030);
      e.scale = 0.72f; e.weapon = WeaponType::Dagger; e.group = 3; e.radius = 0.35f;
      e.attacks = {melee(C_DG_A1, 1.5f, 1, 1.f, 0.25f), melee(C_DG_A3, 1.5f, 1.3f, 4, 0.35f)};
      e.loot = {{"copper_ore", 0.3f}, {"bone", 0.3f}, {"berries", 0.3f}}; }
    { auto& e = E("goblin_shaman", "Gobelin chaman", "Gobelins", R_GREENHAVEN, 4, 42, 10, 1, 4.f, Behavior::Caster, "Il agite des os et invoque des flammes vertes.");
      e.hlook = humanLook(Species::Goblin, Body::Robe, 0x4a5a3a, 0x3a3028, Helm::Skull, 14); e.hlook.skin = hexColor(0x5a7a3a); e.hlook.headSize = 1.35f; e.hlook.emissiveEyes = 0.5f; e.hlook.glow = hexColor(0x8aff5a);
      e.scale = 0.75f; e.weapon = WeaponType::Staff; e.radius = 0.35f;
      e.attacks = {{C_STAFF_CAST, AttackType::Projectile, 16, 1.3f, 2.6f, 0.7f, Element::Nature, 1.5f, 0}, {C_CAST_SELF, AttackType::Summon, 20, 0, 15, 0.5f, Element::Nature, 0, 0}};
      e.loot = {{"herb", 0.5f}, {"crystal", 0.15f}, {"potion_mana", 0.2f}}; }
    // ---- Frostheim
    { auto& e = E("snow_wolf", "Loup des neiges", "Loups", R_FROSTHEIM, 6, 85, 13, 3, 7.f, Behavior::Pack, "Le blizzard est leur terrain de chasse.");
      e.arch = Arch::Quadruped; e.clook.c1 = hexColor(0xd8dde4); e.clook.c2 = hexColor(0xf0f2f4); e.clook.mane = true; e.clook.glow = hexColor(0x80d0ff); e.group = 3; e.scale = 1.15f; e.radius = 0.55f;
      e.attacks = {melee(C_Q_BITE, 2.f, 1, 1.3f, 0.25f), {C_Q_POUNCE, AttackType::Leap, 7, 1.5f, 5, 0.4f, Element::Ice, 1.5f, 0}};
      e.loot = {{"fur", 0.7f}, {"meat", 0.4f}}; e.weak = Element::Fire; e.resist = Element::Ice; }
    { auto& e = E("frost_troll", "Troll des glaces", "Trolls", R_FROSTHEIM, 8, 260, 22, 6, 3.6f, Behavior::Tank, "Sa peau se reforme plus vite qu'on ne la tranche.");
      e.hlook = humanLook(Species::Troll, Body::Rags, 0x6a5a4a, 0x4a3a2a, Helm::None, 21); e.hlook.skin = hexColor(0x8aa0b0); e.hlook.bulk = 1.7f; e.hlook.hair = Hair::Mohawk; e.hlook.hairCol = hexColor(0xe8eef4);
      e.scale = 1.75f; e.radius = 0.9f; e.poise = 80;
      e.attacks = {melee(C_B_SWIPE, 3.2f, 1, 2.2f, 0.55f), {C_B_SLAM, AttackType::Area, 3.4f, 1.6f, 5, 0.8f, Element::Ice, 3.5f, 0}, {C_B_THROW, AttackType::Projectile, 18, 1.3f, 7, 0.8f, Element::Ice, 2.f, 0}};
      e.loot = {{"troll_hide", 0.6f}, {"frost_crystal", 0.3f}, {"fur", 0.4f}}; e.weak = Element::Fire; e.resist = Element::Ice; }
    { auto& e = E("draugr", "Draugr gelé", "Morts-vivants", R_FROSTHEIM, 7, 120, 16, 6, 3.8f, Behavior::Melee, "Les anciens guerriers des clans, réveillés par le froid de la Couronne.");
      e.hlook = humanLook(Species::Undead, Body::Fur, 0x4a5a6a, 0x3a3a3a, Helm::Horned, 22); e.hlook.emissiveEyes = 1; e.hlook.glow = hexColor(0x6ff0ff); e.hlook.eyes = hexColor(0x6ff0ff); e.hlook.beard = true; e.hlook.hairCol = hexColor(0xb0b8c0);
      e.weapon = WeaponType::Axe; e.offhand = WeaponType::Shield;
      e.attacks = {melee(C_1H_A1, 2.f, 1, 1.5f, 0.4f), melee(C_1H_A3, 2.2f, 1.4f, 3, 0.55f), melee(C_SHIELD_BASH, 2.f, 0.8f, 6, 0.35f)};
      e.loot = {{"bone", 0.5f}, {"iron_ore", 0.4f}, {"axe_iron", 0.05f}}; e.weak = Element::Fire; e.night = true; }
    { auto& e = E("frost_elemental", "Élémentaire de givre", "Élémentaires", R_FROSTHEIM, 9, 160, 18, 10, 3.2f, Behavior::Caster, "Une tempête de neige qui a appris à marcher.");
      e.hlook = humanLook(Species::Golem, Body::Plate, 0xa0c8e0, 0xa0c8e0, Helm::None, 23); e.hlook.glow = hexColor(0x9ae6ff); e.hlook.bulk = 1.3f; e.scale = 1.3f; e.radius = 0.6f;
      e.attacks = {{C_CAST_FORWARD, AttackType::Projectile, 15, 1.2f, 2.5f, 0.6f, Element::Ice, 1.5f, 0}, {C_CAST_AREA, AttackType::Area, 5, 1.5f, 8, 0.9f, Element::Ice, 5.f, 0}};
      e.loot = {{"frost_crystal", 0.8f}, {"crystal", 0.3f}}; e.weak = Element::Fire; e.resist = Element::Ice; }
    // ---- Ashen Lands
    { auto& e = E("ember_elemental", "Élémentaire de braise", "Élémentaires", R_ASHEN, 11, 190, 24, 10, 3.4f, Behavior::Caster, "Né des veines de lave, il ne connaît que la faim.");
      e.hlook = humanLook(Species::Golem, Body::Plate, 0x2a2420, 0x2a2420, Helm::None, 31); e.hlook.glow = hexColor(0xff7a26); e.hlook.bulk = 1.3f; e.scale = 1.35f; e.radius = 0.6f;
      e.attacks = {{C_CAST_FORWARD, AttackType::Projectile, 15, 1.2f, 2.4f, 0.6f, Element::Fire, 2.f, 0}, {C_B_SLAM, AttackType::Area, 3.f, 1.5f, 6, 0.8f, Element::Fire, 3.5f, 0}};
      e.loot = {{"ember_core", 0.7f}, {"rare_ore", 0.2f}}; e.weak = Element::Ice; e.resist = Element::Fire; }
    { auto& e = E("salamander", "Salamandre", "Bêtes", R_ASHEN, 10, 140, 20, 6, 5.2f, Behavior::Ranged, "Elle crache du feu liquide et dort dans la lave.");
      e.arch = Arch::Quadruped; e.clook.kind = 8; e.clook.c1 = hexColor(0x3a2a24); e.clook.c2 = hexColor(0xd0602a); e.clook.c3 = hexColor(0x1a1210); e.clook.spikes = true; e.clook.fur = false; e.clook.glow = hexColor(0xff8a2a);
      e.clook.legLen = 0.6f; e.scale = 1.4f; e.radius = 0.6f;
      e.attacks = {melee(C_Q_BITE, 2.2f, 1, 1.5f, 0.3f), {C_Q_HOWL, AttackType::Breath, 7, 0.4f, 6, 0.6f, Element::Fire, 2.f, 0}};
      e.loot = {{"leather", 0.5f}, {"ember_core", 0.2f}, {"dragon_scale", 0.01f}}; e.weak = Element::Ice; e.resist = Element::Fire; }
    { auto& e = E("ash_knight", "Chevalier calciné", "Chevaliers corrompus", R_ASHEN, 12, 240, 26, 14, 3.8f, Behavior::Melee, "Les défenseurs du Bastion, figés dans la cendre et la colère.");
      e.hlook = humanLook(Species::Knight, Body::Plate, 0x2a2420, 0x2a2420, Helm::Great, 32); e.hlook.metal = hexColor(0x4a403a); e.hlook.emissiveEyes = 1; e.hlook.glow = hexColor(0xff6a20);
      e.weapon = WeaponType::Longsword; e.poise = 60;
      e.attacks = {melee(C_2H_A1, 2.6f, 1, 2.f, 0.5f), melee(C_2H_A3, 2.6f, 1.5f, 4, 0.7f), {C_2H_HEAVY, AttackType::Leap, 8, 1.7f, 9, 0.6f, Element::Fire, 3.f, 0}};
      e.loot = {{"iron_bar", 0.5f}, {"ember_core", 0.3f}, {"ember_head", 0.04f}, {"ember_chest", 0.03f}}; e.resist = Element::Fire; }
    { auto& e = E("ash_bear", "Ours de cendre", "Bêtes", R_ASHEN, 11, 300, 26, 8, 5.f, Behavior::Tank, "Son pelage fume encore.");
      e.arch = Arch::Quadruped; e.clook.kind = 2; e.clook.c1 = hexColor(0x2a2624); e.clook.c2 = hexColor(0x4a3a30); e.clook.c3 = hexColor(0x111010); e.clook.girth = 1.5f; e.clook.glow = hexColor(0xff6a20);
      e.scale = 1.7f; e.radius = 0.9f; e.poise = 70;
      e.attacks = {melee(C_Q_BITE, 2.6f, 1, 1.8f, 0.35f), {C_Q_REAR, AttackType::Area, 3.f, 1.6f, 6, 0.8f, Element::Physical, 3.f, 0}};
      e.loot = {{"fur", 0.8f}, {"meat", 0.6f}, {"leather", 0.6f}}; }
    // ---- Elderwood
    { auto& e = E("blight_treant", "Sylvain corrompu", "Créatures magiques", R_ELDERWOOD, 9, 200, 19, 9, 2.8f, Behavior::Tank, "Un arbre ancien rendu fou par le fragment du Gardien.");
      e.hlook = humanLook(Species::Treant, Body::Rags, 0x5a4636, 0x5a4636, Helm::None, 41); e.hlook.bulk = 1.5f; e.hlook.emissiveEyes = 1; e.hlook.eyes = hexColor(0xa0ff60); e.scale = 1.8f; e.radius = 0.8f; e.poise = 70;
      e.attacks = {melee(C_B_SWIPE, 3.2f, 1, 2.2f, 0.5f), {C_B_STOMP, AttackType::Area, 3.5f, 1.4f, 6, 0.8f, Element::Nature, 4.f, 0}};
      e.loot = {{"wood", 0.9f}, {"glowcap", 0.4f}, {"herb", 0.4f}}; e.weak = Element::Fire; e.resist = Element::Nature; }
    { auto& e = E("wisp", "Feu follet", "Créatures magiques", R_ELDERWOOD, 8, 70, 16, 2, 5.5f, Behavior::Flyer, "Une lueur qui attire les voyageurs hors des sentiers.");
      e.hlook = humanLook(Species::Specter, Body::Robe, 0x4ad0c0, 0x4ad0c0, Helm::None, 42); e.hlook.glow = hexColor(0x8ffff0); e.scale = 0.75f; e.radius = 0.35f;
      e.attacks = {{C_CAST_FORWARD, AttackType::Projectile, 14, 1, 1.8f, 0.4f, Element::Arcane, 1.f, 0}};
      e.loot = {{"crystal", 0.4f}, {"glowcap", 0.4f}}; e.night = true; e.weak = Element::Shadow; }
    { auto& e = E("forest_spider", "Araignée sylvestre", "Araignées géantes", R_ELDERWOOD, 8, 110, 17, 4, 5.8f, Behavior::Ambusher, "Elle tisse entre les racines géantes.");
      e.arch = Arch::Spider; e.clook.c1 = hexColor(0x3a5a2a); e.clook.c2 = hexColor(0xb8e040); e.clook.c3 = hexColor(0x1a2a14); e.clook.glow = hexColor(0xc8ff40); e.scale = 1.2f; e.radius = 0.7f; e.group = 2;
      e.attacks = {melee(C_S_BITE, 2.f, 1, 1.4f, 0.3f), {C_S_SPIT, AttackType::Projectile, 12, 0.8f, 4, 0.5f, Element::Nature, 1.5f, 0}, {C_S_LEAP, AttackType::Leap, 8, 1.3f, 6, 0.5f, Element::Physical, 1.5f, 0}};
      e.loot = {{"spider_silk", 0.7f}}; e.weak = Element::Fire; }
    { auto& e = E("spirit_stag", "Cerf spectral", "Créatures magiques", R_ELDERWOOD, 10, 180, 20, 6, 7.f, Behavior::Charger, "Gardien des clairières, il ne pardonne pas les intrus.");
      e.arch = Arch::Quadruped; e.clook.kind = 7; e.clook.c1 = hexColor(0x2a6a6a); e.clook.c2 = hexColor(0x8fe0d0); e.clook.horns = true; e.clook.crystal = true; e.clook.glow = hexColor(0x8ffff0); e.scale = 1.6f; e.radius = 0.7f;
      e.attacks = {{C_Q_CHARGE, AttackType::Charge, 14, 1.5f, 6, 0.6f, Element::Arcane, 1.5f, 0}, {C_Q_REAR, AttackType::Area, 3.f, 1.3f, 5, 0.7f, Element::Nature, 3.5f, 0}};
      e.loot = {{"crystal", 0.5f}, {"leather", 0.6f}}; }
    // ---- Shadows
    { auto& e = E("skeleton", "Squelette guerrier", "Squelettes", R_SHADOWS, 6, 70, 13, 4, 4.f, Behavior::Melee, "Les soldats de Morneval n'ont jamais quitté leur poste.");
      e.hlook = humanLook(Species::Skeleton, Body::Bones, 0x4a3a3a, 0x3a3030, Helm::Iron, 51); e.hlook.emissiveEyes = 1; e.hlook.glow = hexColor(0x6ff0ff); e.hlook.metal = hexColor(0x7a7a70);
      e.weapon = WeaponType::Sword; e.offhand = WeaponType::Shield; e.group = 3;
      e.attacks = {melee(C_1H_A1, 2.f, 1, 1.4f, 0.35f), melee(C_1H_A2, 2.f, 1, 1.4f, 0.35f), melee(C_SHIELD_BASH, 2.f, 0.7f, 5, 0.3f)};
      e.loot = {{"bone", 0.8f}, {"iron_ore", 0.2f}}; e.weak = Element::Light; e.night = true; }
    { auto& e = E("skeleton_archer", "Squelette archer", "Squelettes", R_SHADOWS, 6, 55, 12, 3, 4.f, Behavior::Ranged, "Ses flèches sont aussi vieilles que lui.");
      e.hlook = humanLook(Species::Skeleton, Body::Bones, 0x4a3a3a, 0x3a3030, Helm::Hood, 52); e.hlook.emissiveEyes = 1; e.hlook.glow = hexColor(0x6ff0ff); e.weapon = WeaponType::Bow;
      e.attacks = {{C_BOW_SHOT, AttackType::Projectile, 18, 1, 2.4f, 0.6f, Element::Physical, 0, 0}};
      e.loot = {{"bone", 0.8f}, {"wood", 0.3f}}; e.weak = Element::Light; e.night = true; }
    { auto& e = E("ghoul", "Goule", "Morts-vivants", R_SHADOWS, 7, 90, 15, 2, 6.f, Behavior::Pack, "Elle rôde dans les marais, affamée.");
      e.hlook = humanLook(Species::Undead, Body::Rags, 0x3a3a30, 0x2a2a20, Helm::None, 53); e.hlook.hair = Hair::Long; e.hlook.hairCol = hexColor(0x3a3a3a); e.hlook.emissiveEyes = 0.8f; e.hlook.eyes = hexColor(0xd0ff60); e.group = 2;
      e.attacks = {melee(C_FIST_A1, 1.8f, 1, 1.f, 0.25f), melee(C_FIST_A2, 1.8f, 1.1f, 1.2f, 0.25f), {C_Q_POUNCE, AttackType::Leap, 6, 1.3f, 5, 0.4f, Element::Shadow, 1.5f, 0}};
      e.loot = {{"bone", 0.5f}, {"shadow_essence", 0.05f}}; e.weak = Element::Light; e.night = true; }
    { auto& e = E("specter", "Spectre", "Morts-vivants", R_SHADOWS, 9, 110, 20, 5, 4.5f, Behavior::Caster, "Un écho de douleur qui refuse de s'éteindre.");
      e.hlook = humanLook(Species::Specter, Body::Robe, 0x2a2238, 0x2a2238, Helm::None, 54); e.hlook.glow = hexColor(0xb06cff); e.scale = 1.1f;
      e.attacks = {{C_CAST_FORWARD, AttackType::Projectile, 16, 1.2f, 2.2f, 0.5f, Element::Shadow, 1.5f, 0}, {C_CAST_AREA, AttackType::Area, 5, 1.4f, 8, 0.9f, Element::Shadow, 5.f, 0}};
      e.loot = {{"shadow_essence", 0.4f}}; e.weak = Element::Light; e.resist = Element::Shadow; e.night = true; }
    { auto& e = E("giant_spider", "Araignée géante", "Araignées géantes", R_SHADOWS, 8, 150, 18, 5, 5.5f, Behavior::Ambusher, "La progéniture de la Reine. Venimeuse.");
      e.arch = Arch::Spider; e.clook.c1 = hexColor(0x2a2028); e.clook.c2 = hexColor(0xc02020); e.clook.c3 = hexColor(0x141014); e.clook.glow = hexColor(0xff3030); e.scale = 1.5f; e.radius = 0.85f;
      e.attacks = {melee(C_S_BITE, 2.4f, 1, 1.4f, 0.3f), {C_S_SPIT, AttackType::Projectile, 12, 0.8f, 4, 0.5f, Element::Nature, 1.5f, 0}, {C_S_LEAP, AttackType::Leap, 9, 1.4f, 6, 0.5f, Element::Physical, 1.8f, 0}};
      e.loot = {{"spider_silk", 0.8f}, {"shadow_essence", 0.05f}}; e.weak = Element::Fire; }
    // ---- High Crowns
    { auto& e = E("stone_golem", "Golem de pierre", "Golems", R_HIGHCROWNS, 13, 320, 28, 18, 3.f, Behavior::Tank, "Les anciens gardiens des cols, éveillés par le Titan.");
      e.hlook = humanLook(Species::Golem, Body::Plate, 0x7a766e, 0x7a766e, Helm::None, 61); e.hlook.glow = hexColor(0x40ffd0); e.hlook.bulk = 1.6f; e.scale = 1.6f; e.radius = 0.8f; e.poise = 120;
      e.attacks = {melee(C_B_SWIPE, 3.f, 1, 2.4f, 0.6f), {C_B_SLAM, AttackType::Area, 3.4f, 1.6f, 6, 0.9f, Element::Physical, 4.f, 0}, {C_B_THROW, AttackType::Projectile, 18, 1.4f, 8, 0.9f, Element::Physical, 2.5f, 0}};
      e.loot = {{"stone", 1.f}, {"rare_ore", 0.4f}, {"crystal", 0.3f}}; e.weak = Element::Lightning; }
    { auto& e = E("dark_knight", "Chevalier corrompu", "Chevaliers corrompus", R_HIGHCROWNS, 14, 280, 30, 16, 4.f, Behavior::Melee, "D'anciens paladins de l'Ordre, perdus à l'appel de l'Abîme.");
      e.hlook = humanLook(Species::Knight, Body::Plate, 0x2a2238, 0x1a1a22, Helm::Great, 62); e.hlook.metal = hexColor(0x3a3644); e.hlook.emissiveEyes = 1; e.hlook.glow = hexColor(0xb06cff); e.hlook.cape = true;
      e.weapon = WeaponType::Sword; e.offhand = WeaponType::Shield; e.poise = 70;
      e.attacks = {melee(C_1H_A1, 2.2f, 1, 1.6f, 0.4f), melee(C_1H_A3, 2.3f, 1.4f, 3.5f, 0.55f), melee(C_1H_HEAVY, 2.8f, 1.5f, 7, 0.6f), melee(C_SHIELD_BASH, 2.f, 0.8f, 6, 0.3f)};
      e.loot = {{"iron_bar", 0.5f}, {"shadow_essence", 0.3f}, {"shadow_head", 0.03f}, {"shadow_chest", 0.03f}}; e.weak = Element::Light; }
    { auto& e = E("mountain_bandit", "Brigand des cimes", "Bandits", R_HIGHCROWNS, 12, 180, 24, 10, 4.6f, Behavior::Melee, "Les cols sont à eux. Le péage est votre vie.");
      e.hlook = humanLook(Species::Human, Body::Fur, 0x6a5a4a, 0x3a3028, Helm::Leather, 63); e.hlook.beard = true; e.weapon = WeaponType::Spear; e.group = 2;
      e.attacks = {melee(C_SP_A1, 2.8f, 1, 1.5f, 0.4f), melee(C_SP_A2, 2.8f, 1.2f, 3, 0.45f), {C_SP_HEAVY, AttackType::Leap, 7, 1.6f, 8, 0.5f, Element::Physical, 2.f, 0}};
      e.loot = {{"fur", 0.4f}, {"spear_iron", 0.05f}, {"potion_heal", 0.2f}}; e.gold = 30; }
    { auto& e = E("drakeling", "Dragonnet des cimes", "Dragons", R_HIGHCROWNS, 15, 260, 28, 12, 5.5f, Behavior::Ranged, "Les petits du Dragon Cendré ont trouvé refuge sur les sommets.");
      e.arch = Arch::Dragon; e.clook.c1 = hexColor(0x5a6a7a); e.clook.c2 = hexColor(0xa0a8b0); e.clook.c3 = hexColor(0x3a4a6a); e.clook.glow = hexColor(0x80d0ff); e.scale = 1.6f; e.radius = 0.8f;
      e.attacks = {melee(C_D_BITE, 2.6f, 1, 1.6f, 0.4f), {C_D_BREATH, AttackType::Breath, 8, 0.35f, 7, 0.7f, Element::Lightning, 2.f, 0}, melee(C_D_TAIL, 3.f, 1.2f, 5, 0.5f)};
      e.loot = {{"dragon_scale", 0.15f}, {"rare_ore", 0.3f}}; e.weak = Element::Ice; }

    // ---- bosses' bodies (indices referenced by BossDef) ----------------------------------------------
    { auto& e = E("b_root_guardian", "Le Gardien des Racines", "Boss", R_ELDERWOOD, 10, 1600, 30, 12, 3.2f, Behavior::Boss, "Le premier gardien choisi par Aldric. Le fragment l'a enraciné dans la folie.");
      e.hlook = humanLook(Species::Treant, Body::Rags, 0x4a3a2a, 0x4a3a2a, Helm::None, 71); e.hlook.bulk = 1.8f; e.hlook.emissiveEyes = 1; e.hlook.eyes = hexColor(0x9aff6a); e.hlook.glow = hexColor(0x9aff6a);
      e.scale = 3.3f; e.radius = 1.6f; e.poise = 400;
      e.attacks = {melee(C_B_SWIPE, 5.f, 1, 2.6f, 0.6f), {C_B_SLAM, AttackType::Area, 5.5f, 1.6f, 6, 0.9f, Element::Nature, 6.f, 0}, {C_B_STOMP, AttackType::Area, 6.f, 1.2f, 8, 0.8f, Element::Nature, 9.f, 1}, {C_B_ROAR, AttackType::Summon, 40, 0, 20, 0.6f, Element::Nature, 0, 1}};
      e.weak = Element::Fire; e.resist = Element::Nature; e.xp = 900; e.gold = 300; }
    { auto& e = E("b_skeleton_king", "Le Roi Squelette", "Boss", R_SHADOWS, 12, 1900, 34, 14, 4.2f, Behavior::Boss, "Morneval avait un roi. Il règne encore sur ses catacombes.");
      e.hlook = humanLook(Species::Skeleton, Body::Bones, 0x4a1a1a, 0x3a1a1a, Helm::Crown, 72); e.hlook.emissiveEyes = 2; e.hlook.glow = hexColor(0x6ff0ff); e.hlook.cape = true; e.hlook.pauldrons = true;
      e.weapon = WeaponType::Longsword; e.scale = 1.9f; e.radius = 0.9f; e.poise = 300;
      e.attacks = {melee(C_2H_A1, 3.6f, 1, 2.f, 0.55f), melee(C_2H_A3, 3.6f, 1.5f, 4.5f, 0.75f), melee(C_B_SPIN, 4.f, 1.2f, 8, 0.7f), {C_B_ROAR, AttackType::Summon, 40, 0, 18, 0.6f, Element::Shadow, 0, 1}, {C_CAST_FORWARD, AttackType::Projectile, 20, 1.3f, 5, 0.6f, Element::Shadow, 2.f, 1}};
      e.weak = Element::Light; e.xp = 1100; e.gold = 400; }
    { auto& e = E("b_ice_colossus", "Le Colosse de Glace", "Boss", R_FROSTHEIM, 13, 2400, 38, 20, 2.8f, Behavior::Boss, "Gardien du lac de Givre. Son cœur bat au rythme du fragment.");
      e.hlook = humanLook(Species::Golem, Body::Plate, 0xb0d8f0, 0xb0d8f0, Helm::None, 73); e.hlook.glow = hexColor(0x9ae6ff); e.hlook.bulk = 1.9f;
      e.scale = 3.6f; e.radius = 1.8f; e.poise = 500;
      e.attacks = {melee(C_B_SWIPE, 6.f, 1, 2.8f, 0.7f), {C_B_SLAM, AttackType::Area, 6.f, 1.7f, 6, 1.f, Element::Ice, 7.f, 0}, {C_B_THROW, AttackType::Projectile, 30, 1.5f, 7, 1.f, Element::Ice, 3.5f, 0}, {C_B_STOMP, AttackType::Area, 6.f, 1.3f, 9, 0.9f, Element::Ice, 11.f, 1}};
      e.weak = Element::Fire; e.resist = Element::Ice; e.xp = 1300; e.gold = 450; }
    { auto& e = E("b_ash_dragon", "Le Dragon Cendré", "Boss", R_ASHEN, 15, 3200, 42, 18, 4.5f, Behavior::Boss, "Il couvait le fragment au cœur du volcan. Il ne le rendra pas.");
      e.arch = Arch::Dragon; e.clook.c1 = hexColor(0x3a3634); e.clook.c2 = hexColor(0x8a5a3a); e.clook.c3 = hexColor(0x7a2a1a); e.clook.glow = hexColor(0xff8020);
      e.scale = 5.f; e.radius = 2.4f; e.poise = 600;
      e.attacks = {melee(C_D_BITE, 6.f, 1, 2.2f, 0.6f), melee(C_D_TAIL, 7.f, 1.3f, 5, 0.6f), {C_D_BREATH, AttackType::Breath, 14, 0.45f, 8, 0.9f, Element::Fire, 3.f, 0}, {C_D_WING, AttackType::Area, 10.f, 0.8f, 10, 0.8f, Element::Physical, 10.f, 1}, {C_D_STOMP, AttackType::Area, 6, 1.4f, 7, 0.8f, Element::Fire, 7.f, 1}};
      e.weak = Element::Ice; e.resist = Element::Fire; e.xp = 1700; e.gold = 600; }
    { auto& e = E("b_spider_queen", "La Reine des Araignées", "Boss", R_SHADOWS, 14, 2600, 36, 14, 5.f, Behavior::Boss, "Elle a tissé sa toile autour du fragment… et de ceux qui le cherchaient.");
      e.arch = Arch::Spider; e.clook.c1 = hexColor(0x241c28); e.clook.c2 = hexColor(0xb030ff); e.clook.c3 = hexColor(0x100c14); e.clook.glow = hexColor(0xd060ff); e.clook.crystal = true;
      e.scale = 3.6f; e.radius = 2.f; e.poise = 450;
      e.attacks = {melee(C_S_BITE, 5.f, 1, 2.f, 0.5f), {C_S_SPIT, AttackType::Projectile, 25, 1.f, 3, 0.6f, Element::Nature, 3.f, 0}, {C_S_LEAP, AttackType::Leap, 18, 1.5f, 7, 0.7f, Element::Physical, 5.f, 0}, {C_S_SPIT, AttackType::Summon, 40, 0, 16, 0.5f, Element::Nature, 0, 1}};
      e.weak = Element::Fire; e.xp = 1500; e.gold = 500; }
    { auto& e = E("b_fallen_knight", "Le Chevalier Déchu", "Boss", R_SHADOWS, 16, 2800, 44, 22, 4.6f, Behavior::Boss, "Sire Corwin, champion d'Aldric. Il garde encore le sceau qu'il a juré de protéger.");
      e.hlook = humanLook(Species::Knight, Body::Plate, 0x1a1420, 0x14101a, Helm::Great, 74); e.hlook.metal = hexColor(0x2e2a36); e.hlook.emissiveEyes = 2; e.hlook.glow = hexColor(0xc070ff); e.hlook.cape = true; e.hlook.pauldrons = true; e.hlook.c1 = hexColor(0x3a1030);
      e.weapon = WeaponType::Longsword; e.scale = 1.55f; e.radius = 0.8f; e.poise = 350;
      e.attacks = {melee(C_2H_A1, 3.f, 1, 1.4f, 0.45f), melee(C_2H_A2, 3.f, 1.1f, 2.f, 0.45f), melee(C_2H_A3, 3.f, 1.5f, 4.f, 0.65f), {C_2H_HEAVY, AttackType::Leap, 12, 1.7f, 7, 0.6f, Element::Shadow, 4.f, 0}, {C_CAST_AREA, AttackType::Area, 6.f, 1.4f, 9, 0.8f, Element::Shadow, 7.f, 1}, melee(C_B_SPIN, 3.5f, 1.3f, 6, 0.5f)};
      e.weak = Element::Light; e.xp = 2000; e.gold = 700; }
    { auto& e = E("b_stone_titan", "Le Titan de Pierre", "Boss", R_HIGHCROWNS, 17, 3600, 46, 26, 2.6f, Behavior::Boss, "La montagne elle-même s'est levée pour garder le fragment.");
      e.hlook = humanLook(Species::Golem, Body::Plate, 0x8a8478, 0x8a8478, Helm::None, 75); e.hlook.glow = hexColor(0xffd060); e.hlook.bulk = 2.f;
      e.scale = 4.4f; e.radius = 2.2f; e.poise = 700;
      e.attacks = {melee(C_B_SWIPE, 7.f, 1, 3.f, 0.8f), {C_B_SLAM, AttackType::Area, 7.f, 1.7f, 6, 1.1f, Element::Lightning, 8.f, 0}, {C_B_THROW, AttackType::Projectile, 35, 1.5f, 7, 1.1f, Element::Physical, 4.f, 0}, {C_B_SPIN, AttackType::Area, 7.f, 1.2f, 11, 0.9f, Element::Physical, 8.f, 1}, {C_B_STOMP, AttackType::Area, 7.f, 1.4f, 9, 1.f, Element::Lightning, 13.f, 1}};
      e.weak = Element::Ice; e.xp = 2200; e.gold = 800; }
    { auto& e = E("b_abyss_lord", "Le Seigneur des Abysses", "Boss", R_SHADOWS, 20, 5000, 52, 24, 4.2f, Behavior::Boss, "Malgrath, ce qui attendait sous Morneval depuis l'aube du monde.");
      e.hlook = humanLook(Species::Demon, Body::Robe, 0x1a0a20, 0x100814, Helm::None, 76); e.hlook.skin = hexColor(0x3a1a2a); e.hlook.emissiveEyes = 3; e.hlook.eyes = hexColor(0xff40ff); e.hlook.glow = hexColor(0xd040ff); e.hlook.bulk = 1.4f; e.hlook.hair = Hair::None; e.hlook.cape = true;
      e.weapon = WeaponType::Staff; e.scale = 2.6f; e.radius = 1.2f; e.poise = 600;
      e.attacks = {{C_CAST_FORWARD, AttackType::Projectile, 28, 1.2f, 2.2f, 0.5f, Element::Shadow, 2.5f, 0}, {C_CAST_AREA, AttackType::Area, 8, 1.6f, 7, 0.9f, Element::Shadow, 9.f, 0}, {C_STAFF_CAST, AttackType::Beam, 22, 0.5f, 10, 0.9f, Element::Shadow, 1.5f, 1},
                   {C_B_ROAR, AttackType::Summon, 40, 0, 18, 0.6f, Element::Shadow, 0, 1}, melee(C_STAFF_HEAVY, 4.f, 1.6f, 6, 0.7f), {C_CAST_SELF, AttackType::Area, 30, 1.2f, 14, 1.4f, Element::Shadow, 30.f, 2}};
      e.weak = Element::Light; e.resist = Element::Shadow; e.xp = 5000; e.gold = 2000; }
}

const std::vector<EnemyDef>& enemies() { std::call_once(gEnemiesOnce, buildEnemies); return gEnemies; }
int enemyIndex(const std::string& id) { auto& e = enemies(); for (int i = 0; i < (int)e.size(); i++) if (e[i].id == id) return i; return -1; }

const std::vector<BossDef>& bosses() {
    static std::vector<BossDef> b;
    static std::once_flag once;
    std::call_once(once, [] {
        auto B = [&](const char* id, const char* name, const char* title, const char* enemy, const char* arena, int dungeon, std::vector<float> phases, std::vector<std::string> rewards, const char* shard, const char* intro, const char* outro) {
            BossDef d; d.id = id; d.name = name; d.title = title; d.enemy = enemyIndex(enemy); d.arenaPoi = arena; d.dungeon = dungeon; d.phases = phases; d.rewards = rewards; d.shard = shard; d.intro = intro; d.outro = outro; d.music = "boss";
            b.push_back(d);
        };
        B("root_guardian", "Le Gardien des Racines", "Premier Gardien d'Elderwood", "b_root_guardian", "root_glade", -1, {0.5f}, {"bow_elder", "sylvan_chest", "amulet_oak"}, "crown_shard",
          "« Tu portes l'odeur d'Aldric… La forêt ne rendra pas ce qu'on lui a confié. »", "Les racines se détendent. Une voix ancienne murmure : « Merci… »");
        B("skeleton_king", "Le Roi Squelette", "Souverain des catacombes", "b_skeleton_king", "graveyard", 1, {0.6f, 0.3f}, {"relic_kings_seal", "shield_iron", "ring_sapphire"}, "crown_shard",
          "« Un roi ne s'agenouille pas. Pas même devant la mort. »", "La couronne d'os roule sur les dalles et se brise en poussière.");
        B("ice_colossus", "Le Colosse de Glace", "Cœur du lac de Givre", "b_ice_colossus", "frozen_lake", -1, {0.5f}, {"longsword_frost", "frost_chest", "relic_jarl_horn"}, "crown_shard",
          "La glace du lac craque. Quelque chose d'immense se redresse sous la neige.", "Le colosse se fend comme un iceberg au printemps.");
        B("ash_dragon", "Le Dragon Cendré", "Fléau des Ashen Lands", "b_ash_dragon", "ash_nest", -1, {0.6f, 0.3f}, {"sword_ember", "dragon_scale", "ember_chest"}, "crown_shard",
          "Un rugissement fait trembler le volcan. Des ailes de braise masquent le ciel.", "Le dragon s'effondre ; ses écailles s'éteignent une à une.");
        B("spider_queen", "La Reine des Araignées", "Mère de l'Antre", "b_spider_queen", "spider_den", 2, {0.5f}, {"dagger_viper", "spider_silk", "shadow_feet"}, "crown_shard",
          "Des milliers d'yeux s'allument dans l'obscurité. La toile frémit.", "La Reine se recroqueville. Le silence revient dans l'Antre.");
        B("fallen_knight", "Le Chevalier Déchu", "Sire Corwin, Champion d'Aldric", "b_fallen_knight", "morneval", -1, {0.5f}, {"longsword_fallen", "mount_spectral", "shadow_chest"}, "",
          "« Halte. J'ai juré de garder ce sceau jusqu'au retour du roi. Es-tu ce roi ? Prouve-le. »", "« Tu as… ses yeux. Pardonne-moi, mon roi. Le passage est à toi. »");
        B("stone_titan", "Le Titan de Pierre", "La Montagne éveillée", "b_stone_titan", "titan_cirque", -1, {0.6f, 0.3f}, {"hammer_titan", "amulet_star", "crown_head"}, "crown_shard",
          "Le cirque entier se met à gronder. La montagne ouvre les yeux.", "Le titan retourne à la pierre, apaisé.");
        B("abyss_lord", "Le Seigneur des Abysses", "Malgrath, l'Affamé", "b_abyss_lord", "abyss", 5, {0.66f, 0.33f}, {"staff_abyss", "ring_abyss", "sword_aldric"}, "",
          "« Trois siècles… Aldric a brisé sa couronne pour moi. Toi, tu vas briser ta vie. »", "L'Abîme hurle, puis se referme. La Couronne-Mère brille d'une lumière nouvelle.");
    });
    return b;
}

// ============================================================================================ NPCs
static HumanoidLook villagerLook(bool female, uint32_t c1, uint32_t hair, Hair hs, Body body, uint32_t seed, bool beard = false) {
    HumanoidLook h; h.female = female; h.c1 = hexColor(c1); h.hairCol = hexColor(hair); h.hair = hs; h.body = body; h.seed = seed; h.beard = beard;
    static const uint32_t skins[] = {0xe8c0a0, 0xd8a888, 0xc08868, 0x9a6a4a, 0x7a4f36, 0xf0d0b8};
    h.skin = hexColor(skins[seed % 6]);
    return h;
}

const std::vector<NpcDef>& npcs() {
    static std::vector<NpcDef> n;
    static std::once_flag once;
    std::call_once(once, [] {
        auto N = [&](const char* id, const char* name, const char* title, NpcRole role, const char* home, HumanoidLook look, vec2 off, std::vector<std::string> greet, std::vector<std::string> lore, std::vector<std::string> shop = {}, int faction = -1) {
            NpcDef d; d.id = id; d.name = name; d.title = title; d.role = role; d.home = home; d.look = look; d.offset = off; d.greetings = greet; d.lore = lore; d.shop = shop; d.faction = faction;
            n.push_back(d);
        };
        // Brindlemere
        { HumanoidLook l = villagerLook(true, 0x3f6a3a, 0xd8d0c8, Hair::Bun, Body::Robe, 3); l.c1 = hexColor(0x3f6a3a);
          N("maelle", "Maëlle", "Doyenne de Brindlemere", NpcRole::Elder, "brindlemere", l, {6, 4}, {"Les vagues t'ont rendu à nous, Errant.", "Le vent tourne. Je le sens dans mes os."},
            {"Il y a trois siècles, le Haut-Roi Aldric unifia six royaumes sous la Couronne-Mère.", "Quand l'Abîme s'ouvrit sous Morneval, Aldric brisa sa couronne pour sceller la faille. Six fragments, six gardiens.", "Les gardiens ont gardé les fragments… mais les fragments les ont changés."}, {}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(true, 0x6a4a3a, 0xa04a2a, Hair::Braids, Body::Apron, 4); l.bulk = 1.15f;
          N("hilda", "Hilda", "Forgeronne", NpcRole::Blacksmith, "brindlemere", l, {-10, 14}, {"L'acier ne ment jamais. Les hommes, si.", "Besoin d'une lame ? D'une réparation ?"},
            {"Ma grand-mère disait que les épées d'Aldric chantaient au combat.", "Le meilleur fer vient de la Mine des Cimes. Le plus pur, de Cinderwatch."}, {"sword_iron", "axe_iron", "spear_iron", "shield_wood", "dagger_iron", "tool_pickaxe", "tool_woodaxe", "chain_head"}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(true, 0x5a3a6a, 0x2a2a2a, Hair::Long, Body::Dress, 5);
          N("ysolde", "Mère Ysolde", "Alchimiste", NpcRole::Alchemist, "brindlemere", l, {14, -8}, {"Les plantes savent soigner. Il suffit de les écouter.", "Une potion ? J'en ai toujours une qui mijote."},
            {"Les champignons lumineux d'Elderwood décuplent la puissance des élixirs.", "Le lys de givre ne pousse que sous la neige de Frostheim."}, {"potion_minor", "potion_heal", "potion_mana", "potion_stamina", "herb"}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(false, 0x8a5a3a, 0x6a4a2a, Hair::Short, Body::Apron, 6, true); l.bulk = 1.3f;
          N("bram", "Bram", "Aubergiste du Chaudron Doré", NpcRole::Innkeeper, "brindlemere", l, {-14, -6}, {"Bienvenue au Chaudron Doré ! Un lit, un ragoût ?", "Les voyageurs racontent des choses étranges sur Morneval."},
            {"Les Écorcheurs rançonnent la route de Millbrook. Le capitaine Aubert n'a pas assez d'hommes.", "On dit que le Chevalier Déchu ne dort jamais."}, {"bread", "stew", "fish_grilled"}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(false, 0x3a5a2a, 0x8a6a3a, Hair::Ponytail, Body::Leather, 7);
          N("tomas", "Tomas", "Chasseur", NpcRole::Hunter, "brindlemere", l, {18, 10}, {"Chut… tu entends ? Les loups sont nerveux ces temps-ci.", "Un bon chasseur sait quand ne pas tirer."},
            {"Les loups gris ne s'approchaient jamais du village. Quelque chose les pousse hors des forêts.", "Le cuir des sangliers fait les meilleures bottes."}, {"bow_hunter", "leather", "meat", "leather_feet"}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(false, 0x2a4f7a, 0x3a2a1a, Hair::Short, Body::Chain, 8); l.helm = Helm::Iron; l.pauldrons = true;
          N("aubert", "Capitaine Aubert", "Gardien de Greenhaven", NpcRole::Guard, "brindlemere", l, {0, 22}, {"Halte, voyageur. Ah, c'est toi.", "Brindlemere est sous ma protection."},
            {"Fort Aldren tenait toute la région autrefois. Aujourd'hui, à peine la route.", "Les Gardiens recrutent quiconque sait tenir une épée."}, {}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(true, 0xc9a24a, 0xe8c87a, Hair::Long, Body::Dress, 9);
          N("lise", "Lise", "Marchande", NpcRole::Merchant, "brindlemere", l, {10, 16}, {"Tout s'achète, tout se vend !", "J'ai des choses de Frostheim, et même d'Elderwood."},
            {"Les marchands d'Ashen paient cher le minerai d'étoile.", "Une carte au trésor ? J'en vends parfois… certaines sont vraies."}, {"potion_minor", "bread", "berries", "ring_copper", "amulet_oak", "tool_rod", "dye_royal", "dye_crimson", "dye_forest", "treasure_map"}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(false, 0x6a8a3a, 0xd8a050, Hair::Short, Body::Tunic, 10);
          N("pip", "Pip", "Garnement", NpcRole::Child, "brindlemere", l, {-4, -12}, {"T'es un vrai chevalier ?!", "Je parie que je cours plus vite que toi !"},
            {"Ma mère dit qu'il ne faut pas aller dans la Grotte des Murmures. Moi j'y suis allé. Presque.", "Les lucioles sortent seulement la nuit, près du lac Miroir."}); }
        { HumanoidLook l = villagerLook(false, 0x6a5030, 0x4a3a2a, Hair::Short, Body::Leather, 11, true);
          N("odo", "Odo", "Maître des écuries", NpcRole::Stablemaster, "brindlemere", l, {-24, 6}, {"Un cheval, c'est un ami qui porte tes bagages.", "Mes bêtes sont les plus rapides de Greenhaven."},
            {"Un cheval te mènera d'un bout à l'autre du royaume sans jamais te demander pourquoi.", "Dans le nord, ils montent des loups géants. Des fous."}, {"mount_horse"}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(false, 0xe8e0d0, 0x8a8a8a, Hair::Short, Body::Robe, 12);
          N("anselme", "Frère Anselme", "Prêtre", NpcRole::Priest, "brindlemere", l, {-8, -20}, {"Que la Lumière guide tes pas.", "Les morts méritent le repos, eux aussi."},
            {"La chapelle fut bâtie sur les fondations d'un sanctuaire elfique.", "L'Ordre de Highspire garde les archives du Haut-Roi."}); }
        // Millbrook
        { HumanoidLook l = villagerLook(false, 0x8a7a5a, 0x6a5a4a, Hair::Short, Body::Apron, 13, true); l.bulk = 1.2f;
          N("gareth", "Gareth", "Meunier de Millbrook", NpcRole::Farmer, "millbrook", l, {8, -6}, {"La farine ne se moud pas toute seule !", "Si les bandits continuent, Brindlemere aura faim cet hiver."},
            {"Le moulin a été bâti par mon arrière-grand-père. Il tourne depuis cent ans.", "Les Écorcheurs campent à l'est, près des ruines."}, {"bread", "berries"}, FA_GREENHAVEN); }
        { HumanoidLook l = villagerLook(true, 0x7a5a3a, 0xc08a4a, Hair::Braids, Body::Dress, 14);
          N("margot", "Margot", "Fermière", NpcRole::Farmer, "millbrook", l, {-10, 8}, {"Fais attention aux sangliers, ils piétinent tout.", "Les citrouilles seront belles cette année."},
            {"Une bête énorme rôde près des champs la nuit. On l'appelle la Bête de Millbrook."}, {"bread", "berries", "herb"}); }
        // Frostvik
        { HumanoidLook l = villagerLook(false, 0x6a5a4a, 0xd8c8a8, Hair::Braids, Body::Fur, 15, true); l.helm = Helm::Horned; l.bulk = 1.3f; l.pauldrons = true;
          N("bjorn", "Jarl Bjorn", "Chef des clans", NpcRole::Elder, "frostvik", l, {6, 6}, {"Un sudiste ! Approche du feu avant de geler.", "Les clans honorent ceux qui se battent."},
            {"Le lac de Givre ne gèle jamais vraiment. Il retient son souffle.", "Nos ancêtres juraient fidélité à Aldric. Les Draugr s'en souviennent encore."}, {}, FA_FROSTHEIM); }
        { HumanoidLook l = villagerLook(true, 0x5a4a3a, 0xe8d8b8, Hair::Braids, Body::Fur, 16); l.bulk = 1.1f;
          N("sigrun", "Sigrun", "Forgeronne des clans", NpcRole::Blacksmith, "frostvik", l, {-12, 10}, {"Le froid durcit l'acier. Et les hommes.", "Apporte-moi du cristal de givre et je te forgerai une merveille."},
            {"L'acier trempé dans l'eau du lac de Givre ne rouille jamais."}, {"axe_iron", "longsword_iron", "hammer_iron", "shield_iron", "frost_head", "frost_feet", "elixir_frost"}, FA_FROSTHEIM); }
        { HumanoidLook l = villagerLook(true, 0x8a8a8a, 0x2a2a2a, Hair::Ponytail, Body::Fur, 17);
          N("eira", "Eira", "Dompteuse de loups", NpcRole::Hunter, "frostvik", l, {14, -10}, {"Fenra n'aime pas les étrangers. Elle t'observe.", "Les loups géants ne se domptent pas. Ils choisissent."},
            {"Les loups des neiges deviennent fous. Ils attaquent même les meutes géantes."}, {"fur", "meat", "potion_stamina"}, FA_FROSTHEIM); }
        { HumanoidLook l = villagerLook(false, 0x5a3a2a, 0xe8e8e8, Hair::Long, Body::Fur, 18, true);
          N("hakon", "Vieux Hakon", "Scalde", NpcRole::Bard, "frostvik", l, {-6, -14}, {"Assieds-toi, je vais te chanter la saga du premier Jarl.", "Chaque cicatrice est un vers."},
            {"Le premier Jarl a soufflé dans sa corne et cent clans ont répondu. La corne dort dans les Cavernes de Hrimgard.", "On raconte que le Colosse était un ami d'Aldric. Un géant de neige qui riait comme un enfant."}, {}, FA_FROSTHEIM); }
        // Sylvarin
        { HumanoidLook l = villagerLook(true, 0x2f6a5a, 0xe8e0f0, Hair::Long, Body::Robe, 19); l.species = Species::Elf; l.helm = Helm::Circlet; l.glow = hexColor(0x8ff0d8);
          N("aelith", "Aelith", "Druidesse du Cercle", NpcRole::Elder, "sylvarin", l, {4, 6}, {"La forêt t'a laissé entrer. C'est rare.", "Écoute les arbres. Ils souffrent."},
            {"Le Gardien des Racines était le plus doux d'entre nous. Le fragment l'a rendu fou.", "Les feux follets sont les âmes d'arbres abattus."}, {"mount_elk"}, FA_ELDERWOOD); }
        { HumanoidLook l = villagerLook(false, 0x4a7a5a, 0xd8c890, Hair::Long, Body::Leather, 20); l.species = Species::Elf;
          N("thalion", "Thalion", "Marchand sylvain", NpcRole::Merchant, "sylvarin", l, {-10, -8}, {"Les arcs elfiques sont les meilleurs. Je ne fais que dire la vérité.", "Que cherches-tu, enfant des hommes ?"},
            {"Le Sanctuaire d'Aelindor fut le premier temple de la forêt."}, {"bow_hunter", "staff_apprentice", "sylvan_head", "sylvan_feet", "glowcap", "dye_ivory"}, FA_ELDERWOOD); }
        { HumanoidLook l = villagerLook(true, 0x6a4a8a, 0x3a1a4a, Hair::Long, Body::Dress, 21); l.species = Species::Elf;
          N("nyssa", "Nyssa", "Herboriste", NpcRole::Alchemist, "sylvarin", l, {12, -10}, {"Les champignons lumineux chantent la nuit.", "Ne mange pas tout ce qui brille."},
            {"Une source magique coule au sud du village. Son eau guérit les blessures."}, {"potion_heal", "potion_mana", "glowcap", "herb", "potion_great"}, FA_ELDERWOOD); }
        // Cinderwatch
        { HumanoidLook l = villagerLook(false, 0x6a4a2a, 0x2a1a1a, Hair::Short, Body::Leather, 22, true); l.species = Species::Dwarf; l.bulk = 1.4f; l.helm = Helm::Leather;
          N("dorn", "Contremaître Dorn", "Guilde des Mineurs", NpcRole::Elder, "cinderwatch", l, {6, 4}, {"Si tu n'es pas là pour creuser, tu es là pour te battre.", "Mes gars ont disparu dans la mine. Personne ne m'aide."},
            {"Les Mines de Brasier plongent jusqu'au cœur du volcan.", "Le Dragon Cendré s'est réveillé le jour où on a percé la veine d'étoile."}, {}, FA_ASHEN); }
        { HumanoidLook l = villagerLook(true, 0x4a3a2a, 0xd04a2a, Hair::Mohawk, Body::Apron, 23); l.bulk = 1.25f;
          N("kara", "Kara", "Forgeronne de lave", NpcRole::Blacksmith, "cinderwatch", l, {-12, 8}, {"Ici, on forge à la chaleur du volcan.", "Ramène-moi des noyaux de braise, je te ferai une lame qui brûle."},
            {"Le minerai d'étoile se fond seulement dans la lave."}, {"crossbow_iron", "hammer_iron", "chain_chest", "chain_feet", "iron_bar", "copper_bar"}, FA_ASHEN); }
        { HumanoidLook l = villagerLook(false, 0x8a3a2a, 0x1a1a1a, Hair::Bun, Body::Robe, 24);
          N("vesk", "Vesk", "Négociant", NpcRole::Merchant, "cinderwatch", l, {12, -10}, {"Chaleur, poussière, profit.", "Je t'achète tout ton minerai d'étoile. À bon prix. Presque."},
            {"Les écailles de dragon valent une fortune à Highspire."}, {"potion_heal", "potion_stamina", "elixir_strength", "rare_ore", "dye_night", "mount_boar"}, FA_ASHEN); }
        // Gravemoor
        { HumanoidLook l = villagerLook(true, 0x2a2a3a, 0xd8d8d8, Hair::Bun, Body::Robe, 25);
          N("agathe", "Mère Agathe", "Veilleuse de Gravemoor", NpcRole::Elder, "gravemoor", l, {5, 5}, {"Garde ta lanterne allumée, enfant.", "Les morts sont agités. Ils sentent le sceau faiblir."},
            {"Le Château de Morneval était le siège d'Aldric. C'est là que l'Abîme s'est ouvert.", "Le Chevalier Déchu attend encore son roi."}, {}, FA_GRAVEMOOR); }
        { HumanoidLook l = villagerLook(false, 0x3a3a30, 0x5a5a5a, Hair::Short, Body::Rags, 26, true);
          N("silas", "Silas", "Fossoyeur", NpcRole::Villager, "gravemoor", l, {-10, 8}, {"Je creuse, ils ressortent. Un métier sans fin.", "Tu veux une pelle ? Non ? Dommage."},
            {"Les catacombes passent sous tout le cimetière. Le Roi y dort. Enfin, il y était."}, {"bone"}, FA_GRAVEMOOR); }
        { HumanoidLook l = villagerLook(true, 0x3a2a4a, 0x1a1a2a, Hair::Long, Body::Dress, 27); l.helm = Helm::Hood;
          N("morwen", "Morwen", "Sorcière des marais", NpcRole::Alchemist, "gravemoor", l, {12, -8}, {"Mes potions sentent mauvais. Elles marchent d'autant mieux.", "L'essence d'ombre… apporte-m'en."},
            {"Les araignées de l'Antre gardent un trésor : la Reine elle-même."}, {"potion_heal", "potion_mana", "elixir_strength", "potion_great", "dye_night"}, FA_GRAVEMOOR); }
        // Highspire
        { HumanoidLook l = villagerLook(false, 0x6a6aa0, 0xb0b0b0, Hair::None, Body::Robe, 28);
          N("alaric", "Frère Alaric", "Archiviste de l'Ordre", NpcRole::Scholar, "highspire", l, {4, -8}, {"Les archives ont attendu ce jour pendant trois siècles.", "Le savoir est une lumière qui ne s'éteint pas."},
            {"Seule la forge de l'Ordre peut réunir les fragments en une Couronne-Mère.", "Aldric avait une épouse, Elsa. Elle a disparu le jour de la Chute."}, {}, FA_HIGHCROWNS); }
        { HumanoidLook l = villagerLook(true, 0xe8e0d0, 0xc8a050, Hair::Bun, Body::Robe, 29); l.helm = Helm::Circlet; l.glow = hexColor(0xffe0a0);
          N("odile", "Sœur Odile", "Guérisseuse", NpcRole::Priest, "highspire", l, {-10, 6}, {"Viens, que je soigne ces blessures.", "La Lumière n'abandonne personne."},
            {"L'Égide de Highspire fut forgée pour protéger le roi de l'ombre."}, {"potion_heal", "potion_great", "shield_aegis", "dye_gold"}, FA_HIGHCROWNS); }
        { HumanoidLook l = villagerLook(false, 0x4a6a8a, 0x3a2a1a, Hair::Short, Body::Tunic, 30, true);
          N("corvin", "Maître Corvin", "Intendant", NpcRole::Merchant, "highspire", l, {12, 12}, {"Highspire accueille les pèlerins. Et leur argent.", "Tout ce qu'il faut pour les cols."},
            {"Les cols sont dangereux : golems, brigands, et pire."}, {"potion_heal", "potion_stamina", "spear_crown", "chain_hands", "ring_sapphire", "amulet_ember"}, FA_HIGHCROWNS); }
        { HumanoidLook l = villagerLook(false, 0x3a3a6a, 0x8a8a9a, Hair::Long, Body::Robe, 31, true);
          N("ilian", "Ilian", "Astronome", NpcRole::Scholar, "astronomer", l, {6, 4}, {"Les étoiles ont bougé le jour de la Chute.", "Regarde là-haut. Tu vois cette constellation ? La Couronne."},
            {"Trois cristaux d'étoile sont tombés sur les cimes. Ils formaient la carte céleste d'Aldric."}, {}, FA_HIGHCROWNS); }
    });
    return n;
}
int npcIndex(const std::string& id) { auto& n = npcs(); for (int i = 0; i < (int)n.size(); i++) if (n[i].id == id) return i; return -1; }

// ============================================================================================ quests
const std::vector<QuestDef>& quests() {
    static std::vector<QuestDef> q;
    static std::once_flag once;
    std::call_once(once, [] {
        auto Q = [&](const char* id, const char* title, QuestType t, const char* giver, const char* desc, const char* offer, const char* progress, const char* done,
                     std::vector<QuestStep> steps, int xp, int gold, std::vector<std::string> rewards, int faction = -1, int rep = 0, const char* req = "", int shards = 0) {
            QuestDef d; d.id = id; d.title = title; d.type = t; d.giver = giver; d.desc = desc; d.offer = offer; d.progress = progress; d.done = done;
            d.steps = steps; d.xp = xp; d.gold = gold; d.rewards = rewards; d.faction = faction; d.rep = rep; d.requires_ = req; d.minShards = shards;
            q.push_back(d);
        };
        using K = StepKind;
        // ---- Main story: La Chute des Couronnes
        Q("m1", "Un réveil sur la grève", QuestType::Main, "", "Vous vous réveillez sur la plage de Greenhaven, sans souvenirs. Une marque en forme de couronne brûle sur votre poignet.", "", "", "",
          {{"Parler à Maëlle, la doyenne de Brindlemere", K::Talk, "maelle", 1, "brindlemere"}}, 60, 10, {"potion_minor", "potion_minor"});
        Q("m2", "Les Murmures", QuestType::Main, "maelle", "Maëlle pense que la Pierre d'écho, cachée dans la Grotte des Murmures, pourrait réveiller vos souvenirs.",
          "Ta marque… Je l'ai vue sur les tapisseries du Haut-Roi. La Grotte des Murmures, au sud, garde une Pierre d'écho. Elle répète les voix du passé. Va la chercher.",
          "La grotte est au sud du village. Méfie-toi des gobelins.", "La pierre… écoute. C'est la voix d'Aldric. Il parle des six fragments. Il parle de toi.",
          {{"Explorer la Grotte des Murmures", K::Dungeon, "whisper_cave", 1, "whisper_cave"}, {"Rapporter la Pierre d'écho à Maëlle", K::Deliver, "echo_stone", 1, "brindlemere"}}, 220, 60, {"sword_iron", "leather_chest"}, FA_GREENHAVEN, 10, "m1");
        Q("m3", "La Couronne brisée", QuestType::Main, "maelle", "Les fragments de la Couronne-Mère doivent être réunis. L'Ordre de Highspire saura comment les reforger.",
          "Les six fragments sont entre les mains des gardiens corrompus. Mais d'abord, va voir Frère Alaric à Highspire, au nord-est. Les archives de l'Ordre te diront comment reforger la Couronne. Tiens, porte-lui cette lettre.",
          "Highspire est au nord-est, au-delà de Millbrook. Suis la route des pèlerins.", "",
          {{"Remettre la lettre de Maëlle à Frère Alaric (Highspire)", K::Deliver, "letter_maelle", 1, "highspire"}}, 300, 80, {"amulet_oak"}, FA_HIGHCROWNS, 10, "m2");
        Q("m4", "Les Gardiens corrompus", QuestType::Main, "alaric", "Récupérez les six fragments auprès des gardiens corrompus, dans l'ordre de votre choix.",
          "La lettre dit vrai : tu portes le sang d'Aldric. Six gardiens, six fragments : le Gardien des Racines à Elderwood, le Roi Squelette sous Gravemoor, le Colosse de Glace à Frostheim, le Dragon Cendré dans les Ashen Lands, la Reine des Araignées dans son Antre, et le Titan de Pierre ici, dans les cimes. Rapporte-les-moi. L'ordre t'appartient.",
          "Combien de fragments ? Chacun te rapproche du sceau.", "Six. Les six fragments. Je n'aurais jamais cru vivre ce jour.",
          {{"Réunir les fragments de Couronne", K::Collect, "crown_shard", 6, ""}}, 2500, 500, {"potion_great", "potion_great"}, FA_HIGHCROWNS, 30, "m3");
        Q("m5", "La Couronne-Mère", QuestType::Main, "alaric", "Frère Alaric peut reforger la Couronne-Mère à la forge de l'Ordre.",
          "Donne-moi les fragments. La forge de l'Ordre attend depuis trois siècles.", "La forge chauffe. Rapporte-moi les fragments.",
          "Voilà. La Couronne-Mère. Mais le sceau ne peut être refermé qu'à Morneval… où veille Sire Corwin, le Chevalier Déchu.",
          {{"Remettre les six fragments à Frère Alaric", K::Deliver, "crown_shard", 6, "highspire"}}, 800, 200, {"mother_crown", "amulet_crown"}, FA_HIGHCROWNS, 30, "m4");
        Q("m6", "Le Chevalier Déchu", QuestType::Main, "alaric", "Sire Corwin garde l'entrée de l'Abîme au Château de Morneval.",
          "Sire Corwin était le champion d'Aldric. Il garde encore le sceau. Il ne te laissera passer que s'il te reconnaît. Ou s'il tombe.",
          "Morneval est au sud, au-delà de Gravemoor.", "",
          {{"Vaincre le Chevalier Déchu à Morneval", K::KillBoss, "fallen_knight", 1, "morneval"}}, 1500, 300, {}, FA_GRAVEMOOR, 20, "m5");
        Q("m7", "L'Abîme", QuestType::Main, "", "L'Abîme s'ouvre sous Morneval. Malgrath attend. Scellez-le avec la Couronne-Mère.", "", "", "",
          {{"Descendre dans l'Abîme et vaincre le Seigneur des Abysses", K::KillBoss, "abyss_lord", 1, "abyss"}}, 6000, 1500, {}, -1, 0, "m6");
        // ---- Side quests
        Q("s_wolves", "Le loup de la lisière", QuestType::Side, "tomas", "Tomas veut éloigner les loups qui rôdent près de Brindlemere.",
          "Les loups gris s'approchent trop du village. Abats-en cinq et je te paierai en cuir et en or.", "Encore des loups dans les bois…", "Bon travail. Les enfants pourront de nouveau jouer près de la lisière.",
          {{"Tuer des loups gris", K::Kill, "wolf", 5, ""}}, 150, 40, {"leather_head", "potion_minor"}, FA_GREENHAVEN, 10);
        Q("s_bandits", "Farine et fléaux", QuestType::Side, "gareth", "Les Écorcheurs pillent les convois de farine de Millbrook.",
          "Les Écorcheurs volent ma farine ! Leur camp est à l'est. Chasse-les, je t'en prie.", "Ils sont toujours là-bas, à l'est du moulin.", "Le moulin tourne de nouveau sans crainte. Merci !",
          {{"Vaincre des bandits", K::Kill, "bandit", 4, "bandit_camp"}, {"Vaincre des bandits archers", K::Kill, "bandit_archer", 2, "bandit_camp"}}, 260, 90, {"shield_iron", "bread", "bread"}, FA_GREENHAVEN, 15);
        Q("s_iron", "Le fer de Hilda", QuestType::Side, "hilda", "Hilda manque de minerai de fer pour sa forge.",
          "Mon stock de fer est vide. Rapporte-moi cinq minerais de fer, et je t'apprendrai à forger.", "Cinq minerais, pas un de moins.", "Parfait ! Voilà de quoi te défendre. La forge t'est ouverte.",
          {{"Rapporter du minerai de fer", K::Deliver, "iron_ore", 5, "brindlemere"}}, 120, 30, {"axe_iron", "iron_bar", "iron_bar"}, FA_GREENHAVEN, 10);
        Q("s_herbs", "Les herbes de Mère Ysolde", QuestType::Side, "ysolde", "Ysolde a besoin d'herbes médicinales.",
          "Mes réserves d'herbes s'épuisent. Cueilles-en six dans les prairies, veux-tu ?", "Six herbes, mon enfant.", "Merci ! Prends ces potions. La table d'alchimie t'est ouverte.",
          {{"Récolter des herbes médicinales", K::Deliver, "herb", 6, "brindlemere"}}, 110, 20, {"potion_heal", "potion_heal"}, FA_GREENHAVEN, 10);
        Q("s_jarl", "Le Jarl et la tempête", QuestType::Side, "bjorn", "Bjorn veut prouver la valeur de l'étranger.",
          "Tu veux l'aide des clans ? Prouve ta valeur. Abats deux trolls des glaces et rapporte leur peau.", "Les trolls rôdent au nord du village.", "Ha ! Les clans te saluent, frère d'armes.",
          {{"Vaincre des trolls des glaces", K::Kill, "frost_troll", 2, ""}}, 600, 150, {"axe_nordic", "frost_head"}, FA_FROSTHEIM, 25);
        Q("s_crystals", "Glace éternelle", QuestType::Side, "sigrun", "Sigrun veut forger une lame dans la glace éternelle.",
          "Quatre cristaux de givre. Les élémentaires en portent dans leur cœur.", "Il me faut encore des cristaux.", "Magnifique. Regarde comme ils chantent sur l'enclume.",
          {{"Rapporter des cristaux de givre", K::Deliver, "frost_crystal", 4, "frostvik"}}, 450, 120, {"frost_feet", "elixir_frost"}, FA_FROSTHEIM, 15);
        Q("s_fenra", "Le choix de Fenra", QuestType::Side, "eira", "Eira veut savoir si son loup géant vous accepterait.",
          "Fenra est inquiète : les loups des neiges menacent sa meute. Chasse-en cinq. Si elle t'accepte, elle te portera.", "Fenra t'observe encore.", "Fenra pose sa tête contre ta main. Elle t'a choisi.",
          {{"Tuer des loups des neiges", K::Kill, "snow_wolf", 5, ""}}, 500, 0, {"mount_wolf"}, FA_FROSTHEIM, 20);
        Q("s_spirits", "Le chant des esprits", QuestType::Side, "aelith", "Les feux follets attirent les voyageurs vers la mort.",
          "Les feux follets sont des âmes perdues. Libères-en six, puis visite le Sanctuaire d'Aelindor.", "Les esprits pleurent encore.", "Le chant de la forêt est redevenu clair. Merci, Errant.",
          {{"Libérer des feux follets", K::Kill, "wisp", 6, ""}, {"Visiter le Sanctuaire d'Aelindor", K::Discover, "elven_ruins", 1, "elven_ruins"}}, 520, 120, {"sylvan_hands", "staff_apprentice"}, FA_ELDERWOOD, 25);
        Q("s_glowcaps", "Graines de lumière", QuestType::Side, "nyssa", "Nyssa a besoin de champignons lumineux.",
          "Cinq champignons lumineux. Ils poussent au pied des arbres-mères.", "Ils brillent la nuit, c'est plus facile.", "Merci. Tiens, cet élixir t'aidera.",
          {{"Rapporter des champignons lumineux", K::Deliver, "glowcap", 5, "sylvarin"}}, 300, 60, {"potion_great", "potion_mana"}, FA_ELDERWOOD, 10);
        Q("s_miners", "Les mineurs disparus", QuestType::Side, "dorn", "Les mineurs de Dorn ont disparu dans les Mines de Brasier.",
          "Mes gars sont descendus il y a trois jours. Personne n'est remonté. Trouve-les. Ou ce qu'il en reste.", "Les mines sont à l'est.", "Tu as trouvé l'insigne de Harg… Au moins, on sait. La Guilde te doit une dette.",
          {{"Explorer les Mines de Brasier", K::Dungeon, "ember_mines", 1, "ember_mines"}, {"Rapporter l'insigne du contremaître", K::Deliver, "miners_badge", 1, "cinderwatch"}}, 800, 250, {"crossbow_iron", "relic_miner_lamp"}, FA_ASHEN, 30);
        Q("s_embers", "Le cœur de braise", QuestType::Side, "kara", "Kara veut des noyaux de braise pour sa forge.",
          "Les élémentaires de braise ont un cœur qui ne s'éteint pas. Apporte-m'en trois.", "Trois noyaux, et je te forge quelque chose d'unique.", "Ha, regarde-les briller ! Prends ça.",
          {{"Rapporter des noyaux de braise", K::Deliver, "ember_core", 3, "cinderwatch"}}, 650, 160, {"ember_hands", "ember_feet"}, FA_ASHEN, 15);
        Q("s_restless", "Les morts sans repos", QuestType::Side, "agathe", "Les squelettes de Morneval sortent du cimetière.",
          "Les morts se lèvent chaque nuit. Rends à huit d'entre eux le repos.", "La nuit, ils sont plus nombreux.", "Gravemoor dormira mieux. Que la lumière te garde.",
          {{"Vaincre des squelettes", K::Kill, "skeleton", 8, ""}}, 480, 120, {"grave_lantern", "potion_heal"}, FA_GRAVEMOOR, 20);
        Q("s_shadow", "Essence d'ombre", QuestType::Side, "morwen", "Morwen veut de l'essence d'ombre.",
          "Trois essences d'ombre. Les spectres en sont faits.", "Les spectres rôdent la nuit, près du cimetière.", "Hé hé. Excellente cueillette. Tiens, une babiole qui te plaira.",
          {{"Rapporter de l'essence d'ombre", K::Deliver, "shadow_essence", 3, "gravemoor"}}, 520, 140, {"ring_wolf", "shadow_hands"}, FA_GRAVEMOOR, 15);
        Q("s_stars", "Les étoiles de l'astronome", QuestType::Side, "ilian", "Ilian veut reconstituer la carte céleste d'Aldric.",
          "Trois cristaux sont tombés sur les cimes. Rapporte-les et je te montrerai la carte d'Aldric.", "Les cristaux brillent sur les hauteurs.", "La carte céleste… elle montre l'Abîme sous Morneval. Prends cette larme d'étoile.",
          {{"Rapporter des cristaux", K::Deliver, "crystal", 3, "astronomer"}}, 700, 200, {"amulet_star", "star_chart"}, FA_HIGHCROWNS, 20);
        Q("s_golems", "Le pont du pèlerin", QuestType::Side, "corvin", "Des golems bloquent les cols des pèlerins.",
          "Les golems de pierre se sont réveillés sur les cols. Brise-en trois.", "Les cols sont au nord.", "Les pèlerins reviennent. L'Ordre te remercie.",
          {{"Briser des golems de pierre", K::Kill, "stone_golem", 3, ""}}, 900, 260, {"spear_crown", "chain_hands"}, FA_HIGHCROWNS, 25);
        Q("s_ice", "La corne du premier Jarl", QuestType::Side, "hakon", "La corne légendaire dort dans les Cavernes de Hrimgard.",
          "La corne du premier Jarl dort dans les Cavernes de Hrimgard. Rapporte-la et ta saga sera chantée.", "Les cavernes sont à l'ouest du village.", "La corne… Elle sonne encore ! Ton nom sera dans la saga.",
          {{"Explorer les Cavernes de Hrimgard", K::Dungeon, "ice_caves", 1, "ice_caves"}}, 900, 200, {"ring_wolf"}, FA_FROSTHEIM, 30);
        // ---- Hunting contracts
        Q("h_beast", "Contrat : la Bête de Millbrook", QuestType::Hunt, "margot", "Un sanglier énorme saccage les champs la nuit.", "Il est énorme. Ses défenses sont comme des faux. Tue-le.", "Il vient la nuit, près des champs.", "Enfin ! Les champs sont saufs.",
          {{"Tuer des sangliers furieux", K::Kill, "boar", 3, ""}}, 200, 80, {"leather_feet"});
        Q("h_spiders", "Contrat : la toile d'Elderwood", QuestType::Hunt, "thalion", "Les araignées sylvestres infestent la route.", "Quatre araignées. Brûle leurs toiles si tu peux.", "Elles se cachent dans les branches.", "La route est libre. Bien joué.",
          {{"Tuer des araignées sylvestres", K::Kill, "forest_spider", 4, ""}}, 420, 120, {"spider_silk", "spider_silk"});
        Q("h_bears", "Contrat : les ours de cendre", QuestType::Hunt, "vesk", "Les ours de cendre attaquent les caravanes.", "Deux ours. Leur fourrure vaut de l'or.", "Ils fument, tu ne peux pas les rater.", "Voilà ta prime.",
          {{"Tuer des ours de cendre", K::Kill, "ash_bear", 2, ""}}, 700, 300, {"elixir_strength"});
        Q("h_drakes", "Contrat : dragonnets des cimes", QuestType::Hunt, "corvin", "Les dragonnets terrorisent les bergers.", "Deux dragonnets. Méfie-toi de leur souffle.", "Ils nichent sur les hauteurs.", "Les bergers te bénissent.",
          {{"Tuer des dragonnets des cimes", K::Kill, "drakeling", 2, ""}}, 1000, 400, {"dragon_scale"});
        // ---- Treasure hunts
        Q("t_vael", "Le trésor de Vael", QuestType::Treasure, "lise", "Une carte indique un trésor dans les Ruines de Vael.", "Cette carte vient d'un vieux marin. Le X est sur les Ruines de Vael, à l'est.", "", "",
          {{"Trouver le trésor des Ruines de Vael", K::Discover, "vael_ruins", 1, "vael_ruins"}}, 180, 150, {"ring_sapphire"});
        Q("t_bastion", "Les coffres du Bastion", QuestType::Treasure, "vesk", "Les défenseurs du Bastion Calciné cachaient leur solde.", "La solde du Bastion n'a jamais été retrouvée. Elle est à toi si tu oses.", "", "",
          {{"Fouiller le Bastion Calciné", K::Discover, "ashen_ruins", 1, "ashen_ruins"}}, 600, 500, {"amulet_ember"});
        // ---- Hidden quests (start when discovered)
        Q("x_harp", "La harpe oubliée", QuestType::Hidden, "", "Une harpe vibre seule dans le Sanctuaire d'Aelindor.", "", "", "",
          {{"Explorer le Sanctuaire d'Aelindor", K::Discover, "elven_ruins", 1, "elven_ruins"}}, 300, 0, {"relic_elven_harp"});
        Q("x_stones", "Les sept pierres", QuestType::Hidden, "", "Les pierres de passage semblent liées entre elles.", "", "", "",
          {{"Visiter les pierres de passage", K::Discover, "waystones", 6, ""}}, 900, 300, {"relic_star_glass", "dye_gold"});
        Q("x_ring", "L'alliance d'Aldric", QuestType::Hidden, "", "Une alliance gravée gît dans les ruines de Morneval.", "", "", "",
          {{"Explorer le Château de Morneval", K::Discover, "morneval", 1, "morneval"}}, 400, 0, {"relic_aldric_ring"});
        // ---- Faction
        Q("f_guard", "Le serment des Gardiens", QuestType::Faction, "aubert", "Le capitaine Aubert recrute pour les Gardiens de Greenhaven.", "Les Gardiens ont besoin de toi. Nettoie la Grotte des Murmures et chasse six gobelins.", "Les gobelins sont au sud.", "Bienvenue chez les Gardiens. Porte ces couleurs avec honneur.",
          {{"Tuer des gobelins", K::Kill, "goblin", 6, ""}}, 300, 100, {"chain_chest", "dye_royal"}, FA_GREENHAVEN, 40, "m2");
    });
    return q;
}
int questIndex(const std::string& id) { auto& q = quests(); for (int i = 0; i < (int)q.size(); i++) if (q[i].id == id) return i; return -1; }

const std::vector<MountDef>& mounts() {
    static std::vector<MountDef> m;
    static std::once_flag once;
    std::call_once(once, [] {
        CreatureLook horse; horse.kind = 3; horse.c1 = hexColor(0x6a4a30); horse.c2 = hexColor(0x8a6a4a); horse.saddle = true; horse.girth = 1.1f; horse.mane = false;
        m.push_back({"horse", "Cheval de Brindlemere", "Robuste et fidèle.", horse, 2.0f, 9.f, 12.5f, 120, 600});
        CreatureLook wolf; wolf.kind = 6; wolf.c1 = hexColor(0xb0b4bc); wolf.c2 = hexColor(0xe8ecf0); wolf.mane = true; wolf.saddle = true; wolf.girth = 1.2f; wolf.glow = hexColor(0x80d0ff);
        m.push_back({"wolf", "Fenra, loup géant", "Agile et infatigable dans la neige.", wolf, 2.4f, 10.f, 13.5f, 150, 0});
        CreatureLook elk; elk.kind = 7; elk.c1 = hexColor(0x5a4a3a); elk.c2 = hexColor(0xd8d0c0); elk.horns = true; elk.saddle = true; elk.crystal = true; elk.glow = hexColor(0x8ffff0);
        m.push_back({"elk", "Cerf sylvain", "Le plus élégant des coursiers.", elk, 2.1f, 10.5f, 14.f, 130, 900});
        CreatureLook boar; boar.kind = 1; boar.c1 = hexColor(0x4a3a30); boar.c2 = hexColor(0x6a5040); boar.c3 = hexColor(0x2a2018); boar.tusks = true; boar.armored = true; boar.saddle = true; boar.girth = 1.3f;
        m.push_back({"boar", "Sanglier de guerre", "Lent à démarrer, impossible à arrêter.", boar, 2.0f, 8.5f, 12.f, 200, 800});
        CreatureLook sp; sp.kind = 3; sp.c1 = hexColor(0x2a2238); sp.c2 = hexColor(0x6a50a0); sp.saddle = true; sp.crystal = true; sp.glow = hexColor(0xc070ff); sp.armored = true;
        m.push_back({"spectral", "Destrier spectral", "Le destrier de Sire Corwin. Il ne touche presque pas le sol.", sp, 2.1f, 11.5f, 15.5f, 180, 0});
    });
    return m;
}

const std::vector<DungeonDef>& dungeons() {
    static std::vector<DungeonDef> d = {
        {"whisper_cave", "Grotte des Murmures", "whisper_cave", R_GREENHAVEN, 101, 6, {"goblin", "goblin", "goblin_shaman"}, -1, 0, "echo_stone"},
        {"catacombs", "Catacombes des Rois", "graveyard", R_SHADOWS, 202, 8, {"skeleton", "skeleton_archer", "ghoul"}, 1, 1, ""},
        {"spider_den", "Antre de la Reine", "spider_den", R_SHADOWS, 303, 7, {"giant_spider", "forest_spider"}, 4, 2, ""},
        {"ember_mines", "Mines de Brasier", "ember_mines", R_ASHEN, 404, 8, {"ember_elemental", "salamander", "ash_knight"}, -1, 3, "miners_badge"},
        {"ice_caves", "Cavernes de Hrimgard", "ice_caves", R_FROSTHEIM, 505, 7, {"draugr", "frost_elemental", "snow_wolf"}, -1, 4, "relic_jarl_horn"},
        {"abyss", "L'Abîme", "abyss", R_SHADOWS, 606, 9, {"specter", "dark_knight", "ghoul"}, 7, 5, ""},
    };
    return d;
}

const std::vector<LoreDef>& lore() {
    // 1.7.0: the Chronicles. Ids l1..l6 are kept (older saves); new pages are unlocked by the prologue, relics,
    // discovering regions (r_*) and defeating the Guardians (mem_*: memories of Aldric).
    static std::vector<LoreDef> l = {
        // ---- the legend
        {"l1", "La Couronne-Mère", "Six joyaux, six royaumes, un seul roi. Aldric fit forger la Couronne-Mère au cœur de Highspire, avec le feu de l'Ashen et la glace de Frostheim, pour que plus jamais les royaumes ne se fassent la guerre. On dit qu'elle chantait la nuit, et que celui qui la portait entendait battre le cœur de chaque sujet."},
        {"l2", "La Chute", "La nuit de la Chute, la terre s'ouvrit sous Morneval. Le ciel devint violet, les morts se levèrent dans les marais, et pendant sept jours le soleil ne réchauffa plus personne. Ce qui sortit de la faille n'avait pas de nom. Seulement une faim."},
        {"l3", "Le sacrifice", "Pour refermer la faille, Aldric brisa la Couronne-Mère sur l'enclume de Highspire. Chaque fragment devint un verrou ; chaque Gardien, une clé vivante. Puis le roi entra dans l'Abîme, seul, et lia sa propre âme au sceau. On ne retrouva jamais son corps."},
        {"l4", "Sire Corwin", "Le champion du roi jura de garder la porte de l'Abîme « jusqu'au retour du roi ». Trois siècles plus tard, il attend encore, dans une armure que la rouille n'ose plus toucher. Ceux qui l'ont croisé disent qu'il ne parle qu'à une seule personne : quelqu'un qui n'est jamais revenu."},
        {"l5", "La reine Elsa", "La reine disparut le jour de la Chute. Certains disent qu'elle s'est enfuie par la mer, portant l'héritier du trône. Les bardes chantent encore le retour de « l'enfant de la couronne ». Les érudits de Highspire, eux, ne chantent pas : ils cherchent."},
        {"l6", "Les Errants", "Les marins parlent de naufragés marqués d'une couronne, que la mer rend toujours à Greenhaven. Aucun ne se souvient de son nom. La plupart meurent dans l'année. Un seul, dit-on, a survécu assez longtemps pour poser la question qui fait trembler l'Ordre : « Pourquoi la marque me brûle-t-elle quand je regarde vers Morneval ? »"},
        {"l7", "Malgrath", "Il était le plus brillant des mages d'Aldric, son élève, presque son frère. Il lisait les étoiles et comptait les années qui lui restaient. Un soir, il compta trop bas. Alors il chercha sous la terre ce que le ciel lui refusait, et trouva une voix qui promettait l'éternité. Il ne demanda pas le prix."},
        {"l8", "L'Abîme", "Ce n'est pas un lieu, disent les moines : c'est une blessure. Ce qui en monte ressemble à de l'ombre, mais a faim comme une bête. Les créatures d'ombre ne saignent pas ; elles se défont. Et dans leur silence, ceux qui écoutent trop longtemps entendent des promesses."},
        {"l9", "Les six Gardiens", "L'Esprit des Racines dans l'Elderwood. Le Sénéchal, devenu Roi Squelette, à Gravemoor. Le Colosse sous le Lac de Givre. Le Dragon Cendré au Nid de Cendres. La Reine des Araignées dans son antre. Le Titan de Pierre au Cirque des Titans. Six gardiens, six fragments. Trois siècles de murmures ont suffi à les corrompre tous."},
        {"l10", "L'Ordre de Highspire", "Les moines de Highspire gardent les archives du royaume et la clé de la forge où fut faite la Couronne-Mère. Ils portent le bleu et l'argent, et ne vieillissent pas plus vite que les autres hommes. Presque tous."},
        {"l11", "La marque-couronne", "Une couronne à six branches, sous la peau du poignet, qui brûle quand un fragment est proche. Les vieux grimoires l'appellent « le sceau du porteur ». Aucun grimoire n'explique comment un naufragé sans nom peut porter le sceau d'un roi mort."},
        // ---- the regions (unlocked on discovery)
        {"r_0", "Greenhaven", "Le cœur doux du royaume : blé, moulins, routes pavées. Brindlemere y garde la tapisserie de la Couronne, que la doyenne Maëlle refuse de montrer aux étrangers. Depuis quelques lunes, les loups sortent des bois la nuit, et ils ont les yeux violets."},
        {"r_1", "Frostheim", "Les clans du nord n'ont jamais plié le genou que devant Aldric. Ils racontent que le Colosse dort sous le Lac de Givre et que la glace chante quand il rêve. Cet hiver, la glace hurle."},
        {"r_2", "Ashen Lands", "Une terre de cendre au pied du volcan, où les mineurs de Cinderwatch arrachent le fer à la lave. Le Dragon Cendré y couve un fragment de couronne comme un œuf, et la montagne fume plus fort chaque année."},
        {"r_3", "Elderwood", "La forêt la plus ancienne du monde. Les druides du Cercle disent qu'Aldric y planta l'arbre-mère avec son champion, le jour de son sacre. Les racines remuent la nuit, et les clairières ne guérissent plus."},
        {"r_4", "Royaume des Ombres", "Ce fut le plus riche des six royaumes ; c'est aujourd'hui un marais de brume, de tombes et de cloches qui sonnent seules. Le château de Morneval domine la faille. Personne n'y entre. Quelque chose en sort."},
        {"r_5", "High Crowns", "Les cimes couronnées, où le ciel est si proche qu'on y voit les étoiles en plein jour. Le monastère de Highspire y garde la forge sacrée, et l'astronome de la tour prétend que les étoiles se sont déplacées."},
        // ---- memories of Aldric (one per defeated Guardian)
        {"mem_root", "Souvenir : l'arbre-mère", "Tu te souviens de la terre noire sous tes ongles. D'un jeune chevalier qui riait en tenant la pousse : « Elle verra nos petits-enfants, sire. » Corwin. Tu connais ce nom. Tu as planté cet arbre. Tu."},
        {"mem_king", "Souvenir : le serment du sénéchal", "Un homme agenouillé dans la salle du trône, la main sur l'épée. « Ma vie pour la couronne, ma mort pour le roi. » Le Roi Squelette a gardé son serment trois siècles. Avant de s'effondrer, il t'a appelé « sire »."},
        {"mem_ice", "Souvenir : l'hiver noir", "La neige rouge, les clans du nord en ligne, et l'armée d'ombre qui avance sans bruit. Tu as levé ton épée et des milliers d'hommes ont crié ton nom. Tu ne te souviens pas de ce nom. Mais tu te souviens du cri."},
        {"mem_ash", "Souvenir : l'élève", "Un jeune homme penché sur une carte du ciel. « Maître, combien de temps vivent les étoiles ? » Tu lui as répondu, et tu as vu la peur entrer dans ses yeux. C'était Malgrath. C'est toi qui lui as appris à compter les années."},
        {"mem_spider", "Souvenir : le jeune moine", "La forge de Highspire, la nuit où la Couronne fut brisée. Dans l'ombre, un novice qui tenait les tenailles : cheveux noirs, regard vif, une cicatrice au menton. Tu as déjà vu ce visage. Hier. Il n'a pas pris une ride."},
        {"mem_titan", "Souvenir : la clé de la forge", "Tu as confié la clé de la forge à l'Ordre en disant : « Que personne ne puisse jamais la reforger. » Le prieur a juré. Derrière lui, le novice à la cicatrice souriait."},
        {"mem_knight", "Souvenir : le champion", "Corwin, à genoux devant la porte de l'Abîme, et toi qui entres sans te retourner. « Attends-moi. » C'est le dernier mot que tu lui as dit. Il a attendu. Il attend encore. Ou il t'a enfin reconnu."},
        {"mem_abyss", "Souvenir : la voix", "Au fond de l'Abîme, une voix qui connaît ton vrai nom. « Tu es revenu, Aldric. Mais pas pour sceller. Pour choisir. » Tu sais maintenant qui tu es. Il te reste à décider ce que tu feras de la couronne."},
    };
    return l;
}

// ---------------------------------------------------------------------------------------- mods (1.7.0)
// The tables are built once into static non-const storage; mods patch them at startup, before any use.
ItemDef* itemMutable(const std::string& id) { return const_cast<ItemDef*>(item(id)); }
EnemyDef* enemyMutable(const std::string& id) { int i = enemyIndex(id); return i < 0 ? nullptr : const_cast<EnemyDef*>(&enemies()[i]); }
NpcDef* npcMutable(const std::string& id) { for (auto& n : npcs()) if (n.id == id) return const_cast<NpcDef*>(&n); return nullptr; }
std::vector<LoreDef>& loreMutable() { return const_cast<std::vector<LoreDef>&>(lore()); }

static std::string trimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

bool applyModDataLine(const std::string& raw, std::string& err) {
    std::string line = trimStr(raw);
    if (line.empty() || line[0] == '#') return true;
    size_t eq = line.find('=');
    if (eq == std::string::npos) { err = "il manque « = »"; return false; }
    std::string key = trimStr(line.substr(0, eq)), val = trimStr(line.substr(eq + 1));
    if (val.size() > 2000) { err = "texte trop long"; return false; }
    auto num = [&](float& out, float lo, float hi) {
        char* end = nullptr;
        float v = std::strtof(val.c_str(), &end);
        if (end == val.c_str() || !std::isfinite(v)) { err = "nombre attendu : " + val; return false; }
        out = clampf(v, lo, hi);
        return true;
    };
    if (key == "lore.add") {   // lore.add = id | Titre | Texte
        size_t p1 = val.find('|'), p2 = p1 == std::string::npos ? p1 : val.find('|', p1 + 1);
        if (p2 == std::string::npos) { err = "format : lore.add = id | Titre | Texte"; return false; }
        LoreDef d{trimStr(val.substr(0, p1)), trimStr(val.substr(p1 + 1, p2 - p1 - 1)), trimStr(val.substr(p2 + 1))};
        if (d.id.empty() || d.title.empty()) { err = "id ou titre vide"; return false; }
        auto& L = loreMutable();
        for (auto& l : L) if (l.id == d.id) { l = d; return true; }
        L.push_back(d);
        return true;
    }
    size_t d1 = key.find('.'), d2 = d1 == std::string::npos ? d1 : key.find('.', d1 + 1);
    if (d2 == std::string::npos) { err = "clé attendue : type.id.champ"; return false; }
    std::string type = key.substr(0, d1), id = key.substr(d1 + 1, d2 - d1 - 1), field = key.substr(d2 + 1);
    if (type == "item") {
        ItemDef* it = itemMutable(id);
        if (!it) { err = "objet inconnu : " + id; return false; }
        if (field == "name") { it->name = val; return true; }
        if (field == "desc") { it->desc = val; return true; }
        if (field == "atk") return num(it->atk, 0, 9999);
        if (field == "def") return num(it->def, 0, 9999);
        if (field == "hp") return num(it->hp, 0, 99999);
        if (field == "mp") return num(it->mp, 0, 99999);
        if (field == "crit") return num(it->crit, 0, 1);
        if (field == "speed") return num(it->speed, -0.5f, 1.f);
        if (field == "value") { float v; if (!num(v, 0, 1e7f)) return false; it->value = (int)v; return true; }
        err = "champ d'objet inconnu : " + field; return false;
    }
    if (type == "enemy") {
        EnemyDef* e = enemyMutable(id);
        if (!e) { err = "ennemi inconnu : " + id; return false; }
        if (field == "name") { e->name = val; return true; }
        if (field == "desc") { e->desc = val; return true; }
        if (field == "hp") return num(e->hp, 1, 1e6f);
        if (field == "atk") return num(e->atk, 0, 9999);
        if (field == "def") return num(e->def, 0, 9999);
        if (field == "speed") return num(e->speed, 0.2f, 20.f);
        if (field == "scale") return num(e->scale, 0.3f, 6.f);
        if (field == "aggro") return num(e->aggro, 1, 80);
        err = "champ d'ennemi inconnu : " + field; return false;
    }
    if (type == "npc") {
        NpcDef* n = npcMutable(id);
        if (!n) { err = "PNJ inconnu : " + id; return false; }
        if (field == "name") { n->name = val; return true; }
        if (field == "title") { n->title = val; return true; }
        if (field == "greet") { n->greetings.push_back(val); return true; }
        if (field == "greet.replace") { n->greetings = {val}; return true; }
        if (field == "lore") { n->lore.push_back(val); return true; }
        err = "champ de PNJ inconnu : " + field; return false;
    }
    err = "type inconnu : " + type + " (item, enemy, npc, lore.add)";
    return false;
}

}  // namespace lk
