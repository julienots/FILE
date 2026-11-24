package com.julien.geminidev

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.util.TreeSet
import java.util.concurrent.atomic.AtomicLong

enum class Kind { USER, ASSISTANT, TOOL, INFO, ERROR }

private val nextId = AtomicLong()

data class Line(val kind: Kind, val text: String, val id: Long = nextId.incrementAndGet())

/**
 * Copie de travail du dépôt : les modifications sont préparées en mémoire
 * puis envoyées d'un coup dans UN commit (commit_changes).
 */
class Workspace(private val gh: GitHub, val repo: String, val branch: String) {
    private var remote: MutableSet<String> = TreeSet()
    private val paths: MutableSet<String> = TreeSet()
    private val staged = LinkedHashMap<String, String?>()

    fun refresh(): Int {
        remote = TreeSet(gh.tree(repo, branch))
        paths.clear()
        paths.addAll(remote)
        staged.clear()
        return remote.size
    }

    fun pending(): Int = staged.size

    fun list(): String {
        if (paths.isEmpty()) return "(dépôt vide)"
        val all = paths.toList()
        val shown = all.take(500).joinToString("\n") { p ->
            if (staged.containsKey(p)) "$p  [modifié, pas encore commité]" else p
        }
        return if (all.size > 500) shown + "\n... et ${all.size - 500} autres fichiers" else shown
    }

    private fun cut(c: String): String =
        if (c.length > 60_000) c.take(60_000) + "\n[... tronqué : ${c.length} caractères au total]" else c

    fun read(p: String): String {
        if (staged.containsKey(p)) {
            val c = staged[p] ?: error("fichier supprimé (pas encore commité) : $p")
            return cut(c)
        }
        val c = gh.readFile(repo, p, branch) ?: error("fichier introuvable : $p")
        return cut(c)
    }

    fun write(p: String, content: String): String {
        staged[p] = content
        paths.add(p)
        return "ok : $p écrit (${content.length} caractères), en attente de commit"
    }

    fun delete(p: String): String {
        if (!paths.contains(p)) error("fichier introuvable : $p")
        paths.remove(p)
        if (remote.contains(p)) staged[p] = null else staged.remove(p)
        return "ok : $p supprimé, en attente de commit"
    }

    fun commit(message: String): String {
        if (staged.isEmpty()) return "rien à commiter"
        val n = staged.size
        val sha = gh.commit(repo, branch, message, LinkedHashMap(staged))
        staged.clear()
        remote = TreeSet(paths)
        return "ok : commit ${sha.take(7)} poussé sur $branch ($n fichier(s))"
    }
}

object Agent {
    private const val MAX_STEPS = 60

    private val TOOLS: JSONArray = buildTools()

    private fun prop(desc: String) = JSONObject().put("type", "STRING").put("description", desc)

    private fun fn(
        name: String,
        desc: String,
        props: JSONObject = JSONObject(),
        required: List<String> = emptyList(),
    ): JSONObject {
        val f = JSONObject().put("name", name).put("description", desc)
        if (props.length() > 0) {
            val params = JSONObject().put("type", "OBJECT").put("properties", props)
            if (required.isNotEmpty()) params.put("required", JSONArray(required))
            f.put("parameters", params)
        }
        return f
    }

    private fun buildTools(): JSONArray {
        val decls = JSONArray()
            .put(fn("list_files", "Liste tous les fichiers du dépôt."))
            .put(
                fn(
                    "read_file", "Lit le contenu d'un fichier texte du dépôt.",
                    JSONObject().put("path", prop("Chemin du fichier, ex: src/index.html")),
                    listOf("path"),
                )
            )
            .put(
                fn(
                    "write_file",
                    "Crée ou remplace un fichier avec son contenu COMPLET. Pas encore envoyé sur GitHub tant que commit_changes n'est pas appelé.",
                    JSONObject()
                        .put("path", prop("Chemin du fichier, ex: index.html"))
                        .put("content", prop("Contenu complet du fichier")),
                    listOf("path", "content"),
                )
            )
            .put(
                fn(
                    "delete_file", "Supprime un fichier du dépôt.",
                    JSONObject().put("path", prop("Chemin du fichier à supprimer")),
                    listOf("path"),
                )
            )
            .put(
                fn(
                    "commit_changes",
                    "Crée un commit avec toutes les modifications en attente et le pousse sur GitHub.",
                    JSONObject().put("message", prop("Message de commit, court et clair")),
                    listOf("message"),
                )
            )
        return JSONArray().put(JSONObject().put("functionDeclarations", decls))
    }

