package com.superessence.essabank.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.material.icons.automirrored.rounded.ShowChart
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.market.Asset
import com.superessence.essabank.core.market.ChartRange
import com.superessence.essabank.core.market.MarketSimulator
import com.superessence.essabank.core.model.AccountType
import com.superessence.essabank.core.model.AssetType
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.TradeSide
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.MoneyFormat
import com.superessence.essabank.core.money.ParseResult
import com.superessence.essabank.core.money.sumOfMoney
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.EssaSheet
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.KeyValue
import com.superessence.essabank.ui.components.LineChart
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SegmentedControl
import com.superessence.essabank.ui.components.Sparkline
import com.superessence.essabank.ui.components.StatTile
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import java.math.BigDecimal

private fun assetColor(t: AssetType) = when (t) {
    AssetType.STOCK -> androidx.compose.ui.graphics.Color(0xFF2F6BFF)
    AssetType.CRYPTO -> androidx.compose.ui.graphics.Color(0xFFF5A524)
    AssetType.ETF -> androidx.compose.ui.graphics.Color(0xFF12B886)
}

@Composable
private fun MarketDisclaimer() {
    val c = Essa.colors
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(c.warning.copy(alpha = 0.12f)).padding(12.dp), verticalAlignment = Alignment.Top) {
        Icon(Icons.Rounded.Info, null, tint = c.warning)
        Spacer(Modifier.width(10.dp))
        Text(tr("Marché de démonstration : actifs inventés et prix générés localement. Les performances ne représentent pas des prix de marché réels. Ce n'est pas un service d'investissement.",
            "Demo market: made-up assets and locally generated prices. Performance does not represent real market prices. This is not an investment service."),
            style = EssaType.small, color = c.text)
    }
}

@Composable
fun InvestScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val now by vm.now.collectAsStateWithLifecycle()
    val c = Essa.colors
    val lang = LocalLang.current
    val market = vm.market
    val account = s.accounts.firstOrNull { it.type == AccountType.INVESTMENT }
    var tab by remember { mutableIntStateOf(0) }
    val value = remember(s, now) { s.holdings.sumOfMoney { MarketSimulator.value(market.price(it.symbol, now), it.quantityMicro) } }
    val cost = s.holdings.sumOfMoney { it.costBasis }
    val unrealized = value - cost
    val realized = s.trades.filter { it.side == TradeSide.SELL }.sumOfMoney { it.realized }
    val history = remember(s, now) { portfolioSeries(s, market, now, 30) }
    val createdMsg = tr("Compte investissement fictif ouvert.", "Fictional investment account opened.")
    val accName = tr(lang, "Investissement fictif", "Fictional investing")

    ScreenScaffold(tr("Investissements simulés", "Simulated investing"), nav::back) {
        item { MarketDisclaimer() }
        item {
            Panel {
                Text(tr("Valeur du portefeuille virtuel", "Virtual portfolio value"), style = EssaType.small, color = c.textMuted)
                Text(fmt(value), style = EssaType.amountLarge, color = c.text)
                Row(verticalAlignment = Alignment.CenterVertically) {
                    ChangeBadge(fmt(unrealized, signed = true) + "  " + MoneyFormat.percent(if (cost.isZero) BigDecimal.ZERO else vm.analytics.change(cost, value), lang), !unrealized.isNegative)
                    Spacer(Modifier.width(8.dp))
                    Text(tr("plus/moins-value latente fictive", "fictional unrealized gain/loss"), style = EssaType.small, color = c.textMuted)
                }
                Spacer(Modifier.height(12.dp))
                if (history.size > 1) LineChart(history.map { it.cents.toFloat() }, if (unrealized.isNegative) c.negative else c.positive, height = 120.dp)
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                StatTile(tr("Liquidités", "Cash"), account?.let { fmt(it.balance) } ?: "—", Modifier.weight(1f))
                StatTile(tr("Plus-values réalisées", "Realized gains"), fmt(realized, signed = true), Modifier.weight(1f), valueColor = if (realized.isNegative) c.negative else c.positive)
            }
        }
        item {
            if (account == null) PrimaryButton(tr("Ouvrir un compte investissement fictif", "Open a fictional investment account"), {
                vm.run(createdMsg) { createAccount(it, AccountType.INVESTMENT, accName, 5, "chart") }
            })
            else Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SecondaryButton(tr("Alimenter", "Add cash"), { nav.go(Routes.transfer("internal", to = account.id)) }, Modifier.weight(1f), icon = Icons.Rounded.Add)
                SecondaryButton(tr("Retirer", "Withdraw"), { nav.go(Routes.transfer("internal", from = account.id)) }, Modifier.weight(1f), icon = Icons.Rounded.Remove)
            }
        }
        if (s.holdings.isNotEmpty()) {
            item { SectionHeader(tr("Mes positions fictives", "My fictional positions")) }
            item {
                Panel(padding = 8.dp) {
                    s.holdings.forEach { h ->
                        val asset = market.asset(h.symbol) ?: return@forEach
                        val v = MarketSimulator.value(market.price(h.symbol, now), h.quantityMicro)
                        val pnl = v - h.costBasis
                        ListRow(asset.name, MarketSimulator.formatQuantity(h.quantityMicro) + " · " + asset.symbol,
                            leading = { IconBadge(Icons.AutoMirrored.Rounded.ShowChart, assetColor(asset.type), 42.dp) },
                            trailing = {
                                Column(horizontalAlignment = Alignment.End) {
                                    Text(fmt(v), style = EssaType.amount, color = c.text)
                                    Text(fmt(pnl, signed = true), style = EssaType.small.copy(fontWeight = FontWeight.SemiBold), color = if (pnl.isNegative) c.negative else c.positive)
                                }
                            }, onClick = { nav.go(Routes.asset(h.symbol)) }, chevron = false)
                    }
                }
            }
        }
        item { SectionHeader(tr("Marché de démonstration", "Demo market")) }
        item { SegmentedControl(listOf(tr("Actions", "Stocks"), tr("Cryptos", "Crypto"), "ETF"), tab, { tab = it }) }
        val type = listOf(AssetType.STOCK, AssetType.CRYPTO, AssetType.ETF)[tab]
        items(market.assets.filter { it.type == type }, key = { it.symbol }) { a -> AssetRow(vm, a, now, lang) { nav.go(Routes.asset(a.symbol)) } }
        if (s.trades.isNotEmpty()) {
            item { SectionHeader(tr("Historique des ordres virtuels", "Virtual order history")) }
            item {
                Panel(padding = 8.dp) {
                    s.trades.sortedByDescending { it.timestamp }.take(15).forEach { t ->
                        val name = market.asset(t.symbol)?.name ?: t.symbol
                        ListRow((if (t.side == TradeSide.BUY) tr("Achat · ", "Buy · ") else tr("Vente · ", "Sell · ")) + name,
                            MarketSimulator.formatQuantity(t.quantityMicro) + " × " + fmt(t.price, hideable = false) + " · " + L.date(t.timestamp, vm.zone, lang),
                            trailing = { Text(fmt(if (t.side == TradeSide.BUY) -t.total else t.total, signed = true), style = EssaType.amount, color = if (t.side == TradeSide.SELL) c.positive else c.text) })
                    }
                }
            }
        }
    }
}

