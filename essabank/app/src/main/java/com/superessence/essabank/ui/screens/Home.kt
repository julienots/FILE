package com.superessence.essabank.ui.screens

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ReceiptLong
import androidx.compose.material.icons.rounded.AccountBalance
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.automirrored.rounded.CallReceived
import androidx.compose.material.icons.rounded.Assessment
import androidx.compose.material.icons.rounded.CreditCard
import androidx.compose.material.icons.rounded.Notifications
import androidx.compose.material.icons.rounded.Savings
import androidx.compose.material.icons.automirrored.rounded.ShowChart
import androidx.compose.material.icons.rounded.SwapHoriz
import androidx.compose.material.icons.rounded.Visibility
import androidx.compose.material.icons.rounded.VisibilityOff
import androidx.compose.material.icons.rounded.Warning
import androidx.compose.material.icons.rounded.ArrowDownward
import androidx.compose.material.icons.rounded.ArrowUpward
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.HomeWidget
import com.superessence.essabank.core.money.MoneyFormat
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.AnimatedBalance
import com.superessence.essabank.ui.components.Avatar
import com.superessence.essabank.ui.components.BankCard
import com.superessence.essabank.ui.components.DisclaimerFooter
import com.superessence.essabank.ui.components.EssaMark
import com.superessence.essabank.ui.components.GoalIllustration
import com.superessence.essabank.ui.components.LineChart
import com.superessence.essabank.ui.components.LocalPrefs
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.ProgressBar
import com.superessence.essabank.ui.components.QuickAction
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SimulationPill
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Dimens
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import com.superessence.essabank.ui.components.EmptyState
import androidx.compose.material.icons.rounded.Inbox

@Composable
fun HomeScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val now by vm.now.collectAsStateWithLifecycle()
    val c = Essa.colors
    val lang = LocalLang.current
    val overview = remember(s, now) { vm.analytics.overview(s) }
    val photo = rememberProfilePhoto(s.profile.photoPath)

    LazyColumn(
        Modifier.fillMaxSize().background(c.background),
        contentPadding = PaddingValues(start = Dimens.screen, end = Dimens.screen, bottom = 120.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item {
            Row(Modifier.fillMaxWidth().statusBarsPadding().padding(top = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.pressable { nav.tab(Routes.PROFILE) }) { Avatar(s.profile.initials, s.profile.avatar, 44.dp, photo) }
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(greeting(lang, java.time.LocalTime.now().hour) + ", " + s.profile.firstName, style = EssaType.h3, color = c.text, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        EssaMark(14.dp, tile = false)
                        Spacer(Modifier.width(4.dp))
                        Text(L.tierName(s.profile.tier), style = EssaType.caption, color = c.accent)
                    }
                }
                RoundIconButton(
                    if (s.profile.preferences.hideBalances) Icons.Rounded.VisibilityOff else Icons.Rounded.Visibility,
                    tr("Masquer les montants", "Hide amounts"),
                    { vm.run { updatePreferences(it) { p -> p.copy(hideBalances = !p.hideBalances) } } },
                )
                Spacer(Modifier.width(8.dp))
                RoundIconButton(Icons.Rounded.Notifications, tr("Notifications", "Notifications"), { nav.go(Routes.NOTIFICATIONS) }, badge = s.unreadNotifications)
            }
        }
        for (w in s.profile.preferences.homeWidgets) {
            when (w) {
                HomeWidget.BALANCE -> item(key = "balance") { BalanceHero(s, overview, nav) }
                HomeWidget.QUICK_ACTIONS -> item(key = "quick") { QuickActions(nav) }
                HomeWidget.INSIGHTS -> item(key = "insights") { InsightBanner(vm, s, nav) }
                HomeWidget.WEALTH -> item(key = "wealth") { WealthPanel(vm, s, now) }
                HomeWidget.ACCOUNTS -> item(key = "accounts") { AccountsCarousel(s, nav) }
                HomeWidget.CARDS -> item(key = "cards") { CardsPreview(s, nav) }
                HomeWidget.BUDGET -> item(key = "budget") { BudgetPreview(vm, s, nav) }
                HomeWidget.GOALS -> item(key = "goals") { GoalsPreview(s, nav) }
                HomeWidget.TRANSACTIONS -> item(key = "tx") { RecentTransactions(vm, s, nav) }
            }
        }
        item { DisclaimerFooter() }
    }
}

private fun greeting(lang: com.superessence.essabank.core.money.AppLanguage, hour: Int): String {
    val fr = lang == com.superessence.essabank.core.money.AppLanguage.FR
    return when (hour) {
        in 5..11 -> if (fr) "Bonjour" else "Good morning"
        in 12..17 -> if (fr) "Bon après-midi" else "Good afternoon"
        else -> if (fr) "Bonsoir" else "Good evening"
    }
}

