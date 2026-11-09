package com.superessence.noctea.data

/** Bibliothèque des ambiances sonores embarquées (assets/sounds/*.ogg, boucles parfaites). */
data class AmbientSound(val id: String, val name: String, val group: String, val description: String)

object Sounds {
    const val GROUP_NATURE = "Nature"
    const val GROUP_NOISE = "Bruits colorés"
    const val GROUP_MUSIC = "Musique"

    val all = listOf(
        AmbientSound("rain_soft", "Pluie douce", GROUP_NATURE, "Une pluie fine et régulière"),
        AmbientSound("storm", "Orage lointain", GROUP_NATURE, "Pluie et grondements au loin"),
        AmbientSound("waves", "Vagues", GROUP_NATURE, "Le souffle lent de l'océan"),
        AmbientSound("stream", "Ruisseau", GROUP_NATURE, "Une eau claire qui murmure"),
        AmbientSound("wind_trees", "Vent dans les arbres", GROUP_NATURE, "Feuillages et rafales douces"),
        AmbientSound("wind_soft", "Vent doux", GROUP_NATURE, "Une brise légère et continue"),
        AmbientSound("fire", "Feu de cheminée", GROUP_NATURE, "Crépitements chaleureux"),
        AmbientSound("night", "Sons nocturnes", GROUP_NATURE, "Grillons et nuit d'été"),
        AmbientSound("white", "Bruit blanc", GROUP_NOISE, "Masque les bruits extérieurs"),
        AmbientSound("pink", "Bruit rose", GROUP_NOISE, "Plus doux, équilibré"),
        AmbientSound("brown", "Bruit brun", GROUP_NOISE, "Profond et enveloppant"),
        AmbientSound("ambient", "Musique ambient", GROUP_MUSIC, "Nappes harmoniques lentes"),
        AmbientSound("piano", "Piano relaxant", GROUP_MUSIC, "Notes rêveuses et espacées"),
        AmbientSound("pads", "Pads atmosphériques", GROUP_MUSIC, "Textures aériennes"),
        AmbientSound("space", "Univers spatial", GROUP_MUSIC, "Drones cosmiques profonds"),
    )

    private val byId = all.associateBy { it.id }
    fun get(id: String) = byId[id]
    fun assetUri(id: String) = "asset:///sounds/$id.ogg"
}
