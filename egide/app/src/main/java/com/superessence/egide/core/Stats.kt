package com.superessence.egide.core

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

enum class Origin { SYSTEM, BROWSER }

data class LogEntry(val time: Long, val host: String, val blocked: Boolean, val origin: Origin, val note: String? = null)

data class Snapshot(
    val queries: Long = 0,
    val blocked: Long = 0,
    val redirects: Long = 0,
    val recent: List<LogEntry> = emptyList(),
)

/** Compteurs en direct, partagés entre le service VPN, le navigateur et l'interface. */
object Stats {
    private const val MAX_LOG = 300
    private val log = ArrayDeque<LogEntry>()
    private var queries = 0L
    private var blocked = 0L
    private var redirects = 0L
    private var lastPublish = 0L
    private var unsaved = 0

    private val _state = MutableStateFlow(Snapshot())
    val state: StateFlow<Snapshot> = _state

    @Synchronized
    fun record(host: String, wasBlocked: Boolean, origin: Origin, note: String? = null) {
        queries++
        if (wasBlocked) {
            blocked++
            if (++unsaved >= 20) flush()
        }
        if (log.size >= MAX_LOG) log.removeFirst()
        log.addLast(LogEntry(System.currentTimeMillis(), host, wasBlocked, origin, note))
        publish(force = false)
    }

    @Synchronized
    fun redirectBlocked(host: String) {
        redirects++
        blocked++
        if (log.size >= MAX_LOG) log.removeFirst()
        log.addLast(LogEntry(System.currentTimeMillis(), host, true, Origin.BROWSER, "redirection / pop-up"))
        publish(force = true)
    }

    @Synchronized
    fun flush() {
        if (unsaved > 0) {
            Prefs.totalBlocked = Prefs.totalBlocked + unsaved
            unsaved = 0
        }
    }

    val lifetimeBlocked: Long get() = Prefs.totalBlocked + unsaved

    private fun publish(force: Boolean) {
        val now = System.currentTimeMillis()
        if (!force && now - lastPublish < 400) return
        lastPublish = now
        _state.value = Snapshot(queries, blocked, redirects, log.reversed())
    }

    @Synchronized
    fun publishNow() = publish(force = true)
}