@Composable
private fun BalanceHero(s: BankState, o: com.superessence.essabank.core.engine.Overview, nav: Nav) {
    val c = Essa.colors
    Box(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(28.dp)).background(c.hero).pressable { nav.go(Routes.ACCOUNTS) },
    ) {
        Canvas(Modifier.matchParentSize()) {
            for (i in 1..5) drawCircle(Color.White.copy(alpha = 0.045f), size.height * 0.33f * i, Offset(size.width * 0.95f, size.height * 0.05f), style = Stroke(1.5f))
            drawCircle(Brand.Cyan.copy(alpha = 0.10f), size.height * 0.55f, Offset(size.width * 0.05f, size.height * 1.1f))
        }
        Column(Modifier.padding(22.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(tr("Solde principal", "Main balance"), style = EssaType.bodyStrong, color = Color.White.copy(alpha = 0.75f), modifier = Modifier.weight(1f))
                SimulationPill(text = tr("UNITÉS VIRTUELLES", "VIRTUAL UNITS"))
            }
            Spacer(Modifier.height(6.dp))
            AnimatedBalance(o.primaryBalance, color = Color.White)
            Text(
                tr("Disponible ", "Available ") + fmt(o.primaryAvailable) + "  ·  " + (s.primaryAccount?.name ?: ""),
                style = EssaType.small, color = Color.White.copy(alpha = 0.7f), maxLines = 1, overflow = TextOverflow.Ellipsis,
            )
            Spacer(Modifier.height(18.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                HeroStat(Icons.Rounded.ArrowDownward, tr("Revenus du mois", "Income this month"), fmt(o.monthIncome), Brand.Success, Modifier.weight(1f))
                HeroStat(Icons.Rounded.ArrowUpward, tr("Dépenses du mois", "Spending this month"), fmt(o.monthSpending), Color(0xFFFF8FA0), Modifier.weight(1f))
            }
            Spacer(Modifier.height(12.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(tr("Patrimoine virtuel total", "Total virtual net worth"), style = EssaType.small, color = Color.White.copy(alpha = 0.7f), modifier = Modifier.weight(1f))
                Text(fmt(o.netWorth), style = EssaType.bodyStrong, color = Color.White)
            }
        }
    }
}

@Composable
private fun HeroStat(icon: androidx.compose.ui.graphics.vector.ImageVector, label: String, value: String, tint: Color, modifier: Modifier) {
    Column(modifier.clip(RoundedCornerShape(16.dp)).background(Color.White.copy(alpha = 0.10f)).padding(12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.size(22.dp).clip(RoundedCornerShape(7.dp)).background(tint.copy(alpha = 0.22f)), contentAlignment = Alignment.Center) {
                Icon(icon, null, tint = tint, modifier = Modifier.size(14.dp))
            }
            Spacer(Modifier.width(6.dp))
            Text(label, style = EssaType.small, color = Color.White.copy(alpha = 0.72f), maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        Spacer(Modifier.height(6.dp))
        Text(value, style = EssaType.amount.copy(fontWeight = FontWeight.Bold), color = Color.White, maxLines = 1)
    }
}

@Composable
private fun QuickActions(nav: Nav) {
    Column(verticalArrangement = Arrangement.spacedBy(14.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            QuickAction(Icons.Rounded.SwapHoriz, tr("Virer", "Send"), { nav.go(Routes.transfer("beneficiary")) }, Modifier.weight(1f), highlight = true)
            QuickAction(Icons.AutoMirrored.Rounded.CallReceived, tr("Recevoir", "Receive"), { nav.go(Routes.transfer("receive")) }, Modifier.weight(1f))
            QuickAction(Icons.Rounded.AccountBalance, tr("Comptes", "Accounts"), { nav.go(Routes.ACCOUNTS) }, Modifier.weight(1f))
            QuickAction(Icons.AutoMirrored.Rounded.ReceiptLong, tr("Historique", "History"), { nav.go(Routes.history()) }, Modifier.weight(1f))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            QuickAction(Icons.Rounded.Savings, tr("Épargne", "Savings"), { nav.go(Routes.SAVINGS) }, Modifier.weight(1f))
            QuickAction(Icons.AutoMirrored.Rounded.ShowChart, tr("Investir", "Invest"), { nav.go(Routes.INVEST) }, Modifier.weight(1f))
            QuickAction(Icons.Rounded.Assessment, tr("Crédit", "Loans"), { nav.go(Routes.LOANS) }, Modifier.weight(1f))
            QuickAction(Icons.Rounded.CreditCard, tr("Cartes", "Cards"), { nav.tab(Routes.CARDS) }, Modifier.weight(1f))
        }
    }
}

@Composable
private fun InsightBanner(vm: AppViewModel, s: BankState, nav: Nav) {
    val c = Essa.colors
    val lang = LocalLang.current
    val statuses = remember(s) { vm.analytics.budgetStatuses(s) }
    val exceeded = statuses.filter { it.exceeded }
    val o = remember(s) { vm.analytics.overview(s) }
    val (title, text, warn) = if (exceeded.isNotEmpty()) {
        val names = exceeded.joinToString { it.category?.let { cat -> L.category(cat, lang) } ?: tr(lang, "Budget mensuel", "Monthly budget") }
        Triple(tr("Budget dépassé", "Budget exceeded"), names + tr(" : alerte simulée, pensez à ajuster vos dépenses.", ": simulated alert, consider adjusting your spending."), true)
    } else {
        val diff = vm.analytics.change(o.previousMonthSpending, vm.analytics.projectedMonthSpending(s))
        val down = diff.signum() <= 0
        Triple(
            tr("Projection du mois", "Month projection"),
            tr("À ce rythme, vos dépenses atteindront ", "At this pace, your spending will reach ") + fmt(vm.analytics.projectedMonthSpending(s)) +
                tr(" (", " (") + MoneyFormat.percent(diff, lang) + tr(" vs mois dernier).", " vs last month)."),
            !down,
        )
    }
    Panel(onClick = { nav.go(Routes.BUDGETS) }, padding = 14.dp) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.size(40.dp).clip(RoundedCornerShape(13.dp)).background((if (warn) c.warning else c.accent).copy(alpha = 0.15f)), contentAlignment = Alignment.Center) {
                Icon(if (warn) Icons.Rounded.Warning else Icons.AutoMirrored.Rounded.ShowChart, null, tint = if (warn) c.warning else c.accent, modifier = Modifier.size(21.dp))
            }
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(title, style = EssaType.title, color = c.text)
                Text(text, style = EssaType.small, color = c.textMuted)
            }
        }
    }
}

