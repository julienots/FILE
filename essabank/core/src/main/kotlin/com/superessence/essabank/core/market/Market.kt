package com.superessence.essabank.core.market

import com.superessence.essabank.core.model.AssetType
import com.superessence.essabank.core.money.Money
import java.math.BigDecimal
import java.math.RoundingMode
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import kotlin.math.exp
import kotlin.math.ln
import kotlin.math.sqrt

/**
 * Actif FICTIF. Les noms et symboles sont inventés (suffixe « .SIM ») et ne
 * correspondent à aucune valeur cotée réelle.
 */
data class Asset(
    val symbol: String,
    val name: String,
    val type: AssetType,
    val sectorFr: String,
    val sectorEn: String,
    /** Prix de départ en unités virtuelles au 1er janvier 2024. */
    val basePrice: Double,
    /** Volatilité annuelle (0,25 = 25 %). */
    val volatility: Double,
    /** Tendance annuelle moyenne. */
    val drift: Double,
    val descriptionFr: String,
    val descriptionEn: String,
)

data class PricePoint(val time: Long, val price: Money)

enum class ChartRange(val days: Int) { DAY(1), WEEK(7), MONTH(30), YEAR(365) }

/**
 * Marché de démonstration : prix générés localement et de façon déterministe
 * (marche aléatoire log-normale initialisée par le symbole). Les performances
 * affichées ne représentent pas des prix de marché réels.
 */
class MarketSimulator(private val zone: ZoneId = ZoneId.systemDefault()) {

    val assets: List<Asset> = Catalogue.assets
    private val bySymbol = assets.associateBy { it.symbol }
    private val closeCache = HashMap<String, DoubleArray>()

    fun asset(symbol: String): Asset? = bySymbol[symbol]

    /** Prix fictif à l'instant donné, au pas de 5 minutes. */
    fun price(symbol: String, at: Long): Money {
        val a = bySymbol[symbol] ?: return Money.ZERO
        return toMoney(priceAt(a, at))
    }

    fun previousClose(symbol: String, at: Long): Money {
        val a = bySymbol[symbol] ?: return Money.ZERO
        val day = dayIndex(at)
        return toMoney(close(a, day - 1))
    }

    /** Variation depuis la clôture fictive de la veille, en pourcentage. */
    fun dayChangePercent(symbol: String, at: Long): BigDecimal {
        val now = price(symbol, at); val prev = previousClose(symbol, at)
        if (prev.isZero) return BigDecimal.ZERO
        return (now.toBigDecimal() - prev.toBigDecimal())
            .multiply(BigDecimal(100)).divide(prev.toBigDecimal(), 2, RoundingMode.HALF_EVEN)
    }

    fun changePercent(symbol: String, at: Long, range: ChartRange): BigDecimal {
        val h = history(symbol, at, range)
        if (h.size < 2 || h.first().price.isZero) return BigDecimal.ZERO
        val first = h.first().price.toBigDecimal(); val last = h.last().price.toBigDecimal()
        return (last - first).multiply(BigDecimal(100)).divide(first, 2, RoundingMode.HALF_EVEN)
    }

    /** Série de prix pour un graphique. */
    fun history(symbol: String, at: Long, range: ChartRange): List<PricePoint> {
        val a = bySymbol[symbol] ?: return emptyList()
        val points = ArrayList<PricePoint>()
        when (range) {
            ChartRange.DAY -> {
                val start = startOfDay(at)
                var t = start
                while (t <= at) { points += PricePoint(t, toMoney(priceAt(a, t))); t += STEP_MS }
                if (points.isEmpty() || points.last().time != at) points += PricePoint(at, toMoney(priceAt(a, at)))
            }
            ChartRange.WEEK -> {
                var t = at - 7 * DAY_MS
                while (t < at) { points += PricePoint(t, toMoney(priceAt(a, t))); t += 2 * HOUR_MS }
                points += PricePoint(at, toMoney(priceAt(a, at)))
            }
            ChartRange.MONTH, ChartRange.YEAR -> {
                val today = dayIndex(at)
                for (d in today - range.days until today) points += PricePoint(dayStartMillis(d) + DAY_MS - 1, toMoney(close(a, d)))
                points += PricePoint(at, toMoney(priceAt(a, at)))
            }
        }
        return points
    }

    // --------------------------------------------------------------- modèle

