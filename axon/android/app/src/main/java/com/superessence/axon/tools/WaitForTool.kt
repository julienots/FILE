package com.superessence.axon.tools

import android.accessibilityservice.AccessibilityService
import android.view.accessibility.AccessibilityNodeInfo
import kotlinx.coroutines.delay
import kotlinx.coroutines.withTimeoutOrNull
import org.json.JSONArray
import org.json.JSONObject

/**
 * Outil MCP "wait_for" pour AXON.
 *
 * Attend côté téléphone qu'un des textes demandés apparaisse (ou disparaisse)
 * à l'écran, puis répond immédiatement avec la liste des éléments.
 * Claude n'a plus besoin de faire des captures en boucle : un seul appel
 * revient dès que c'est son tour (ex. bouton "FOLD" visible au poker).
 *
 * Paramètres JSON :
 *   texts      : ["FOLD", "CHECK"]  (au moins un ; correspondance partielle, insensible à la casse)
 *   mode       : "appear" (défaut) ou "gone"
 *   clickable  : true = ne compte que les éléments cliquables (défaut true)
 *   timeout_ms : délai max (défaut 30000, max 120000)
 *   poll_ms    : intervalle de vérification (défaut 120)
 *
 * Réponse : { matched: "FOLD" | null, timed_out: bool, waited_ms, elements: [...] }
 */
object WaitForTool {

    const val NAME = "wait_for"

    val SCHEMA: JSONObject = JSONObject(
        """
        {
          "name": "wait_for",
          "description": "Attend sur l'appareil qu'un des textes apparaisse (ou disparaisse) à l'écran puis renvoie aussitôt les éléments. Idéal pour attendre son tour dans un jeu sans faire de captures en boucle.",
          "inputSchema": {
            "type": "object",
            "properties": {
              "texts": { "type": "array", "items": { "type": "string" }, "minItems": 1 },
              "mode": { "type": "string", "enum": ["appear", "gone"] },
              "clickable": { "type": "boolean" },
              "timeout_ms": { "type": "number" },
              "poll_ms": { "type": "number" },
              "device": { "type": "string" }
            },
            "required": ["texts"]
          }
        }
        """.trimIndent()
    )

    /**
     * @param service       le service d'accessibilité d'AXON
     * @param args          arguments JSON reçus du MCP
     * @param dumpElements  fonction existante d'AXON qui produit la liste des éléments (get_ui_elements)
     */
    suspend fun run(
        service: AccessibilityService,
        args: JSONObject,
        dumpElements: () -> JSONArray
    ): JSONObject {
        val texts = args.optJSONArray("texts")?.let { arr ->
            (0 until arr.length()).map { arr.getString(it).trim() }.filter { it.isNotEmpty() }
        }.orEmpty()
        require(texts.isNotEmpty()) { "texts est requis" }

        val waitGone = args.optString("mode", "appear") == "gone"
        val onlyClickable = args.optBoolean("clickable", true)
        val timeout = args.optLong("timeout_ms", 30_000L).coerceIn(500L, 120_000L)
        val poll = args.optLong("poll_ms", 120L).coerceIn(50L, 2_000L)

        val start = System.currentTimeMillis()
        var matched: String? = null

        val done = withTimeoutOrNull(timeout) {
            while (true) {
                val found = findFirst(service, texts, onlyClickable)
                if (!waitGone && found != null) { matched = found; return@withTimeoutOrNull true }
                if (waitGone && found == null) return@withTimeoutOrNull true
                delay(poll)
            }
            @Suppress("UNREACHABLE_CODE") false
        } ?: false

        return JSONObject().apply {
            put("matched", matched ?: JSONObject.NULL)
            put("timed_out", !done)
            put("waited_ms", System.currentTimeMillis() - start)
            put("elements", dumpElements())
        }
    }

    /** Renvoie le premier texte demandé trouvé à l'écran, ou null. */
    private fun findFirst(
        service: AccessibilityService,
        texts: List<String>,
        onlyClickable: Boolean
    ): String? {
        // Parcourt toutes les fenêtres (les jeux en WebView y compris), pas seulement la fenêtre active.
        val roots = buildList {
            service.windows?.forEach { w -> w.root?.let { add(it) } }
            if (isEmpty()) service.rootInActiveWindow?.let { add(it) }
        }
        try {
            for (root in roots) {
                for (t in texts) {
                    val nodes = root.findAccessibilityNodeInfosByText(t) ?: continue
                    val hit = nodes.any { n ->
                        val ok = n.isVisibleToUser && (!onlyClickable || isClickableOrParent(n))
                        n.recycleCompat()
                        ok
                    }
                    if (hit) return t
                }
            }
        } finally {
            roots.forEach { it.recycleCompat() }
        }
        return null
    }

    /** Dans les WebView, le texte est souvent dans un enfant du bouton cliquable. */
    private fun isClickableOrParent(node: AccessibilityNodeInfo): Boolean {
        var cur: AccessibilityNodeInfo? = node
        var depth = 0
        while (cur != null && depth < 4) {
            if (cur.isClickable) return true
            cur = cur.parent
            depth++
        }
        return false
    }

    @Suppress("DEPRECATION")
    private fun AccessibilityNodeInfo.recycleCompat() {
        if (android.os.Build.VERSION.SDK_INT < 33) runCatching { recycle() }
    }
}
