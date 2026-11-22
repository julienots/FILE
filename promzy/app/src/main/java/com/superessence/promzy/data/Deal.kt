package com.superessence.promzy.data

import kotlinx.serialization.Serializable

@Serializable
data class Deal(
    val id: String,
    val title: String,
    val link: String,
    val image: String? = null,
    val price: String? = null,
    val oldPrice: String? = null,
    val merchant: String? = null,
    val publishedAt: Long = 0L,
    val category: String? = null,
    val summary: String = "",
    val temperature: Int? = null,
)

/** Catégories : filtrage par mots-clés sur le titre, la catégorie et le marchand. */
enum class Category(val label: String, val keywords: List<String>) {
    ALL("Tout", emptyList()),
    TECH("High-tech", listOf("smartphone", "iphone", "samsung", "xiaomi", "pc ", "ordinateur", "laptop", "tablette", "ipad", "ecran", "moniteur", "tv ", "televiseur", "oled", "qled", "casque", "ecouteur", "airpods", "ssd", "disque", "souris", "clavier", "montre", "watch", "chargeur", "enceinte", "macbook", "carte graphique", "rtx", "routeur", "high-tech", "informatique", "telephonie", "pixel")),
    GAMING("Jeux vidéo", listOf("ps5", "ps4", "playstation", "xbox", "switch", "nintendo", "steam", "jeu ", "jeux", "gaming", "manette", "epic games", "gog", "dualsense")),
    HOME("Maison", listOf("aspirateur", "robot", "cafetiere", "machine", "lave", "frigo", "refrigerateur", "four", "airfryer", "friteuse", "matelas", "canape", "lampe", "meuble", "cuisine", "jardin", "bricolage", "perceuse", "dyson", "seb", "tefal", "ikea", "maison", "electromenager", "outil")),
    FASHION("Mode", listOf("nike", "adidas", "puma", "chaussure", "basket", "sneakers", "veste", "jean", "t-shirt", "sweat", "pull", "manteau", "sac", "lunettes", "zalando", "mode", "vetement", "parfum", "beaute")),
    FOOD("Courses", listOf("carrefour", "leclerc", "auchan", "lidl", "intermarche", "monoprix", "courses", "epicerie", "cafe", "capsule", "biere", "vin", "chocolat", "lessive", "couches", "nutella", "coca", "alimentation", "boisson")),
    TRAVEL("Voyages", listOf("vol ", "vols", "avion", "billet", "train", "sncf", "ouigo", "hotel", "sejour", "voyage", "location", "croisiere", "airbnb", "booking")),
    FREE("Gratuit", listOf("gratuit", "offert", "free", "0€", "0,00€", "0.00€")),
    ;

    fun matches(d: Deal): Boolean {
        if (this == ALL) return true
        val hay = " " + normalize("${d.title} ${d.category ?: ""} ${d.merchant ?: ""} ${d.price ?: ""}") + " "
        if (this == FREE && d.price != null && normalize(d.price).let { it == "gratuit" || it.matches(Regex("0([,.]0+)? ?€?")) }) return true
        return keywords.any { hay.contains(it) }
    }
}

fun normalize(s: String): String =
    java.text.Normalizer.normalize(s.lowercase(), java.text.Normalizer.Form.NFD)
        .replace(Regex("\\p{Mn}+"), "")

/** Recherche locale tolérante : tous les mots doivent apparaître (accents ignorés). */
fun Deal.matchesQuery(q: String): Boolean {
    val words = normalize(q).split(Regex("\\s+")).filter { it.isNotBlank() }
    if (words.isEmpty()) return true
    val hay = normalize("$title ${merchant ?: ""} ${category ?: ""} $summary")
    return words.all { hay.contains(it) }
}
