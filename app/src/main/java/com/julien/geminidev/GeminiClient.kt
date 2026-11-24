package com.julien.geminidev

import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

class GeminiException(val code: Int, message: String) : Exception(message)

/** Client Gemini (REST generateContent) avec appel de fonctions — appel bloquant. */
class Gemini(private val key: String, model: String) {

    private val model = model.trim().removePrefix("models/")

    private val http = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(300, TimeUnit.SECONDS)
        .writeTimeout(60, TimeUnit.SECONDS)
        .build()

    private val jsonType = "application/json; charset=utf-8".toMediaType()

    fun generate(contents: JSONArray, system: String, tools: JSONArray): JSONObject {
        val body = JSONObject()
            .put(
                "systemInstruction",
                JSONObject().put("parts", JSONArray().put(JSONObject().put("text", system)))
            )
            .put("contents", contents)
            .put("tools", tools)

        val request = Request.Builder()
            .url("https://generativelanguage.googleapis.com/v1beta/models/$model:generateContent")
            .header("x-goog-api-key", key)
            .post(body.toString().toRequestBody(jsonType))
            .build()

        return http.newCall(request).execute().use { r ->
            val text = r.body?.string() ?: ""
            if (!r.isSuccessful) throw GeminiException(r.code, parseError(text))
            JSONObject(text)
        }
    }

    private fun parseError(s: String): String = try {
        JSONObject(s).getJSONObject("error").optString("message", s.take(300))
    } catch (e: Exception) {
        s.take(300)
    }
}