/** Valeur quotidienne du portefeuille fictif (quantités détenues × prix de clôture fictif). */
private fun portfolioSeries(s: BankState, market: MarketSimulator, now: Long, days: Int): List<Money> {
    if (s.trades.isEmpty()) return emptyList()
    val trades = s.trades.sortedBy { it.timestamp }
    return (days - 1 downTo 0).map { d ->
        val at = now - d * 86_400_000L
        val qty = HashMap<String, Long>()
        for (t in trades) { if (t.timestamp > at) break; qty[t.symbol] = (qty[t.symbol] ?: 0L) + if (t.side == TradeSide.BUY) t.quantityMicro else -t.quantityMicro }
        Money(qty.entries.sumOf { (sym, q) -> if (q <= 0) 0L else MarketSimulator.value(market.price(sym, at), q).cents })
    }
}

@Composable
private fun AssetRow(vm: AppViewModel, a: Asset, now: Long, lang: AppLanguage, onClick: () -> Unit) {
    val c = Essa.colors
    val price = vm.market.price(a.symbol, now)
    val change = vm.market.dayChangePercent(a.symbol, now)
    val spark = remember(a.symbol, now / 600_000) { vm.market.history(a.symbol, now, ChartRange.WEEK).map { it.price.cents.toFloat() } }
    Panel(onClick = onClick, padding = 12.dp) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconBadge(Icons.AutoMirrored.Rounded.ShowChart, assetColor(a.type), 42.dp)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(a.name, style = EssaType.title, color = c.text, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(a.symbol + " · " + if (lang == AppLanguage.FR) a.sectorFr else a.sectorEn, style = EssaType.small, color = c.textMuted, maxLines = 1)
            }
            Box(Modifier.width(70.dp)) { Sparkline(spark, if (change.signum() >= 0) c.positive else c.negative) }
            Spacer(Modifier.width(10.dp))
            Column(horizontalAlignment = Alignment.End) {
                Text(fmt(price, hideable = false), style = EssaType.amount, color = c.text)
                Text(MoneyFormat.percent(change, lang), style = EssaType.small.copy(fontWeight = FontWeight.SemiBold), color = if (change.signum() >= 0) c.positive else c.negative)
            }
        }
    }
}