    private fun systemPrompt(ws: Workspace) = """
Tu es Gemini Dev, un agent de développement autonome qui travaille directement dans le dépôt GitHub "${ws.repo}" (branche "${ws.branch}"), depuis un téléphone Android.
L'utilisateur est francophone et donne des ordres courts : exécute-les sans poser de questions, prends les décisions toi-même.

Méthode :
1. Commence par list_files, puis read_file sur les fichiers utiles avant de modifier quoi que ce soit.
2. Écris toujours des fichiers COMPLETS avec write_file (jamais de morceaux, jamais de "reste du code inchangé").
3. Par défaut, une app ou un jeu web = un seul fichier index.html autonome (HTML + CSS + JS), sauf demande contraire. Pour une app Android : projet Gradle complet en Kotlin.
4. Quand tout est écrit, appelle commit_changes avec un message clair. Tu peux commiter plusieurs fois.
5. Termine par un résumé court en français (2 à 4 phrases) de ce qui a été fait.

Tu as les outils pour lire, écrire, supprimer et commiter : ne dis jamais que tu ne peux pas accéder au dépôt.
""".trimIndent()

    private fun normPath(raw: String): String {
        var p = raw.trim().replace('\\', '/')
        while (p.startsWith("./")) p = p.removePrefix("./")
        p = p.trimStart('/')
        require(p.isNotEmpty() && p.split('/').none { it.isEmpty() || it == ".." }) { "chemin invalide : $raw" }
        return p
    }

    private fun describe(name: String, a: JSONObject): String = when (name) {
        "list_files" -> "ls"
        "read_file" -> "cat ${a.optString("path")}"
        "write_file" -> "write ${a.optString("path")} (${a.optString("content").length} car.)"
        "delete_file" -> "rm ${a.optString("path")}"
        "commit_changes" -> "git commit -m \"${a.optString("message")}\" && git push"
        else -> name
    }

    private fun execute(name: String, a: JSONObject, ws: Workspace): String = when (name) {
        "list_files" -> ws.list()
        "read_file" -> ws.read(normPath(a.optString("path")))
        "write_file" -> {
            require(a.has("content")) { "paramètre content manquant" }
            ws.write(normPath(a.optString("path")), a.optString("content"))
        }
        "delete_file" -> ws.delete(normPath(a.optString("path")))
        "commit_changes" -> ws.commit(a.optString("message").ifBlank { "Mise à jour par Gemini Dev" })
        else -> "ERREUR : outil inconnu $name"
    }

    private suspend fun generateWithRetry(
        gem: Gemini,
        history: JSONArray,
        system: String,
        emit: (Line) -> Unit,
    ): JSONObject {
        var attempt = 0
        while (true) {
            try {
                return gem.generate(history, system, TOOLS)
            } catch (e: GeminiException) {
                val retryable = e.code == 429 || e.code == 500 || e.code == 503
                if (!retryable || attempt >= 3) throw e
                attempt++
                val wait = 15 * attempt
                emit(Line(Kind.INFO, "Gemini est saturé ou la limite gratuite est atteinte, nouvel essai dans ${wait}s ($attempt/3)…"))
                delay(wait * 1000L)
            }
        }
    }

