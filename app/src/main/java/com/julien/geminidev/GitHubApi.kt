package com.julien.geminidev

import android.util.Base64
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.net.URLEncoder
import java.util.concurrent.TimeUnit

class GhException(val code: Int, message: String) : Exception(message)

data class Repo(
    val fullName: String,
    val defaultBranch: String,
    val isPrivate: Boolean,
    val description: String,
)

/** Client GitHub minimaliste (API REST) — appels bloquants, à lancer sur Dispatchers.IO. */
class GitHub(private val token: String) {

    private val http = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(60, TimeUnit.SECONDS)
        .writeTimeout(60, TimeUnit.SECONDS)
        .build()

    private val jsonType = "application/json; charset=utf-8".toMediaType()

    private fun req(
        method: String,
        path: String,
        body: JSONObject? = null,
        accept: String = "application/vnd.github+json",
    ): Pair<Int, String> {
        val url = if (path.startsWith("http")) path else "https://api.github.com$path"
        val b = Request.Builder()
            .url(url)
            .header("Authorization", "Bearer $token")
            .header("Accept", accept)
            .header("X-GitHub-Api-Version", "2022-11-28")
        val rb = body?.toString()?.toRequestBody(jsonType)
        when (method) {
            "GET" -> b.get()
            "POST" -> b.post(rb!!)
            "PUT" -> b.put(rb!!)
            "PATCH" -> b.patch(rb!!)
            "DELETE" -> if (rb != null) b.delete(rb) else b.delete()
        }
        return http.newCall(b.build()).execute().use { r ->
            r.code to (r.body?.string() ?: "")
        }
    }

    private fun errorMessage(r: Pair<Int, String>): String {
        val msg = try {
            JSONObject(r.second).optString("message", "")
        } catch (e: Exception) {
            ""
        }
        return when (r.first) {
            401 -> "Token GitHub invalide ou expiré."
            403 -> "GitHub a refusé l'accès (droits du token ou limite atteinte). $msg".trim()
            404 -> "Introuvable sur GitHub (le token a-t-il bien la case « repo » ?). $msg".trim()
            else -> "GitHub ${r.first} : ${msg.ifBlank { r.second.take(200) }}"
        }
    }

    private fun ok(r: Pair<Int, String>): String {
        if (r.first !in 200..299) throw GhException(r.first, errorMessage(r))
        return r.second
    }

    private fun enc(path: String): String =
        path.split("/").joinToString("/") { URLEncoder.encode(it, "UTF-8").replace("+", "%20") }

    fun user(): String = JSONObject(ok(req("GET", "/user"))).getString("login")

    private fun parseRepo(o: JSONObject) = Repo(
        fullName = o.getString("full_name"),
        defaultBranch = o.optString("default_branch", "main").ifBlank { "main" },
        isPrivate = o.optBoolean("private", false),
        description = if (o.isNull("description")) "" else o.optString("description", ""),
    )

    fun repos(): List<Repo> {
        val arr = JSONArray(
            ok(req("GET", "/user/repos?per_page=100&sort=updated&affiliation=owner,collaborator,organization_member"))
        )
        return (0 until arr.length()).map { parseRepo(arr.getJSONObject(it)) }
    }

    fun createRepo(name: String, isPrivate: Boolean): Repo {
        val body = JSONObject()
            .put("name", name)
            .put("private", isPrivate)
            .put("auto_init", true)
            .put("description", "Créé avec Gemini Dev")
        return parseRepo(JSONObject(ok(req("POST", "/user/repos", body))))
    }

    /** Liste des fichiers (blobs) de la branche. Vide si le dépôt est vide. */
    fun tree(repo: String, branch: String): List<String> {
        val r = req("GET", "/repos/$repo/git/trees/${enc(branch)}?recursive=1")
        if (r.first == 409 || r.first == 404) return emptyList()
        val arr = JSONObject(ok(r)).getJSONArray("tree")
        val out = ArrayList<String>()
        for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            if (o.optString("type") == "blob") out.add(o.getString("path"))
        }
        return out
    }

    /** Contenu texte d'un fichier, ou null s'il n'existe pas. */
    fun readFile(repo: String, path: String, branch: String): String? {
        val r = req(
            "GET",
            "/repos/$repo/contents/${enc(path)}?ref=${URLEncoder.encode(branch, "UTF-8")}",
            accept = "application/vnd.github.raw+json",
        )
        if (r.first == 404) return null
        return ok(r)
    }

    /**
     * Crée UN commit contenant toutes les modifications, puis pousse sur la branche.
     * changes : chemin -> nouveau contenu (null = suppression). Retourne le SHA du commit.
     */
    fun commit(repo: String, branch: String, message: String, changes: Map<String, String?>): String {
        val ref = req("GET", "/repos/$repo/git/ref/heads/${enc(branch)}")

        // Dépôt vide : la branche n'existe pas encore, on crée les fichiers un par un.
        if (ref.first == 404 || ref.first == 409) {
            var last = ""
            for ((p, c) in changes) {
                if (c == null) continue
                val body = JSONObject()
                    .put("message", message)
                    .put("content", Base64.encodeToString(c.toByteArray(Charsets.UTF_8), Base64.NO_WRAP))
                val res = ok(req("PUT", "/repos/$repo/contents/${enc(p)}", body))
                last = JSONObject(res).getJSONObject("commit").getString("sha")
            }
            return last
        }

        val headSha = JSONObject(ok(ref)).getJSONObject("object").getString("sha")
        val headCommit = JSONObject(ok(req("GET", "/repos/$repo/git/commits/$headSha")))
        val baseTree = headCommit.getJSONObject("tree").getString("sha")

        val entries = JSONArray()
        for ((p, c) in changes) {
            val e = JSONObject().put("path", p).put("mode", "100644").put("type", "blob")
            if (c == null) {
                e.put("sha", JSONObject.NULL)
            } else {
                val blob = JSONObject()
                    .put("content", c)
                    .put("encoding", "utf-8")
                val sha = JSONObject(ok(req("POST", "/repos/$repo/git/blobs", blob))).getString("sha")
                e.put("sha", sha)
            }
            entries.put(e)
        }

        val treeBody = JSONObject().put("base_tree", baseTree).put("tree", entries)
        val treeSha = JSONObject(ok(req("POST", "/repos/$repo/git/trees", treeBody))).getString("sha")

        val commitBody = JSONObject()
            .put("message", message)
            .put("tree", treeSha)
            .put("parents", JSONArray().put(headSha))
        val commitSha = JSONObject(ok(req("POST", "/repos/$repo/git/commits", commitBody))).getString("sha")

        ok(req("PATCH", "/repos/$repo/git/refs/heads/${enc(branch)}", JSONObject().put("sha", commitSha)))
        return commitSha
    }
}