@Composable
fun AssetScreen(vm: AppViewModel, nav: Nav, symbol: String) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val now by vm.now.collectAsStateWithLifecycle()
    val asset = vm.market.asset(symbol)
    if (asset == null) { LaunchedEffect(Unit) { nav.back() }; return }
    val c = Essa.colors
    val lang = LocalLang.current
    var range by remember { mutableIntStateOf(0) }
    var sel by remember { mutableStateOf<Int?>(null) }
    var order by remember { mutableStateOf<TradeSide?>(null) }
    val r = ChartRange.entries[range]
    val history = remember(symbol, range, now) { vm.market.history(symbol, now, r) }
    val price = vm.market.price(symbol, now)
    val change = vm.market.changePercent(symbol, now, r)
    val holding = s.holdings.firstOrNull { it.symbol == symbol }
    val shown = sel?.let { history.getOrNull(it) }
    val account = s.accounts.firstOrNull { it.type == AccountType.INVESTMENT }

    ScreenScaffold(asset.name, nav::back) {
        item { MarketDisclaimer() }
        item {
            Panel {
                Text(asset.symbol + " · " + (if (lang == AppLanguage.FR) asset.sectorFr else asset.sectorEn), style = EssaType.small, color = c.textMuted)
                Text(fmt(shown?.price ?: price, hideable = false), style = EssaType.amountLarge.copy(fontSize = 34.sp), color = c.text)
                Row(verticalAlignment = Alignment.CenterVertically) {
                    ChangeBadge(MoneyFormat.percent(change, lang), change.signum() >= 0)
                    Spacer(Modifier.width(8.dp))
                    Text(shown?.let { L.dateTime(it.time, vm.zone, lang) } ?: tr("Prix fictif en direct", "Live fictional price"), style = EssaType.small, color = c.textMuted)
                }
                Spacer(Modifier.height(12.dp))
                LineChart(history.map { it.price.cents.toFloat() }, if (change.signum() >= 0) c.positive else c.negative, height = 190.dp, selected = sel, onSelect = { sel = it }, animationKey = range)
                Spacer(Modifier.height(10.dp))
                SegmentedControl(listOf(tr("1J", "1D"), tr("1S", "1W"), "1M", tr("1A", "1Y")), range, { range = it; sel = null })
            }
        }
        if (holding != null) item {
            val v = MarketSimulator.value(price, holding.quantityMicro)
            val pnl = v - holding.costBasis
            val qtyDec = BigDecimal.valueOf(holding.quantityMicro, 6)
            val avg = if (holding.quantityMicro > 0) Money.of(holding.costBasis.toBigDecimal().divide(qtyDec, 2, java.math.RoundingMode.HALF_EVEN)) else Money.ZERO
            Panel {
                Text(tr("Ma position fictive", "My fictional position"), style = EssaType.title, color = c.text)
                KeyValue(tr("Quantité", "Quantity"), MarketSimulator.formatQuantity(holding.quantityMicro))
                KeyValue(tr("Valeur actuelle", "Current value"), fmt(v))
                KeyValue(tr("Prix de revient moyen", "Average cost"), fmt(avg, hideable = false))
                KeyValue(tr("Plus/moins-value latente", "Unrealized gain/loss"), fmt(pnl, signed = true), valueColor = if (pnl.isNegative) c.negative else c.positive)
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                PrimaryButton(tr("Acheter", "Buy"), { order = TradeSide.BUY }, Modifier.weight(1f), enabled = account != null)
                SecondaryButton(tr("Vendre", "Sell"), { order = TradeSide.SELL }, Modifier.weight(1f).height(56.dp), enabled = holding != null && account != null)
            }
        }
        item {
            Panel {
                Text(tr("À propos (fictif)", "About (fictional)"), style = EssaType.title, color = c.text)
                Spacer(Modifier.height(6.dp))
                Text(if (lang == AppLanguage.FR) asset.descriptionFr else asset.descriptionEn, style = EssaType.body, color = c.textSecondary)
                KeyValue(tr("Volatilité simulée", "Simulated volatility"), "${(asset.volatility * 100).toInt()} %")
                KeyValue(tr("Type", "Type"), when (asset.type) { AssetType.STOCK -> tr("Action fictive", "Fictional stock"); AssetType.CRYPTO -> tr("Cryptomonnaie fictive", "Fictional crypto"); AssetType.ETF -> tr("ETF fictif", "Fictional ETF") })
            }
        }
        val trades = s.trades.filter { it.symbol == symbol }.sortedByDescending { it.timestamp }
        if (trades.isNotEmpty()) item {
            Panel(padding = 8.dp) {
                trades.take(10).forEach { t ->
                    ListRow(if (t.side == TradeSide.BUY) tr("Achat virtuel", "Virtual buy") else tr("Vente virtuelle", "Virtual sale"),
                        MarketSimulator.formatQuantity(t.quantityMicro) + " × " + fmt(t.price, hideable = false) + " · " + L.date(t.timestamp, vm.zone, lang),
                        trailing = { Text(fmt(t.total), style = EssaType.amount, color = c.text) })
                }
            }
        }
    }
    order?.let { side -> if (account != null) OrderSheet(vm, s, asset, side, price, account.id) { order = null } }
}