    /** Lance l'agent pour un message de l'utilisateur. `history` est la conversation Gemini (modifiée sur place). */
    suspend fun run(prompt: String, history: JSONArray, ws: Workspace, gem: Gemini, emit: (Line) -> Unit) {
        withContext(Dispatchers.IO) {
            val mark = history.length()
            try {
                history.put(
                    JSONObject().put("role", "user")
                        .put("parts", JSONArray().put(JSONObject().put("text", prompt)))
                )
                val system = systemPrompt(ws)
                var finished = false
                var steps = 0

                while (steps < MAX_STEPS) {
                    steps++
                    ensureActive()

                    val resp = generateWithRetry(gem, history, system, emit)
                    val cand = resp.optJSONArray("candidates")?.optJSONObject(0)
                    if (cand == null) {
                        val why = resp.optJSONObject("promptFeedback")?.optString("blockReason").orEmpty()
                        emit(Line(Kind.ERROR, "Gemini n'a pas répondu${if (why.isNotBlank()) " (bloqué : $why)" else ""}."))
                        finished = true
                        break
                    }
                    val content = cand.optJSONObject("content")
                    val parts = content?.optJSONArray("parts")
                    if (content == null || parts == null || parts.length() == 0) {
                        val reason = cand.optString("finishReason", "inconnue")
                        emit(Line(Kind.ERROR, "Réponse vide de Gemini (raison : $reason). Réessaie."))
                        finished = true
                        break
                    }
                    content.put("role", "model")
                    history.put(content)

                    val calls = ArrayList<JSONObject>()
                    val text = StringBuilder()
                    for (i in 0 until parts.length()) {
                        val part = parts.getJSONObject(i)
                        if (part.has("functionCall")) {
                            calls.add(part.getJSONObject("functionCall"))
                        } else if (part.has("text") && !part.optBoolean("thought", false)) {
                            text.append(part.getString("text"))
                        }
                    }
                    if (text.isNotBlank()) emit(Line(Kind.ASSISTANT, text.toString().trim()))

                    if (calls.isEmpty()) {
                        finished = true
                        break
                    }

                    val responses = JSONArray()
                    for (c in calls) {
                        val name = c.getString("name")
                        val args = c.optJSONObject("args") ?: JSONObject()
                        emit(Line(Kind.TOOL, describe(name, args)))
                        val result = try {
                            execute(name, args, ws)
                        } catch (e: CancellationException) {
                            throw e
                        } catch (e: Exception) {
                            "ERREUR : ${e.message ?: e.javaClass.simpleName}"
                        }
                        if (result.startsWith("ok : commit")) emit(Line(Kind.INFO, "✓ $result"))
                        responses.put(
                            JSONObject().put(
                                "functionResponse",
                                JSONObject().put("name", name)
                                    .put("response", JSONObject().put("result", result))
                            )
                        )
                    }
                    history.put(JSONObject().put("role", "user").put("parts", responses))
                }

                if (!finished) emit(Line(Kind.INFO, "Limite de $MAX_STEPS étapes atteinte. Écris « continue » pour reprendre."))

                if (ws.pending() > 0) {
                    emit(Line(Kind.TOOL, "git commit (automatique) && git push"))
                    val res = ws.commit("Modifications par Gemini Dev")
                    emit(Line(Kind.INFO, "✓ $res"))
                }
            } catch (e: Throwable) {
                // Garde l'historique cohérent (pas d'appel de fonction sans réponse).
                while (history.length() > mark) history.remove(history.length() - 1)
                throw e
            }
        }
    }

    /** Message d'erreur lisible pour l'utilisateur. */
    fun friendly(e: Throwable): String = when (e) {
        is GeminiException -> when {
            e.code == 400 && e.message.orEmpty().contains("API key", ignoreCase = true) ->
                "Clé Gemini invalide. Vérifie-la dans les réglages (⚙)."
            e.code == 400 -> "Gemini a refusé la requête : ${e.message}"
            e.code == 403 ->
                "Accès refusé par Gemini. Ce modèle n'est sans doute pas disponible avec une clé gratuite (Argon = API payante). Mets gemini-3.8-flash dans les réglages."
            e.code == 404 -> "Modèle introuvable. Vérifie son nom dans les réglages (⚙)."
            e.code == 429 -> "Limite gratuite de Gemini atteinte. Attends un peu (ou demain) puis réessaie."
            else -> "Erreur Gemini ${e.code} : ${e.message}"
        }
        is GhException -> e.message ?: "Erreur GitHub"
        is IOException -> "Pas de connexion : ${e.message}"
        else -> e.message ?: e.javaClass.simpleName
    }
}
