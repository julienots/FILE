package com.superessence.squad.model

import org.json.JSONObject

enum class HairStyle(val label: String) {
    BALD("Crâne rasé"), BUZZ("Boule à zéro"), CREW("Coupe militaire"), MOHAWK("Iroquoise"),
    SLICK("Plaqués"), LONG("Longs"), BUN("Chignon"), CURLY("Bouclés")
}

enum class Beard(val label: String) {
    NONE("Rasé"), STUBBLE("Barbe de 3 jours"), FULL("Barbe pleine"), GOATEE("Bouc"),
    VIKING("Barbe tressée"), MUSTACHE("Moustache"), CHEVRON("Grosse moustache")
}

enum class Outfit(val label: String) {
    TANK("Débardeur"), LEATHER("Cuir"), SUIT("Costard"), CAMO("Treillis"), GI("Kimono"),
    FLANNEL("Chemise à carreaux"), FUR("Fourrure"), APRON("Tablier"), HOODIE("Sweat à capuche"),
    BOMBER("Blouson aviateur"), BARE("Torse nu"), VEST("Gilet")
}

enum class Accessory(val label: String) {
    NONE("Aucun"), SHADES("Lunettes noires"), AVIATORS("Aviateur"), HEADBAND("Bandeau"),
    CAP("Casquette"), HORNS("Casque viking"), BEANIE("Bonnet"), BANDANA("Bandana"),
    BERET("Béret"), HEADPHONES("Casque audio"), COWBOY("Chapeau cowboy"), TOQUE("Toque")
}

object Palette {
    val skins = intArrayOf(0xFFF3CFB3.toInt(), 0xFFE8B48F.toInt(), 0xFFD09A6E.toInt(), 0xFFB07850.toInt(), 0xFF8A5A3B.toInt(), 0xFF5E3B26.toInt())
    val hairs = intArrayOf(0xFF1A1412.toInt(), 0xFF3B2418.toInt(), 0xFF6B4226.toInt(), 0xFFB5651D.toInt(), 0xFFD9B25F.toInt(), 0xFF9A9A9A.toInt(), 0xFFEDEDED.toInt())
    val clothes = intArrayOf(
        0xFF151515.toInt(), 0xFF2E2E2E.toInt(), 0xFFF2F2F2.toInt(), 0xFF8B1E1E.toInt(), 0xFFC0392B.toInt(),
        0xFF1F3A5F.toInt(), 0xFF2F6FB0.toInt(), 0xFF4B5320.toInt(), 0xFF6B7F3A.toInt(), 0xFF5C3A21.toInt(),
        0xFF8E5B2E.toInt(), 0xFFFFC928.toInt(), 0xFFE67E22.toInt(), 0xFF5B2C6F.toInt(),
    )
}

data class Look(
    val skin: Int,
    val hairColor: Int,
    val hair: HairStyle,
    val beard: Beard,
    val outfit: Outfit,
    val outfitColor: Int,
    val pantsColor: Int,
    val accessory: Accessory,
    val tattoos: Boolean = false,
    val scar: Boolean = false,
) {
    fun toJson(): JSONObject = JSONObject()
        .put("skin", skin).put("hairColor", hairColor).put("hair", hair.name).put("beard", beard.name)
        .put("outfit", outfit.name).put("outfitColor", outfitColor).put("pantsColor", pantsColor)
        .put("accessory", accessory.name).put("tattoos", tattoos).put("scar", scar)

    companion object {
        fun fromJson(o: JSONObject) = Look(
            skin = o.getInt("skin"), hairColor = o.getInt("hairColor"),
            hair = enumOr(o.optString("hair"), HairStyle.CREW), beard = enumOr(o.optString("beard"), Beard.NONE),
            outfit = enumOr(o.optString("outfit"), Outfit.TANK), outfitColor = o.getInt("outfitColor"),
            pantsColor = o.getInt("pantsColor"), accessory = enumOr(o.optString("accessory"), Accessory.NONE),
            tattoos = o.optBoolean("tattoos"), scar = o.optBoolean("scar"),
        )
    }
}

inline fun <reified E : Enum<E>> enumOr(name: String?, default: E): E =
    enumValues<E>().firstOrNull { it.name == name } ?: default