    private fun priceAt(a: Asset, at: Long): Double {
        val day = dayIndex(at)
        val prev = close(a, day - 1)
        val next = close(a, day)
        val dayStart = dayStartMillis(day)
        val k = ((at - dayStart) / STEP_MS).toInt().coerceIn(0, STEPS_PER_DAY)
        // Pont brownien entre la clôture de la veille et celle du jour.
        val sigmaIntra = a.volatility / sqrt(252.0) * 0.6
        val w = intradayWalk(a, day)
        val bridge = w[k] - k.toDouble() / STEPS_PER_DAY * w[STEPS_PER_DAY]
        val logP = ln(prev) + (ln(next) - ln(prev)) * k / STEPS_PER_DAY + sigmaIntra * bridge
        return exp(logP)
    }

    private fun intradayWalk(a: Asset, day: Int): DoubleArray {
        val rnd = java.util.Random(seed(a) * 7919L + day * 104_729L)
        val w = DoubleArray(STEPS_PER_DAY + 1)
        val scale = 1.0 / sqrt(STEPS_PER_DAY.toDouble())
        for (i in 1..STEPS_PER_DAY) w[i] = w[i - 1] + rnd.nextGaussian() * scale
        return w
    }

    /** Clôture fictive du jour d'index [day] (jours depuis l'origine). */
    private fun close(a: Asset, day: Int): Double {
        if (day < 0) return a.basePrice
        val arr = synchronized(closeCache) { closeCache[a.symbol] }
        if (arr != null && day < arr.size) return arr[day]
        val size = maxOf(day + 400, 1200)
        val out = DoubleArray(size)
        val rnd = java.util.Random(seed(a))
        val dt = 1.0 / 365.0
        val sigma = a.volatility
        var logP = ln(a.basePrice)
        // Régimes de marché lents : périodes de hausse et de correction.
        for (d in 0 until size) {
            val regime = kotlin.math.sin(d / 97.0 + seed(a) % 13) * 0.6 + kotlin.math.sin(d / 23.0 + seed(a) % 7) * 0.25
            val mu = a.drift + regime * sigma * 0.8
            logP += (mu - sigma * sigma / 2) * dt + sigma * sqrt(dt) * rnd.nextGaussian()
            // Garde-fou : l'actif fictif ne descend jamais sous 5 % de son prix de base.
            logP = maxOf(logP, ln(a.basePrice * 0.05))
            out[d] = exp(logP)
        }
        synchronized(closeCache) { closeCache[a.symbol] = out }
        return out[day]
    }

    private fun seed(a: Asset): Long = a.symbol.fold(1125899906842597L) { h, c -> 31 * h + c.code }

    private fun dayIndex(at: Long): Int =
        (Instant.ofEpochMilli(at).atZone(zone).toLocalDate().toEpochDay() - ORIGIN.toEpochDay()).toInt()

    private fun dayStartMillis(day: Int): Long = ORIGIN.plusDays(day.toLong()).atStartOfDay(zone).toInstant().toEpochMilli()

    private fun startOfDay(at: Long): Long = Instant.ofEpochMilli(at).atZone(zone).toLocalDate().atStartOfDay(zone).toInstant().toEpochMilli()

    private fun toMoney(p: Double): Money = Money.of(BigDecimal(p).setScale(2, RoundingMode.HALF_EVEN).max(BigDecimal("0.01")))

    companion object {
        val ORIGIN: LocalDate = LocalDate.of(2024, 1, 1)
        const val STEP_MS = 5 * 60_000L
        const val HOUR_MS = 3_600_000L
        const val DAY_MS = 86_400_000L
        const val STEPS_PER_DAY = 288
        const val MICRO = 1_000_000L

        /** Montant = prix × quantité (quantité en millionièmes), arrondi au centième. */
        fun value(price: Money, quantityMicro: Long): Money =
            Money.of(price.toBigDecimal().multiply(BigDecimal.valueOf(quantityMicro, 6)))

        /** Quantité achetable avec un montant, arrondie au millionième inférieur. */
        fun quantityFor(amount: Money, price: Money): Long {
            if (price.cents <= 0) return 0
            return amount.toBigDecimal().divide(price.toBigDecimal(), 6, RoundingMode.DOWN)
                .movePointRight(6).longValueExact()
        }

        fun formatQuantity(quantityMicro: Long): String =
            BigDecimal.valueOf(quantityMicro, 6).stripTrailingZeros().toPlainString()
    }
}

