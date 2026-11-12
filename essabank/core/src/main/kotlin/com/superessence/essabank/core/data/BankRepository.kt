package com.superessence.essabank.core.data

import com.superessence.essabank.core.engine.BankEngine
import com.superessence.essabank.core.engine.BankError
import com.superessence.essabank.core.engine.EngineResult
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.security.BackupCodec
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext

/**
 * Point d'entrée unique des opérations : exécute le moteur, enregistre le nouvel état
 * dans SQLite, puis publie l'état. Les opérations sont sérialisées par un verrou :
 * deux virements ne peuvent jamais s'entrelacer.
 *
 * Si l'écriture en base échoue, l'état publié reste l'ancien : l'opération n'a pas eu lieu.
 */
class BankRepository(
    private val db: BankDatabase,
    val engine: BankEngine,
    private val io: CoroutineDispatcher = Dispatchers.IO,
) {
    private val mutex = Mutex()
    private val _state = MutableStateFlow<BankState?>(null)
    val state: StateFlow<BankState?> = _state.asStateFlow()

    private val _loaded = MutableStateFlow(false)
    val loaded: StateFlow<Boolean> = _loaded.asStateFlow()

    val current: BankState? get() = _state.value

    suspend fun load(): BankState? = mutex.withLock {
        val s = withContext(io) { db.load() }
        _state.value = s
        _loaded.value = true
        s
    }

    suspend fun <T> execute(op: BankEngine.(BankState) -> EngineResult<T>): EngineResult<T> = mutex.withLock {
        val cur = _state.value ?: return@withLock EngineResult.Err(BankError.AccountNotFound)
        val result = engine.op(cur)
        if (result is EngineResult.Ok && result.state !== cur) {
            withContext(io) { db.save(cur, result.state) }
            _state.value = result.state
        }
        result
    }

    /** Remplace tout l'état (création de profil, démonstration, restauration). */
    suspend fun replaceAll(state: BankState) = mutex.withLock {
        withContext(io) { db.replaceAll(state) }
        _state.value = state
    }

    suspend fun wipe() = mutex.withLock {
        withContext(io) { db.wipe() }
        _state.value = null
    }

    /** Exporte l'état complet, chiffré avec [password]. */
    suspend fun exportBackup(password: String): ByteArray {
        val s = _state.value ?: error("Aucune donnée")
        return withContext(io) {
            BackupCodec.encrypt(BankJson.encodeToString(BankState.serializer(), s).toByteArray(Charsets.UTF_8), password)
        }
    }

    /** Restaure une sauvegarde chiffrée. Lève une exception si le mot de passe ou le fichier est invalide. */
    suspend fun importBackup(data: ByteArray, password: String): BankState {
        val restored = withContext(io) {
            val plain = BackupCodec.decrypt(data, password)
            BankJson.decodeFromString(BankState.serializer(), plain.toString(Charsets.UTF_8))
        }
        replaceAll(restored)
        return restored
    }
}