@Composable
private fun WealthPanel(vm: AppViewModel, s: BankState, now: Long) {
    val c = Essa.colors
    val lang = LocalLang.current
    val series = remember(s, now) { vm.analytics.wealthSeries(s, 30) }
    var sel by remember { mutableStateOf<Int?>(null) }
    if (series.isEmpty()) return
    val first = series.first().amount; val last = series.last().amount
    val change = vm.analytics.change(first, last)
    val shown = sel?.let { series.getOrNull(it) } ?: series.last()
    Panel {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(tr("Évolution du patrimoine virtuel", "Virtual net worth trend"), style = EssaType.small, color = c.textMuted)
                Text(fmt(shown.amount), style = EssaType.h2, color = c.text)
                Text(if (sel != null) L.date(shown.date, lang) else tr("30 derniers jours", "Last 30 days"), style = EssaType.small, color = c.textMuted)
            }
            ChangeBadge(MoneyFormat.percent(change, lang), change.signum() >= 0)
        }
        Spacer(Modifier.height(10.dp))
        LineChart(series.map { it.amount.cents.toFloat() }, c.accent, height = 120.dp, selected = sel, onSelect = { sel = it })
    }
}

@Composable
private fun AccountsCarousel(s: BankState, nav: Nav) {
    Column {
        SectionHeader(tr("Mes comptes", "My accounts"), tr("Tout voir", "See all")) { nav.go(Routes.ACCOUNTS) }
        Spacer(Modifier.height(8.dp))
        LazyRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            items(s.accounts.sortedBy { it.sortOrder }, key = { it.id }) { a -> AccountTile(a, { nav.go(Routes.account(a.id)) }) }
            item { NewTile(tr("Nouveau compte", "New account")) { nav.go(Routes.NEW_ACCOUNT) } }
        }
    }
}

