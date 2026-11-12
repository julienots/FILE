package com.superessence.essabank.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.superessence.essabank.EssaApp
import com.superessence.essabank.core.demo.DemoData
import com.superessence.essabank.core.engine.BankEngine
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.pdf.Reports
import com.superessence.essabank.core.model.Transaction
import com.superessence.essabank.data.PinCheck
import com.superessence.essabank.ui.i18n.L
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class UiMessage(val text: String, val error: Boolean = false, val id: Long = System.nanoTime())

/**
 * ViewModel unique de l'application : expose l'état bancaire virtuel et les actions.
 * Toutes les opérations passent par [com.superessence.essabank.core.data.BankRepository]
 * (moteur pur + enregistrement SQLite atomique).
 */
class AppViewModel(app: Application) : AndroidViewModel(app) {
    val container = (app as EssaApp).container
    private val repo = container.repository
    val engine: BankEngine get() = container.engine
    val analytics get() = container.analytics
    val market get() = container.market
    val security get() = container.security
    val zone get() = engine.clock.zone

    val state: StateFlow<BankState?> = repo.state
    val loaded: StateFlow<Boolean> = repo.loaded

    private val _locked = MutableStateFlow(true)
    val locked: StateFlow<Boolean> = _locked.asStateFlow()

    private val _message = MutableStateFlow<UiMessage?>(null)
    val message: StateFlow<UiMessage?> = _message.asStateFlow()

    /** Horloge d'affichage, rafraîchie toutes les 20 s (marché fictif « en direct »). */
    private val _now = MutableStateFlow(System.currentTimeMillis())
    val now: StateFlow<Long> = _now.asStateFlow()

    private var backgroundSince: Long = 0L

    val lang: AppLanguage get() = state.value?.profile?.preferences?.language ?: AppLanguage.FR

    init {
        viewModelScope.launch {
            repo.load()
            if (repo.current == null || !security.hasPin) _locked.value = false
            processDue()
        }
        viewModelScope.launch {
            while (isActive) { delay(20_000); _now.value = System.currentTimeMillis() }
        }
    }

    // ------------------------------------------------------------------ messages

    fun toast(text: String, error: Boolean = false) { _message.value = UiMessage(text, error) }
    fun consumeMessage(id: Long) { if (_message.value?.id == id) _message.value = null }

    // ------------------------------------------------------------------ opérations

    /** Exécute une opération du moteur ; affiche l'erreur éventuelle. */
    suspend fun <T> exec(op: BankEngine.(BankState) -> EngineResult<T>): EngineResult<T> {
        val r = try { repo.execute(op) } catch (e: Exception) {
            toast(tr("Enregistrement impossible : opération annulée.", "Could not save: operation cancelled."), true)
            return EngineResult.Err(com.superessence.essabank.core.engine.BankError.InvalidAmount)
        }
        if (r is EngineResult.Err) toast(L.error(r.error, lang), true)
        return r
    }

    /** Version « lancer et oublier » avec message de succès facultatif. */
    fun run(success: String? = null, onOk: (Any?) -> Unit = {}, op: BankEngine.(BankState) -> EngineResult<Any?>): Job =
        viewModelScope.launch {
            val r = exec(op)
            if (r is EngineResult.Ok) { success?.let { toast(it) }; onOk(r.value) }
        }

    fun processDue() = viewModelScope.launch {
        if (repo.current != null) repo.execute { processDue(it) }
    }

    private fun tr(fr: String, en: String) = if (lang == AppLanguage.FR) fr else en

    // ------------------------------------------------------------------ profil et démonstration

    fun startDemo(language: AppLanguage, onDone: () -> Unit) = viewModelScope.launch {
        val s = withContext(Dispatchers.Default) { DemoData.create(engine.clock, engine.ids, market, language) }
        repo.replaceAll(s)
        onDone()
    }

    fun createProfile(first: String, last: String, capital: Money, language: AppLanguage, onDone: () -> Unit) = viewModelScope.launch {
        repo.replaceAll(DemoData.blank(engine.clock, engine.ids, first, last, language, capital))
        onDone()
    }

    fun resetDemo(onDone: () -> Unit = {}) = viewModelScope.launch {
        val language = lang
        repo.replaceAll(withContext(Dispatchers.Default) { DemoData.create(engine.clock, engine.ids, market, language) })
        toast(tr("Données de démonstration réinitialisées.", "Demo data reset."))
        onDone()
    }

    /** Efface toutes les données locales (profil, base, PIN) et revient à l'accueil. */
    fun deleteEverything(onDone: () -> Unit) = viewModelScope.launch {
        repo.wipe()
        security.clear()
        security.introSeen = true
        _locked.value = false
        onDone()
    }

    // ------------------------------------------------------------------ sécurité

    fun setPin(pin: String) { security.setPin(pin); _locked.value = false }

    fun checkPin(pin: String): PinCheck {
        val r = security.check(pin)
        if (r is PinCheck.Ok) unlock()
        return r
    }

    fun unlock() {
        _locked.value = false
        processDue()
    }

    fun lockNow() { if (security.hasPin && repo.current != null) _locked.value = true }

    fun onBackground() { backgroundSince = System.currentTimeMillis() }

    fun onForeground() {
        _now.value = System.currentTimeMillis()
        if (backgroundSince > 0 && security.hasPin && repo.current != null) {
            val elapsed = (System.currentTimeMillis() - backgroundSince) / 1000
            if (elapsed >= security.autoLockSeconds) _locked.value = true
        }
        backgroundSince = 0
        processDue()
    }

    // ------------------------------------------------------------------ export

    fun statementPdf(transactions: List<Transaction>, period: String): ByteArray? =
        state.value?.let { Reports.statement(it, transactions, period, zone, System.currentTimeMillis()) }

    fun receiptPdf(tx: Transaction): ByteArray? = state.value?.let { Reports.receipt(it, tx, zone) }

    suspend fun exportBackup(password: String): ByteArray = repo.exportBackup(password)

    suspend fun importBackup(data: ByteArray, password: String): Boolean = try {
        repo.importBackup(data, password)
        toast(tr("Sauvegarde restaurée.", "Backup restored."))
        true
    } catch (e: com.superessence.essabank.core.security.BackupCodec.BadPasswordException) {
        toast(tr("Mot de passe incorrect ou fichier endommagé.", "Wrong password or damaged file."), true); false
    } catch (e: Exception) {
        toast(tr("Ce fichier n'est pas une sauvegarde ESSABANK valide.", "This file is not a valid ESSABANK backup."), true); false
    }
}
