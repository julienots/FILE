package com.superessence.essabank.data

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import com.superessence.essabank.core.security.PinHasher
import com.superessence.essabank.core.security.PinLockout

/**
 * Réglages de sécurité locaux : PIN haché (PBKDF2), compteur d'échecs, biométrie,
 * délai de verrouillage automatique et protection d'écran. Le PIN n'est jamais stocké en clair.
 * Ce fichier est exclu des sauvegardes Android (voir data_extraction_rules.xml).
 */
class SecurityStore(context: Context) {
    private val prefs: SharedPreferences = context.getSharedPreferences("essabank_security", Context.MODE_PRIVATE)

    val hasPin: Boolean get() = prefs.getString(KEY_PIN, null) != null

    fun setPin(pin: String) {
        require(PinHasher.isValidPin(pin))
        prefs.edit { putString(KEY_PIN, PinHasher.hash(pin)); putInt(KEY_FAILURES, 0); putLong(KEY_LOCK_UNTIL, 0) }
    }

    /** Vérifie le PIN. Renvoie [PinCheck.Locked] pendant l'attente imposée après trop d'échecs. */
    fun check(pin: String, now: Long = System.currentTimeMillis()): PinCheck {
        val until = prefs.getLong(KEY_LOCK_UNTIL, 0)
        if (until > now) return PinCheck.Locked(until - now)
        val stored = prefs.getString(KEY_PIN, null) ?: return PinCheck.Wrong(0, 0)
        return if (PinHasher.verify(pin, stored)) {
            prefs.edit { putInt(KEY_FAILURES, 0); putLong(KEY_LOCK_UNTIL, 0) }
            PinCheck.Ok
        } else {
            val failures = prefs.getInt(KEY_FAILURES, 0) + 1
            val delay = PinLockout.delayMillis(failures)
            prefs.edit { putInt(KEY_FAILURES, failures); putLong(KEY_LOCK_UNTIL, if (delay > 0) now + delay else 0) }
            if (delay > 0) PinCheck.Locked(delay) else PinCheck.Wrong(failures, PinLockout.FREE_ATTEMPTS - failures)
        }
    }

    fun lockRemaining(now: Long = System.currentTimeMillis()): Long = (prefs.getLong(KEY_LOCK_UNTIL, 0) - now).coerceAtLeast(0)

    var biometricEnabled: Boolean
        get() = prefs.getBoolean(KEY_BIO, false)
        set(v) = prefs.edit { putBoolean(KEY_BIO, v) }

    /** Délai avant verrouillage automatique en arrière-plan, en secondes (0 = immédiat). */
    var autoLockSeconds: Int
        get() = prefs.getInt(KEY_AUTOLOCK, 30)
        set(v) = prefs.edit { putInt(KEY_AUTOLOCK, v) }

    /** Bloque les captures d'écran et masque l'aperçu dans les applications récentes. */
    var secureScreen: Boolean
        get() = prefs.getBoolean(KEY_SECURE, true)
        set(v) = prefs.edit { putBoolean(KEY_SECURE, v) }

    var introSeen: Boolean
        get() = prefs.getBoolean(KEY_INTRO, false)
        set(v) = prefs.edit { putBoolean(KEY_INTRO, v) }

    fun clear() = prefs.edit { clear() }

    companion object {
        private const val KEY_PIN = "pin_hash"
        private const val KEY_FAILURES = "pin_failures"
        private const val KEY_LOCK_UNTIL = "pin_lock_until"
        private const val KEY_BIO = "biometric"
        private const val KEY_AUTOLOCK = "auto_lock"
        private const val KEY_SECURE = "secure_screen"
        private const val KEY_INTRO = "intro_seen"
    }
}

sealed interface PinCheck {
    data object Ok : PinCheck
    data class Wrong(val failures: Int, val attemptsLeft: Int) : PinCheck
    data class Locked(val remainingMillis: Long) : PinCheck
}
