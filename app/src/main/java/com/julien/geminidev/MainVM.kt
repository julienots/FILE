package com.julien.geminidev

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray

enum class Screen { SETUP, REPOS, CHAT }

class MainVM(app: Application) : AndroidViewModel(app) {

    val store = Store(app)

    var screen by mutableStateOf(
        if (store.ghToken.isBlank() || store.geminiKey.isBlank()) Screen.SETUP else Screen.REPOS
    )
        private set

    var login by mutableStateOf("")
        private set
    var repos by mutableStateOf<List<Repo>>(emptyList())
        private set
    var loadingRepos by mutableStateOf(false)
        private set
    var busy by mutableStateOf(false)
        private set
    var message by mutableStateOf<String?>(null)
        private set
    var current by mutableStateOf<Repo?>(null)
        private set
    var running by mutableStateOf(false)
        private set

    val lines = mutableStateListOf<Line>()

    private var history = JSONArray()
    private var ws: Workspace? = null
    private var job: Job? = null
    private var refreshJob: Job? = null

    init {
        if (screen == Screen.REPOS) loadRepos()
    }

    // ---------- Réglages ----------

    fun openSettings() {
        message = null
        screen = Screen.SETUP
    }

    fun closeSettings() {
        message = null
        if (store.ghToken.isNotBlank() && store.geminiKey.isNotBlank()) screen = Screen.REPOS
    }

    fun saveSetup(gh: String, key: String, model: String) {
        if (gh.isBlank() || key.isBlank()) {
            message = "Colle le token GitHub ET la clé Gemini."
            return
        }
        viewModelScope.launch {
            busy = true
            message = null
            try {
                val name = withContext(Dispatchers.IO) { GitHub(gh.trim()).user() }
                store.ghToken = gh
                store.geminiKey = key
                store.model = model
                login = name
                screen = Screen.REPOS
                loadRepos()
            } catch (e: Exception) {
                message = Agent.friendly(e)
            }
            busy = false
        }
    }

    // ---------- Dépôts ----------

    fun loadRepos() {
        viewModelScope.launch {
            loadingRepos = true
            message = null
            try {
                val gh = GitHub(store.ghToken)
                val (name, list) = withContext(Dispatchers.IO) { gh.user() to gh.repos() }
                login = name
                repos = list
            } catch (e: Exception) {
                message = Agent.friendly(e)
            }
            loadingRepos = false
        }
    }

    fun createRepo(name: String, isPrivate: Boolean) {
        viewModelScope.launch {
            busy = true
            message = null
            try {
                val gh = GitHub(store.ghToken)
                val r = withContext(Dispatchers.IO) { gh.createRepo(name, isPrivate) }
                repos = listOf(r) + repos
                openRepo(r)
            } catch (e: Exception) {
                message = Agent.friendly(e)
            }
            busy = false
        }
    }

    fun openRepo(r: Repo) {
        job?.cancel()
        running = false
        current = r
        lines.clear()
        history = JSONArray()
        val w = Workspace(GitHub(store.ghToken), r.fullName, r.defaultBranch)
        ws = w
        screen = Screen.CHAT
        refreshJob = viewModelScope.launch {
            try {
                val n = withContext(Dispatchers.IO) { w.refresh() }
                lines.add(Line(Kind.INFO, "${r.fullName} (${r.defaultBranch}) — $n fichier(s)"))
            } catch (e: Exception) {
                lines.add(Line(Kind.ERROR, Agent.friendly(e)))
            }
        }
    }

    fun back() {
        if (screen == Screen.CHAT) {
            job?.cancel()
            running = false
            screen = Screen.REPOS
        }
    }

    // ---------- Chat ----------

    fun send(text: String) {
        val t = text.trim()
        val w = ws ?: return
        if (t.isEmpty() || running) return
        lines.add(Line(Kind.USER, t))
        running = true
        job = viewModelScope.launch {
            try {
                refreshJob?.join()
                val gem = Gemini(store.geminiKey, store.model)
                Agent.run(t, history, w, gem) { l ->
                    viewModelScope.launch(Dispatchers.Main.immediate) { lines.add(l) }
                }
            } catch (e: CancellationException) {
                lines.add(Line(Kind.INFO, "Arrêté."))
            } catch (e: Exception) {
                lines.add(Line(Kind.ERROR, Agent.friendly(e)))
            } finally {
                running = false
            }
        }
    }

    fun stop() {
        job?.cancel()
    }
}
