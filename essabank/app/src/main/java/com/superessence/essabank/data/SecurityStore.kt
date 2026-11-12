package com.superessence.essabank.data

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import com.superessence.essabank.core.security.PinHasher
import com.superessence.essabank.core.security.PinLockout

/**
 * Réglages de sécurité locaux : PIN haché (PBKDF2), compteur d'échecs, biométrie,
 * délai de verrouillage automatique et protection d'écran. Le PIN n'est jamais stocké en clair.
 * Implémentation Android : [SecurityStore] (fichier exclu des sauvegardes, voir data_extraction_rules.xml).
 */
interface SecuritySettings {
    val hasPin: Boolean
    fun setPin(pin: String)
    fun check(pin: String, now: Long = System.currentTimeMillis()): PinCheck
    fun lockRemaining(now: Long = System.currentTimeMillis()): Long
    var biometricEnabled: Boolean
    var autoLockSeconds: Int
    var secureScreen: Boolean
    var introSeen: Boolean
    fun clear()
}

/**
 * Logique commune : PIN haché (PBKDF2), compteur d'échecs et délai croissant.
 * Le stockage clé-valeur est fourni par la sous-classe.
 */
abstract class BaseSecuritySettings : SecuritySettings {
    protected abstract fun getString(key: String): String?
    protected abstract fun getInt(key: String, def: Int): Int
    protected abstract fun getLong(key: String, def: Long): Long
    protected abstract fun getBoolean(key: String, def: Boolean): Boolean
    protected abstract fun put(values: Map<String, Any?>)

    override val hasPin: Boolean get() = getString(KEY_PIN) != null

    override fun setPin(pin: String) {
        require(PinHasher.isValidPin(pin))
        put(mapOf(KEY_PIN to PinHasher.hash(pin), KEY_FAILURES to 0, KEY_LOCK_UNTIL to 0L))
    }

    override fun check(pin: String, now: Long): PinCheck {
        val until = getLong(KEY_LOCK_UNTIL, 0)
        if (until > now) return PinCheck.Locked(until - now)
        val stored = getString(KEY_PIN) ?: return PinCheck.Wrong(0, 0)
        return if (PinHasher.verify(pin, stored)) {
            put(mapOf(KEY_FAILURES to 0, KEY_LOCK_UNTIL to 0L))
            PinCheck.Ok
        } else {
            val failures = getInt(KEY_FAILURES, 0) + 1
            val delay = PinLockout.delayMillis(failures)
            put(mapOf(KEY_FAILURES to failures, KEY_LOCK_UNTIL to if (delay > 0) now + delay else 0L))
            if (delay > 0) PinCheck.Locked(delay) else PinCheck.Wrong(failures, PinLockout.FREE_ATTEMPTS - failures)
        }
    }

    override fun lockRemaining(now: Long): Long = (getLong(KEY_LOCK_UNTIL, 0) - now).coerceAtLeast(0)

    override var biometricEnabled: Boolean
        get() = getBoolean(KEY_BIO, false)
        set(v) = put(mapOf(KEY_BIO to v))

    override var autoLockSeconds: Int
        get() = getInt(KEY_AUTOLOCK, 30)
        set(v) = put(mapOf(KEY_AUTOLOCK to v))

    override var secureScreen: Boolean
        get() = getBoolean(KEY_SECURE, true)
        set(v) = put(mapOf(KEY_SECURE to v))

    override var introSeen: Boolean
        get() = getBoolean(KEY_INTRO, false)
        set(v) = put(mapOf(KEY_INTRO to v))

    companion object {
        const val KEY_PIN = "pin_hash"
        const val KEY_FAILURES = "pin_failures"
        const val KEY_LOCK_UNTIL = "pin_lock_until"
        const val KEY_BIO = "biometric"
        const val KEY_AUTOLOCK = "auto_lock"
        const val KEY_SECURE = "secure_screen"
        const val KEY_INTRO = "intro_seen"
    }
}

/** Réglages en mémoire (tests et aperçus). */
class InMemorySecuritySettings : BaseSecuritySettings() {
    private val map = HashMap<String, Any?>()
    override fun getString(key: String) = map[key] as? String
    override fun getInt(key: String, def: Int) = map[key] as? Int ?: def
    override fun getLong(key: String, def: Long) = (map[key] as? Number)?.toLong() ?: def
    override fun getBoolean(key: String, def: Boolean) = map[key] as? Boolean ?: def
    override fun put(values: Map<String, Any?>) { map.putAll(values) }
    override fun clear() = map.clear()
}

/** Réglages persistés dans les SharedPreferences privées de l'application. */
class SecurityStore(context: Context) : BaseSecuritySettings() {
    private val prefs: SharedPreferences = context.getSharedPreferences("essabank_security", Context.MODE_PRIVATE)

    override fun getString(key: String): String? = prefs.getString(key, null)
    override fun getInt(key: String, def: Int): Int = prefs.getInt(key, def)
    override fun getLong(key: String, def: Long): Long = prefs.getLong(key, def)
    override fun getBoolean(key: String, def: Boolean): Boolean = prefs.getBoolean(key, def)
    override fun put(values: Map<String, Any?>) = prefs.edit {
        for ((k, v) in values) when (v) {
            null -> remove(k)
            is String -> putString(k, v)
            is Int -> putInt(k, v)
            is Long -> putLong(k, v)
            is Boolean -> putBoolean(k, v)
        }
    }
    override fun clear() = prefs.edit { clear() }
}

sealed interface PinCheck {
    data object Ok : PinCheck
    data class Wrong(val failures: Int, val attemptsLeft: Int) : PinCheck
    data class Locked(val remainingMillis: Long) : PinCheck
}
