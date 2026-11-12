package com.superessence.essabank.core.engine

import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.money.Money
import java.security.SecureRandom
import java.time.Instant
import java.time.LocalDate
import java.time.LocalDateTime
import java.time.ZoneId
import java.time.ZonedDateTime
import java.util.UUID
import kotlin.random.Random

/** Erreurs métier du moteur. Le texte affiché est choisi par l'interface (FR/EN). */
sealed class BankError {
    data object InvalidAmount : BankError()
    data object AmountTooLarge : BankError()
    data class InsufficientFunds(val available: Money) : BankError()
    data object AccountNotFound : BankError()
    data object SameAccount : BankError()
    data object CardNotFound : BankError()
    data object CardLocked : BankError()
    data object CardInactive : BankError()
    data object CardExpired : BankError()
    data object OnlineDisabled : BankError()
    data class CardLimitExceeded(val remaining: Money) : BankError()
    data object DuplicateRequest : BankError()
    data object BeneficiaryNotFound : BankError()
    data object InvalidBeneficiary : BankError()
    data object InvalidName : BankError()
    data object GoalNotFound : BankError()
    data object GoalInsufficient : BankError()
    data object InvalidDate : BankError()
    data object AccountNotEmpty : BankError()
    data object PrimaryAccount : BankError()
    data object LastAccount : BankError()
    data object LimitReached : BankError()
    data object PremiumRequired : BankError()
    data object AssetNotFound : BankError()
    data object InsufficientQuantity : BankError()
    data object InvalidQuantity : BankError()
    data object ScheduledNotFound : BankError()
    data object InvalidAccountType : BankError()
}

/** Événement produit par une opération réussie (utilisé pour l'interface et les tests). */
sealed class BankEvent {
    data class TransactionsCreated(val ids: List<String>) : BankEvent()
    data class NotificationCreated(val id: String) : BankEvent()
    data class GoalReached(val goalId: String) : BankEvent()
    data class BudgetAlert(val key: String) : BankEvent()
}

sealed class EngineResult<out T> {
    data class Ok<T>(val state: BankState, val value: T, val events: List<BankEvent> = emptyList()) : EngineResult<T>()
    data class Err(val error: BankError) : EngineResult<Nothing>()

    val isOk: Boolean get() = this is Ok

    fun stateOrNull(): BankState? = (this as? Ok)?.state
    fun valueOrNull(): T? = when (this) { is Ok -> value; is Err -> null }
    fun errorOrNull(): BankError? = (this as? Err)?.error
}

/** Horloge injectable pour des tests déterministes. */
interface BankClock {
    fun now(): Long
    val zone: ZoneId
    fun today(): LocalDate = Instant.ofEpochMilli(now()).atZone(zone).toLocalDate()
    fun dateTime(): LocalDateTime = Instant.ofEpochMilli(now()).atZone(zone).toLocalDateTime()

    companion object {
        val SYSTEM: BankClock = object : BankClock {
            override fun now(): Long = System.currentTimeMillis()
            override val zone: ZoneId get() = ZoneId.systemDefault()
        }
    }
}

class FixedClock(var millis: Long, override val zone: ZoneId = ZoneId.of("Europe/Paris")) : BankClock {
    override fun now(): Long = millis
    fun advanceDays(days: Long) { millis += days * 86_400_000L }
    fun advanceMinutes(minutes: Long) { millis += minutes * 60_000L }

    companion object {
        fun at(y: Int, m: Int, d: Int, h: Int = 10, min: Int = 0, zone: ZoneId = ZoneId.of("Europe/Paris")) =
            FixedClock(ZonedDateTime.of(y, m, d, h, min, 0, 0, zone).toInstant().toEpochMilli(), zone)
    }
}

/** Générateur d'identifiants et de nombres aléatoires (injectable pour les tests). */
interface IdGenerator {
    fun id(prefix: String): String
    fun digits(count: Int): String
    fun nextInt(bound: Int): Int

    companion object {
        val SECURE: IdGenerator = object : IdGenerator {
            private val rnd = SecureRandom()
            override fun id(prefix: String): String = prefix + "_" + UUID.randomUUID().toString().replace("-", "").take(20)
            override fun digits(count: Int): String = buildString { repeat(count) { append(rnd.nextInt(10)) } }
            override fun nextInt(bound: Int): Int = rnd.nextInt(bound)
        }
    }
}

class SeededIds(seed: Long) : IdGenerator {
    private val rnd = Random(seed)
    private var counter = 0L
    override fun id(prefix: String): String = prefix + "_" + (++counter).toString().padStart(8, '0') + rnd.nextInt(1000, 9999)
    override fun digits(count: Int): String = buildString { repeat(count) { append(rnd.nextInt(10)) } }
    override fun nextInt(bound: Int): Int = rnd.nextInt(bound)
}

/** Fonctions utilitaires de date. */
object Dates {
    fun epochDay(millis: Long, zone: ZoneId): Long = Instant.ofEpochMilli(millis).atZone(zone).toLocalDate().toEpochDay()
    fun startOfDay(day: LocalDate, zone: ZoneId): Long = day.atStartOfDay(zone).toInstant().toEpochMilli()
    fun localDate(millis: Long, zone: ZoneId): LocalDate = Instant.ofEpochMilli(millis).atZone(zone).toLocalDate()
    fun monthKey(millis: Long, zone: ZoneId): String = localDate(millis, zone).let { "%04d-%02d".format(it.year, it.monthValue) }
    fun monthKey(day: LocalDate): String = "%04d-%02d".format(day.year, day.monthValue)
}