@Composable
private fun OrderSheet(vm: AppViewModel, s: BankState, asset: Asset, side: TradeSide, price: Money, accountId: String, onDismiss: () -> Unit) {
    val c = Essa.colors
    val isBuy = side == TradeSide.BUY
    val holding = s.holdings.firstOrNull { it.symbol == asset.symbol }
    val cash = s.account(accountId)?.balance ?: Money.ZERO
    var text by remember { mutableStateOf("") }
    var fraction by remember { mutableStateOf<Int?>(null) }
    val parsed = Money.parse(text)
    val requestId = remember { java.util.UUID.randomUUID().toString() }
    val okBuy = tr("Achat virtuel exécuté.", "Virtual purchase executed.")
    val okSell = tr("Vente virtuelle exécutée.", "Virtual sale executed.")
    val sellQty: Long? = when {
        fraction != null && holding != null -> if (fraction == 100) holding.quantityMicro else holding.quantityMicro * fraction!! / 100
        parsed is ParseResult.Valid && holding != null -> MarketSimulator.quantityFor(parsed.money, price).coerceAtMost(holding.quantityMicro)
        else -> null
    }
    val estimate = if (isBuy) (parsed as? ParseResult.Valid)?.money?.let { MarketSimulator.quantityFor(it, price) } else sellQty
    EssaSheet(onDismiss) {
        Text((if (isBuy) tr("Acheter ", "Buy ") else tr("Vendre ", "Sell ")) + asset.name, style = EssaType.h2, color = c.text)
        Text(tr("Prix fictif : ", "Fictional price: ") + fmt(price, hideable = false) + " · " + (if (isBuy) tr("liquidités ", "cash ") + fmt(cash, hideable = false)
            else tr("détenu : ", "held: ") + MarketSimulator.formatQuantity(holding?.quantityMicro ?: 0)), style = EssaType.small, color = c.textMuted)
        Spacer(Modifier.height(12.dp))
        EssaTextField(text, { text = it; fraction = null }, tr("Montant (€)", "Amount (€)"), keyboard = KeyboardType.Decimal, maxLength = 12)
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (isBuy) listOf(50, 100, 250, 500).forEach { v -> Chip("$v", text == "$v") { text = "$v" } }
            else listOf(25, 50, 100).forEach { p -> Chip("$p %", fraction == p) { fraction = p; text = "" } }
        }
        Spacer(Modifier.height(10.dp))
        Text(tr("Quantité estimée : ", "Estimated quantity: ") + (estimate?.let { MarketSimulator.formatQuantity(it) } ?: "—"), style = EssaType.bodyStrong, color = c.textSecondary)
        Spacer(Modifier.height(14.dp))
        PrimaryButton(if (isBuy) tr("Confirmer l'achat virtuel", "Confirm virtual purchase") else tr("Confirmer la vente virtuelle", "Confirm virtual sale"),
            enabled = if (isBuy) parsed is ParseResult.Valid else (sellQty ?: 0) > 0, onClick = {
                if (isBuy) {
                    val amount = (parsed as? ParseResult.Valid)?.money ?: return@PrimaryButton
                    vm.run(okBuy, onOk = { onDismiss() }) { buy(it, asset.symbol, amount, accountId, requestId) }
                } else {
                    val q = sellQty ?: return@PrimaryButton
                    vm.run(okSell, onOk = { onDismiss() }) { sell(it, asset.symbol, q, accountId, requestId) }
                }
            })
        Spacer(Modifier.height(8.dp))
        Text(tr("Ordre exécuté immédiatement au prix fictif affiché, sans frais. Aucun ordre n'est transmis à un marché réel.", "Order executed immediately at the displayed fictional price, with no fees. No order is sent to any real market."),
            style = EssaType.small, color = c.textMuted)
    }
}