private object Catalogue {
    val assets = listOf(
        // Actions fictives
        Asset("NWND.SIM", "Nordwind Énergies", AssetType.STOCK, "Énergie", "Energy", 48.20, 0.24, 0.07,
            "Producteur fictif d'énergie éolienne en mer.", "Fictional offshore wind energy producer."),
        Asset("HLXR.SIM", "Helix Robotique", AssetType.STOCK, "Technologie", "Technology", 132.50, 0.38, 0.14,
            "Concepteur imaginaire de robots industriels.", "Imaginary industrial robotics designer."),
        Asset("AUFD.SIM", "Aurelia Gourmet", AssetType.STOCK, "Consommation", "Consumer", 61.75, 0.17, 0.05,
            "Groupe agroalimentaire fictif.", "Fictional food and beverage group."),
        Asset("QNTX.SIM", "Quantix Puces", AssetType.STOCK, "Semi-conducteurs", "Semiconductors", 214.00, 0.45, 0.16,
            "Fabricant imaginaire de microprocesseurs.", "Imaginary microprocessor maker."),
        Asset("SOLP.SIM", "Solenne Santé", AssetType.STOCK, "Santé", "Healthcare", 88.40, 0.22, 0.06,
            "Laboratoire pharmaceutique fictif.", "Fictional pharmaceutical laboratory."),
        Asset("VIRM.SIM", "Vireo Mobilité", AssetType.STOCK, "Automobile", "Automotive", 37.10, 0.41, 0.09,
            "Constructeur imaginaire de véhicules électriques.", "Imaginary electric vehicle maker."),
        Asset("CLYP.SIM", "Calypso Voyages", AssetType.STOCK, "Tourisme", "Travel", 24.60, 0.33, 0.04,
            "Agence de voyages fictive.", "Fictional travel agency."),
        Asset("ORBT.SIM", "Orbis Réseaux", AssetType.STOCK, "Télécoms", "Telecom", 18.90, 0.19, 0.03,
            "Opérateur de télécommunications imaginaire.", "Imaginary telecom operator."),
        // Cryptomonnaies fictives
        Asset("ESC.SIM", "Essa Coin", AssetType.CRYPTO, "Crypto fictive", "Fictional crypto", 1840.00, 0.72, 0.20,
            "Jeton de démonstration de l'univers ESSABANK.", "Demo token of the ESSABANK universe."),
        Asset("NBLT.SIM", "Nebulite", AssetType.CRYPTO, "Crypto fictive", "Fictional crypto", 312.00, 0.85, 0.18,
            "Cryptomonnaie imaginaire à forte volatilité.", "Highly volatile imaginary cryptocurrency."),
        Asset("QRK.SIM", "Quark", AssetType.CRYPTO, "Crypto fictive", "Fictional crypto", 0.86, 0.95, 0.12,
            "Micro-jeton fictif.", "Fictional micro-token."),
        Asset("HALO.SIM", "Halo Token", AssetType.CRYPTO, "Crypto fictive", "Fictional crypto", 27.40, 0.80, 0.15,
            "Jeton imaginaire de réseau décentralisé.", "Imaginary decentralized network token."),
        // ETF fictifs
        Asset("EWLD.SIM", "ESSA Monde Index", AssetType.ETF, "Indice mondial", "World index", 104.30, 0.14, 0.07,
            "Panier fictif d'actions mondiales.", "Fictional basket of global equities."),
        Asset("EGRN.SIM", "ESSA Avenir Vert", AssetType.ETF, "Transition énergétique", "Clean energy", 56.80, 0.21, 0.08,
            "Panier fictif d'entreprises de la transition.", "Fictional clean-transition basket."),
        Asset("ETEC.SIM", "ESSA Tech Momentum", AssetType.ETF, "Technologie", "Technology", 148.90, 0.26, 0.11,
            "Panier fictif de valeurs technologiques.", "Fictional technology basket."),
        Asset("EDIV.SIM", "ESSA Dividendes Europe", AssetType.ETF, "Dividendes", "Dividends", 72.15, 0.12, 0.05,
            "Panier fictif d'actions à dividendes.", "Fictional dividend equity basket."),
    )
}