@Composable
fun NewTile(label: String, onClick: () -> Unit) {
    val c = Essa.colors
    Column(
        Modifier.width(140.dp).height(150.dp).clip(RoundedCornerShape(22.dp)).border(1.5.dp, c.border, RoundedCornerShape(22.dp)).pressable(onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
    ) {
        Box(Modifier.size(42.dp).clip(RoundedCornerShape(14.dp)).background(c.accentSoft), contentAlignment = Alignment.Center) { Icon(Icons.Rounded.Add, null, tint = c.accent) }
        Spacer(Modifier.height(10.dp))
        Text(label, style = EssaType.small.copy(fontWeight = FontWeight.SemiBold), color = c.textSecondary)
    }
}

@Composable
private fun CardsPreview(s: BankState, nav: Nav) {
    val c = Essa.colors
    val card = s.cards.firstOrNull() ?: return
    Column {
        SectionHeader(tr("Mes cartes", "My cards"), tr("Gérer", "Manage")) { nav.tab(Routes.CARDS) }
        Spacer(Modifier.height(8.dp))
        Box(Modifier.fillMaxWidth().pressable { nav.go(Routes.card(card.id)) }) {
            BankCard(card, elevation = 10.dp)
        }
        if (s.cards.size > 1) {
            Spacer(Modifier.height(8.dp))
            Text(tr("+ ${s.cards.size - 1} autre(s) carte(s) fictive(s)", "+ ${s.cards.size - 1} other fictional card(s)"), style = EssaType.small, color = c.textMuted)
        }
    }
}

@Composable
private fun BudgetPreview(vm: AppViewModel, s: BankState, nav: Nav) {
    val c = Essa.colors
    val status = remember(s) { vm.analytics.budgetStatuses(s).firstOrNull { it.category == null } } ?: return
    Panel(onClick = { nav.go(Routes.BUDGETS) }) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(tr("Budget mensuel", "Monthly budget"), style = EssaType.title, color = c.text, modifier = Modifier.weight(1f))
            Text("${(status.ratio * 100).toInt()} %", style = EssaType.bodyStrong, color = if (status.exceeded) c.negative else c.text)
        }
        Spacer(Modifier.height(10.dp))
        ProgressBar(status.ratio, if (status.exceeded) c.negative else if (status.ratio > 0.8f) c.warning else c.accent)
        Spacer(Modifier.height(8.dp))
        Text(fmt(status.spent) + tr(" dépensés sur ", " spent of ") + fmt(status.budget), style = EssaType.small, color = c.textMuted)
    }
}

@Composable
private fun GoalsPreview(s: BankState, nav: Nav) {
    val c = Essa.colors
    Column {
        SectionHeader(tr("Mes coffres", "My vaults"), tr("Épargne", "Savings")) { nav.go(Routes.SAVINGS) }
        Spacer(Modifier.height(8.dp))
        if (s.goals.isEmpty()) {
            Panel(onClick = { nav.go(Routes.NEW_GOAL) }) { Text(tr("Créer un premier objectif d'épargne", "Create a first savings goal"), style = EssaType.bodyStrong, color = c.accent) }
        } else LazyRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            items(s.goals, key = { it.id }) { g ->
                Column(Modifier.width(200.dp).clip(RoundedCornerShape(22.dp)).background(c.surface).border(1.dp, c.border, RoundedCornerShape(22.dp)).pressable { nav.go(Routes.goal(g.id)) }.padding(12.dp)) {
                    GoalIllustration(g.kind, Brand.color(g.colorIndex), Modifier.fillMaxWidth().height(78.dp))
                    Spacer(Modifier.height(10.dp))
                    Text(g.name, style = EssaType.bodyStrong, color = c.text, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(fmt(g.saved) + " / " + fmt(g.target, decimals = false), style = EssaType.small, color = c.textMuted, maxLines = 1)
                    Spacer(Modifier.height(8.dp))
                    ProgressBar(g.progress, Brand.color(g.colorIndex), height = 6.dp)
                }
            }
        }
    }
}

@Composable
private fun RecentTransactions(vm: AppViewModel, s: BankState, nav: Nav) {
    val recent = remember(s) { s.transactions.sortedWith(compareByDescending<com.superessence.essabank.core.model.Transaction> { it.timestamp }.thenByDescending { it.seq }).take(6) }
    Column {
        SectionHeader(tr("Dernières transactions", "Latest transactions"), tr("Tout voir", "See all")) { nav.go(Routes.history()) }
        Spacer(Modifier.height(4.dp))
        Panel(padding = 8.dp) {
            if (recent.isEmpty()) EmptyState(Icons.Rounded.Inbox, tr("Aucune opération", "No operation yet"), tr("Vos opérations virtuelles apparaîtront ici.", "Your virtual operations will appear here."))
            recent.forEach { tx -> TransactionRow(tx, s, vm.zone, { nav.go(Routes.tx(tx.id)) }, showAccount = true) }
        }
    }
}

/** Photo de profil locale (chargée depuis le stockage privé de l'application). */
@Composable
fun rememberProfilePhoto(path: String?): androidx.compose.ui.graphics.ImageBitmap? = remember(path) {
    path?.let { p ->
        try { android.graphics.BitmapFactory.decodeFile(p)?.asImageBitmap() } catch (e: Exception) { null }
    }
}
