package com.superessence.promzy.data

import android.util.Xml
import org.xmlpull.v1.XmlPullParser
import java.io.InputStream
import java.text.SimpleDateFormat
import java.util.Locale

/**
 * Lecteur RSS tolérant (format des flux Dealabs / Pepper, mais aussi RSS 2.0 et Atom génériques).
 * Les préfixes d'espace de noms (pepper:, media:, dc:…) sont ignorés : seul le nom local compte.
 */
object RssParser {

    fun parse(input: InputStream): List<Deal> {
        val p = Xml.newPullParser()
        p.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, false)
        p.setInput(input, null)
        val out = mutableListOf<Deal>()
        var item: MutableMap<String, String>? = null
        var event = p.eventType
        while (event != XmlPullParser.END_DOCUMENT) {
            when (event) {
                XmlPullParser.START_TAG -> {
                    val name = p.name.substringAfter(':').lowercase()
                    if (name == "item" || name == "entry") {
                        item = mutableMapOf()
                    } else if (item != null) {
                        // attributs utiles (pepper:merchant name/price, media:content url, enclosure url, link href)
                        for (i in 0 until p.attributeCount) {
                            val an = p.getAttributeName(i).substringAfter(':').lowercase()
                            val av = p.getAttributeValue(i).trim()
                            if (av.isEmpty()) continue
                            when {
                                name == "merchant" && an == "name" -> item.putIfAbsent("merchant", av)
                                name == "merchant" && an == "price" -> item.putIfAbsent("price", av)
                                (name == "content" || name == "thumbnail" || name == "enclosure") && an == "url" -> {
                                    val type = (0 until p.attributeCount).firstOrNull { p.getAttributeName(it).endsWith("type") || p.getAttributeName(it).endsWith("medium") }?.let { p.getAttributeValue(it) } ?: "image"
                                    if (type.startsWith("image")) item.putIfAbsent("image", av)
                                }
                                name == "link" && an == "href" -> item.putIfAbsent("link", av)
                                an == "price" -> item.putIfAbsent("price", av)
                                an == "temperature" -> item.putIfAbsent("temperature", av)
                            }
                        }
                        if (!p.isEmptyElementTag) {
                            val text = readText(p)
                            if (text.isNotBlank()) {
                                val key = when (name) {
                                    "encoded" -> "content"
                                    "summary" -> "description"
                                    "updated", "published", "date" -> "pubdate"
                                    else -> name
                                }
                                if (key == "category") item["category"] = listOfNotNull(item["category"], text.trim()).joinToString(" · ")
                                else item.putIfAbsent(key, text.trim())
                            }
                            event = p.next(); continue
                        }
                    }
                }
                XmlPullParser.END_TAG -> {
                    val name = p.name.substringAfter(':').lowercase()
                    if ((name == "item" || name == "entry") && item != null) {
                        item.toDeal()?.let(out::add)
                        item = null
                    }
                }
            }
            event = p.next()
        }
        return out
    }

    /** Lit le texte d'un élément simple ; si l'élément contient des enfants, il est ignoré proprement. */
    private fun readText(p: XmlPullParser): String {
        val sb = StringBuilder()
        var depth = 1
        while (depth > 0) {
            when (p.next()) {
                XmlPullParser.TEXT, XmlPullParser.CDSECT -> if (depth == 1) sb.append(p.text)
                XmlPullParser.START_TAG -> depth++
                XmlPullParser.END_TAG -> depth--
                XmlPullParser.END_DOCUMENT -> return sb.toString()
            }
        }
        return sb.toString()
    }

    private val priceRegex = Regex("""(\d{1,3}(?:[ .  ]\d{3})*(?:[.,]\d{1,2})?)\s?€|€\s?(\d+(?:[.,]\d{1,2})?)""")
    private val imgRegex = Regex("""<img[^>]+src=["']([^"']+)["']""", RegexOption.IGNORE_CASE)
    private val tempRegex = Regex("""(-?\d{1,3}(?:[ \u00A0\u202F]\d{3})+|-?\d{2,5})\s?°""")

    private fun Map<String, String>.toDeal(): Deal? {
        val title = this["title"]?.let(::stripHtml)?.trim().orEmpty()
        val link = (this["link"] ?: this["guid"])?.trim().orEmpty()
        if (title.isEmpty() || !link.startsWith("http")) return null
        val html = this["content"] ?: this["description"].orEmpty()
        val summary = stripHtml(this["description"] ?: html).take(400)
        val image = this["image"] ?: imgRegex.find(html)?.groupValues?.get(1)
        val prices = priceRegex.findAll(title).map { it.value.replace(" ", "").replace(" ", "") }.toList()
        val price = this["price"]?.let { formatPrice(it) } ?: prices.firstOrNull()
        val oldPrice = prices.getOrNull(1)?.takeIf { it != price }
        val temp = this["temperature"]?.toDoubleOrNull()?.toInt()
            ?: tempRegex.find(summary)?.groupValues?.get(1)?.filter { it.isDigit() || it == '-' }?.toIntOrNull()
        return Deal(
            id = (this["guid"] ?: link).trim(),
            title = title,
            link = link,
            image = image?.replace("&amp;", "&"),
            price = price,
            oldPrice = oldPrice,
            merchant = this["merchant"],
            publishedAt = parseDate(this["pubdate"]),
            category = this["category"],
            summary = summary,
            temperature = temp,
        )
    }

    private fun formatPrice(raw: String): String {
        val t = raw.trim()
        if (t.contains('€') || t.isEmpty()) return t
        val n = t.replace(',', '.').toDoubleOrNull() ?: return t
        if (n == 0.0) return "Gratuit"
        return String.format(Locale.FRANCE, if (n % 1.0 == 0.0) "%.0f€" else "%.2f€", n)
    }

    fun stripHtml(s: String): String =
        s.replace(Regex("<br\\s*/?>", RegexOption.IGNORE_CASE), " ")
            .replace(Regex("<[^>]+>"), "")
            .replace("&nbsp;", " ").replace("&amp;", "&").replace("&quot;", "\"")
            .replace("&#039;", "'").replace("&apos;", "'").replace("&lt;", "<").replace("&gt;", ">")
            .replace("&euro;", "€")
            .replace(Regex("\\s+"), " ").trim()

    private val dateFormats = listOf(
        "EEE, dd MMM yyyy HH:mm:ss Z", "EEE, d MMM yyyy HH:mm:ss Z", "EEE, dd MMM yyyy HH:mm:ss zzz",
        "yyyy-MM-dd'T'HH:mm:ssXXX", "yyyy-MM-dd'T'HH:mm:ss.SSSXXX", "yyyy-MM-dd HH:mm:ss",
    )

    fun parseDate(s: String?): Long {
        if (s.isNullOrBlank()) return 0L
        for (f in dateFormats) {
            try { return SimpleDateFormat(f, Locale.US).parse(s.trim())!!.time } catch (_: Exception) {}
        }
        return 0L
    }
}
