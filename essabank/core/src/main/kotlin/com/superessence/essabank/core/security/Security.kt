package com.superessence.essabank.core.security

import java.security.MessageDigest
import java.security.SecureRandom
import java.util.Base64
import javax.crypto.Cipher
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.PBEKeySpec
import javax.crypto.spec.SecretKeySpec

/**
 * Hachage du code PIN local (PBKDF2-HMAC-SHA256, sel aléatoire de 16 octets).
 * Le PIN n'est jamais stocké en clair. Format : « pbkdf2$<itérations>$<sel b64>$<hash b64> ».
 */
object PinHasher {
    private const val ITERATIONS = 120_000
    private const val KEY_BITS = 256

    fun isValidPin(pin: String): Boolean = pin.length in 4..6 && pin.all { it.isDigit() } && !isTrivial(pin)

    /** Refuse les codes trop évidents (0000, 1234, 4321, 1111…). */
    fun isTrivial(pin: String): Boolean {
        if (pin.toSet().size == 1) return true
        val asc = "0123456789"; val desc = "9876543210"
        return asc.contains(pin) || desc.contains(pin)
    }

    fun hash(pin: String, iterations: Int = ITERATIONS): String {
        val salt = ByteArray(16).also { SecureRandom().nextBytes(it) }
        val h = derive(pin.toCharArray(), salt, iterations, KEY_BITS)
        val b64 = Base64.getEncoder()
        return "pbkdf2$$iterations$${b64.encodeToString(salt)}$${b64.encodeToString(h)}"
    }

    fun verify(pin: String, stored: String): Boolean {
        val parts = stored.split('$')
        if (parts.size != 4 || parts[0] != "pbkdf2") return false
        val iterations = parts[1].toIntOrNull() ?: return false
        val dec = Base64.getDecoder()
        val salt = dec.decode(parts[2]); val expected = dec.decode(parts[3])
        val actual = derive(pin.toCharArray(), salt, iterations, expected.size * 8)
        return MessageDigest.isEqual(expected, actual)
    }

    internal fun derive(secret: CharArray, salt: ByteArray, iterations: Int, bits: Int): ByteArray {
        val spec = PBEKeySpec(secret, salt, iterations, bits)
        return try { SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256").generateSecret(spec).encoded } finally { spec.clearPassword() }
    }
}

/**
 * Politique anti-force brute du PIN : après 5 échecs, attente croissante
 * (30 s, 1 min, 2 min… plafonnée à 15 min).
 */
object PinLockout {
    const val FREE_ATTEMPTS = 5
    fun delayMillis(failures: Int): Long {
        if (failures < FREE_ATTEMPTS) return 0
        val step = failures - FREE_ATTEMPTS
        return minOf(30_000L shl minOf(step, 5), 15 * 60_000L)
    }
}

/**
 * Sauvegarde chiffrée : AES-256-GCM, clé dérivée du mot de passe de sauvegarde
 * (PBKDF2, 150 000 itérations). Format binaire : « ESSABK1 » + sel(16) + iv(12) + données chiffrées.
 */
object BackupCodec {
    private val MAGIC = "ESSABK1".toByteArray(Charsets.US_ASCII)
    private const val ITERATIONS = 150_000

    class BadPasswordException : Exception("Mot de passe incorrect ou fichier endommagé")
    class BadFormatException : Exception("Ce fichier n'est pas une sauvegarde ESSABANK")

    fun encrypt(plain: ByteArray, password: String): ByteArray {
        require(password.length >= 6) { "Mot de passe trop court" }
        val rnd = SecureRandom()
        val salt = ByteArray(16).also(rnd::nextBytes)
        val iv = ByteArray(12).also(rnd::nextBytes)
        val key = SecretKeySpec(PinHasher.derive(password.toCharArray(), salt, ITERATIONS, 256), "AES")
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, key, GCMParameterSpec(128, iv))
        cipher.updateAAD(MAGIC)
        return MAGIC + salt + iv + cipher.doFinal(plain)
    }

    fun decrypt(data: ByteArray, password: String): ByteArray {
        if (data.size < MAGIC.size + 28 + 16 || !data.copyOfRange(0, MAGIC.size).contentEquals(MAGIC)) throw BadFormatException()
        var p = MAGIC.size
        val salt = data.copyOfRange(p, p + 16); p += 16
        val iv = data.copyOfRange(p, p + 12); p += 12
        val key = SecretKeySpec(PinHasher.derive(password.toCharArray(), salt, ITERATIONS, 256), "AES")
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128, iv))
        cipher.updateAAD(MAGIC)
        return try { cipher.doFinal(data, p, data.size - p) } catch (e: javax.crypto.AEADBadTagException) { throw BadPasswordException() }
    }
}
