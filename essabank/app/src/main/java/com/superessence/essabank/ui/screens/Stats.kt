package com.superessence.essabank.ui.screens

import androidx.compose.foundation.background
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
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.ArrowDownward
import androidx.compose.material.icons.rounded.ArrowUpward
import androidx.compose.material.icons.rounded.CreditCard
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.Savings
import androidx.compose.material.icons.rounded.Tune
import androidx.compose.material.icons.rounded.Warning
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
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
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.model.Category
import com.superessence.essabank.core.model.NotificationType
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.MoneyFormat
import com.superessence.essabank.core.money.ParseResult
import com.superessence.essabank.core.money.sumOfMoney
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.BarChart
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.DonutChart
import com.superessence.essabank.ui.components.EssaSheet
import com.superessence.essabank.ui.components.EssaTextField
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.LegendDot
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.ProgressBar
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.SegmentedControl
import com.superessence.essabank.ui.components.StatTile
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Dimens
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import java.time.DayOfWeek
import java.time.LocalDate
import java.time.temporal.TemporalAdjusters

@Composable
fun StatsScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val a = vm.analytics
    var period by remember { mutableIntStateOf(1) }
    var selectedCat by remember { mutableStateOf<Int?>(null) }
    val today = LocalDate.now(vm.zone)
    val start = when (period) {
        0 -> today.with(TemporalAdjusters.previousOrSame(DayOfWeek.MONDAY))
        1 -> today.withDayOfMonth(1)
        else -> today.withDayOfYear(1)
    }
    val flow = remember(s, period) { a.flow(s, start, today) }
    val cats = remember(s, period) { a.categoryBreakdown(s, start, today) }
    val daily = remember(s, period) { a.dailySpending(s, if (period == 0) 7 else 30) }
    val monthly = remember(s) { a.monthlyFlows(s, 6) }
    val weekly = remember(s) { a.weeklyFlows(s, 8) }
    val advanced = s.profile.tier.advancedStats
    val net = flow.income - flow.spending
    val rate = if (flow.income.isPositive) (net.cents * 100 / flow.income.cents).toInt() else 0
    val avg = if (daily.isNotEmpty()) Money(daily.sumOf { it.amount.cents } / daily.size) else Money.ZERO

    LazyColumn(
        Modifier.fillMaxSize().background(c.background),
        contentPadding = PaddingValues(start = Dimens.screen, end = Dimens.screen, bottom = 120.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item {
            Row(Modifier.statusBarsPadding().padding(top = 14.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(tr("Statistiques", "Insights"), style = EssaType.h1, color = c.text)
                    Text(tr("Analyse de vos opérations virtuelles", "Analysis of your virtual operations"), style = EssaType.small, color = c.textMuted)
                }
                RoundIconButton(Icons.Rounded.Tune, tr("Budgets", "Budgets"), { nav.go(Routes.BUDGETS) })
            }
        }
        item { SegmentedControl(listOf(tr("Semaine", "Week"), tr("Mois", "Month"), tr("Année", "Year")), period, { period = it; selectedCat = null }) }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                StatTile(tr("Dépenses", "Spending"), fmt(flow.spending), Modifier.weight(1f), icon = Icons.Rounded.ArrowUpward, iconTint = c.negative)
                StatTile(tr("Revenus", "Income"), fmt(flow.income), Modifier.weight(1f), icon = Icons.Rounded.ArrowDownward, iconTint = c.positive)
            }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                StatTile(tr("Épargne nette", "Net savings"), fmt(net, signed = true), Modifier.weight(1f), valueColor = if (net.isNegative) c.negative else c.positive, icon = Icons.Rounded.Savings)
                StatTile(tr("Taux d'épargne", "Savings rate"), "$rate %", Modifier.weight(1f))
            }
        }
        item {
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(tr("Dépenses quotidiennes", "Daily spending"), style = EssaType.title, color = c.text)
                        Text(tr("Moyenne : ", "Average: ") + fmt(avg) + tr(" / jour", " / day"), style = EssaType.small, color = c.textMuted)
                    }
                }
                Spacer(Modifier.height(12.dp))
                val labels = daily.mapIndexed { i, d -> if (daily.size <= 7) L.weekdayInitial(d.date, lang) else if (i % 5 == 0 || i == daily.lastIndex) d.date.dayOfMonth.toString() else "" }
                BarChart(daily.map { it.amount.cents.toFloat() }, labels, c.accent, limit = avg.cents.toFloat(), height = 140.dp)
            }
        }
        item {
            Panel {
                Text(tr("Catégories les plus coûteuses", "Top spending categories"), style = EssaType.title, color = c.text)
                Spacer(Modifier.height(14.dp))
                if (cats.isEmpty()) Text(tr("Aucune dépense sur la période.", "No spending in this period."), style = EssaType.body, color = c.textMuted)
                else {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        DonutChart(cats.map { it.share to L.categoryColor(it.category) }, 140.dp, selected = selectedCat) {
                            val sel = selectedCat?.let { cats.getOrNull(it) }
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                Text(sel?.let { L.category(it.category, lang) } ?: tr("Total", "Total"), style = EssaType.small, color = c.textMuted)
                                Text(fmt(sel?.amount ?: flow.spending, decimals = false), style = EssaType.bodyStrong, color = c.text)
                            }
                        }
                        Spacer(Modifier.width(14.dp))
                        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            cats.take(5).forEachIndexed { i, cs ->
                                Box(Modifier.clip(RoundedCornerShape(8.dp)).pressable { selectedCat = if (selectedCat == i) null else i }) {
                                    LegendDot(L.categoryColor(cs.category), L.category(cs.category, lang) + " · " + (cs.share * 100).toInt() + " %")
                                }
                            }
                        }
                    }
                    Spacer(Modifier.height(12.dp))
                    cats.forEach { cs ->
                        ListRow(L.category(cs.category, lang), tr("${cs.count} opération(s)", "${cs.count} operation(s)"),
                            leading = { IconBadge(L.categoryIcon(cs.category), L.categoryColor(cs.category), 38.dp) },
                            trailing = { Text(fmt(cs.amount), style = EssaType.amount, color = c.text) },
                            onClick = { nav.go(Routes.history(category = cs.category.name)) }, chevron = false)
                    }
                }
            }
        }
        item {
            Panel {
                Text(tr("Comparaison mensuelle", "Monthly comparison"), style = EssaType.title, color = c.text)
                val cur = monthly.last(); val prev = monthly.getOrNull(monthly.size - 2)
                if (prev != null) {
                    val diff = a.change(prev.spending, cur.spending)
                    Text(tr("Dépenses ", "Spending ") + MoneyFormat.percent(diff, lang) + tr(" par rapport au mois précédent", " compared with last month"),
                        style = EssaType.small, color = if (diff.signum() > 0) c.negative else c.positive)
                }
                Spacer(Modifier.height(12.dp))
                BarChart(monthly.map { it.spending.cents.toFloat() }, monthly.map { L.monthShort(it.start, lang) }, c.accent, secondary = monthly.map { it.income.cents.toFloat() })
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) { LegendDot(c.accent, tr("Dépenses", "Spending")); LegendDot(c.positive, tr("Revenus", "Income")) }
            }
        }
        if (advanced) {
            item {
                Panel {
                    Text(tr("Évolution hebdomadaire", "Weekly trend"), style = EssaType.title, color = c.text)
                    Text(tr("8 dernières semaines · statistiques avancées", "Last 8 weeks · advanced insights"), style = EssaType.small, color = c.textMuted)
                    Spacer(Modifier.height(12.dp))
                    BarChart(weekly.map { it.spending.cents.toFloat() }, weekly.map { L.shortDate(it.start, lang).substringBefore(' ') }, c.accent, secondary = weekly.map { it.income.cents.toFloat() })
                }
            }
            item {
                Panel {
                    Text(tr("Principales dépenses du mois", "Largest expenses this month"), style = EssaType.title, color = c.text)
                    Spacer(Modifier.height(4.dp))
                    a.topExpenses(s, 5).forEach { tx -> TransactionRow(tx, s, vm.zone, { nav.go(Routes.tx(tx.id)) }, showAccount = true) }
                    Spacer(Modifier.height(6.dp))
                    KeyValue2(tr("Projection fin de mois", "End-of-month projection"), fmt(a.projectedMonthSpending(s)))
                    KeyValue2(tr("Dépense moyenne par jour", "Average daily spending"), fmt(a.averageDailySpending(s)))
                    KeyValue2(tr("Épargne en coffres", "Savings in vaults"), fmt(s.goals.sumOfMoney { it.saved }))
                }
            }
        } else item {
            Panel(onClick = { nav.go(Routes.PLANS) }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconBadge(Icons.Rounded.Lock, c.accent, 42.dp)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(tr("Statistiques avancées", "Advanced insights"), style = EssaType.title, color = c.text)
                        Text(tr("Tendances hebdomadaires, principales dépenses et projections avec ESSA PLUS — gratuit, sans paiement.", "Weekly trends, largest expenses and projections with ESSA PLUS — free, no payment."),
                            style = EssaType.small, color = c.textMuted)
                    }
                }
            }
        }
        item { BudgetSummary(vm, s, nav) }
    }
}

@Composable
private fun KeyValue2(label: String, value: String) = com.superessence.essabank.ui.components.KeyValue(label, value)

@Composable
private fun BudgetSummary(vm: AppViewModel, s: com.superessence.essabank.core.model.BankState, nav: Nav) {
    val c = Essa.colors
    val lang = LocalLang.current
    val statuses = remember(s) { vm.analytics.budgetStatuses(s) }
    Panel(onClick = { nav.go(Routes.BUDGETS) }) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(tr("Budgets du mois", "This month's budgets"), style = EssaType.title, color = c.text, modifier = Modifier.weight(1f))
            Text(tr("Gérer", "Manage"), style = EssaType.bodyStrong, color = c.accent)
        }
        if (statuses.isEmpty()) Text(tr("Définissez un budget mensuel pour recevoir des alertes simulées.", "Set a monthly budget to receive simulated alerts."), style = EssaType.small, color = c.textMuted)
        statuses.take(4).forEach { b ->
            Spacer(Modifier.height(10.dp))
            Row {
                Text(b.category?.let { L.category(it, lang) } ?: tr("Global", "Overall"), style = EssaType.small.copy(fontWeight = FontWeight.SemiBold), color = c.textSecondary, modifier = Modifier.weight(1f))
                Text(fmt(b.spent, decimals = false) + " / " + fmt(b.budget, decimals = false), style = EssaType.small, color = if (b.exceeded) c.negative else c.textMuted)
            }
            Spacer(Modifier.height(4.dp))
            ProgressBar(b.ratio, if (b.exceeded) c.negative else if (b.ratio > 0.8f) c.warning else b.category?.let { L.categoryColor(it) } ?: c.accent, height = 6.dp)
        }
    }
}

// ================================================================ Budgets

@Composable
fun BudgetsScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    val statuses = remember(s) { vm.analytics.budgetStatuses(s) }
    var editing by remember { mutableStateOf<Category?>(null) }
    var editingGlobal by remember { mutableStateOf(false) }
    var addCat by remember { mutableStateOf(false) }
    val alerts = s.notifications.filter { it.type == NotificationType.BUDGET_EXCEEDED || it.type == NotificationType.BUDGET_WARNING }.take(6)
    val global = statuses.firstOrNull { it.category == null }

    ScreenScaffold(tr("Budgets et limites", "Budgets and limits"), nav::back) {
        item {
            Panel {
                Text(tr("Budget mensuel global", "Overall monthly budget"), style = EssaType.small, color = c.textMuted)
                if (global != null) {
                    Text(fmt(global.spent) + " / " + fmt(global.budget), style = EssaType.h2, color = if (global.exceeded) c.negative else c.text)
                    Spacer(Modifier.height(10.dp))
                    ProgressBar(global.ratio, if (global.exceeded) c.negative else if (global.ratio > 0.8f) c.warning else c.accent, height = 10.dp)
                    Spacer(Modifier.height(8.dp))
                    Text(if (global.exceeded) tr("Budget dépassé de ", "Budget exceeded by ") + fmt(-global.remaining) else tr("Reste ", "Remaining ") + fmt(global.remaining) +
                        tr(" · projection : ", " · projection: ") + fmt(vm.analytics.projectedMonthSpending(s)), style = EssaType.small, color = c.textMuted)
                } else Text(tr("Aucun budget global défini.", "No overall budget set."), style = EssaType.title, color = c.text)
                Spacer(Modifier.height(12.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    SecondaryButton(if (global == null) tr("Définir", "Set") else tr("Modifier", "Edit"), { editingGlobal = true }, Modifier.weight(1f), icon = Icons.Rounded.Edit)
                    if (global != null) SecondaryButton(tr("Retirer", "Remove"), { vm.run { setMonthlyBudget(it, null) } }, Modifier.weight(1f), icon = Icons.Rounded.Delete, danger = true)
                }
            }
        }
        item { SectionHeader(tr("Budgets par catégorie", "Budgets by category"), tr("+ Ajouter", "+ Add")) { addCat = true } }
        val catStatuses = statuses.filter { it.category != null }
        if (catStatuses.isEmpty()) item { Text(tr("Ajoutez un budget pour une catégorie (alimentation, loisirs…).", "Add a budget for a category (groceries, leisure…)."), style = EssaType.body, color = c.textMuted) }
        items(catStatuses, key = { it.category!!.name }) { b ->
            val cat = b.category!!
            Panel(onClick = { editing = cat }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconBadge(L.categoryIcon(cat), L.categoryColor(cat), 40.dp)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(L.category(cat, lang), style = EssaType.title, color = c.text)
                        Text(fmt(b.spent) + " / " + fmt(b.budget), style = EssaType.small, color = if (b.exceeded) c.negative else c.textMuted)
                    }
                    if (b.exceeded) Icon(Icons.Rounded.Warning, null, tint = c.negative)
                    else Text("${(b.ratio * 100).toInt()} %", style = EssaType.bodyStrong, color = c.textSecondary)
                }
                Spacer(Modifier.height(10.dp))
                ProgressBar(b.ratio, if (b.exceeded) c.negative else if (b.ratio > 0.8f) c.warning else L.categoryColor(cat))
            }
        }
        item { SectionHeader(tr("Limites de dépenses virtuelles", "Virtual spending limits")) }
        item {
            Panel(padding = 8.dp) {
                s.cards.forEach { card ->
                    val spent = vm.engine.cardSpentThisMonth(s, card.id)
                    ListRow(card.label + " " + card.maskedNumber, fmt(spent) + " / " + fmt(card.monthlyLimit) + tr(" ce mois-ci", " this month"),
                        leading = { IconBadge(Icons.Rounded.CreditCard, c.accent, 38.dp) }, onClick = { nav.go(Routes.card(card.id)) })
                }
            }
        }
        item {
            Panel(onClick = { nav.go(Routes.SAVINGS) }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconBadge(Icons.Rounded.Savings, c.positive, 40.dp)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(tr("Objectifs d'épargne", "Savings goals"), style = EssaType.title, color = c.text)
                        Text(tr("${s.goals.size} coffre(s) · ", "${s.goals.size} vault(s) · ") + fmt(s.goals.sumOfMoney { it.saved }) + tr(" épargnés", " saved"), style = EssaType.small, color = c.textMuted)
                    }
                }
            }
        }
        item { SectionHeader(tr("Alertes simulées récentes", "Recent simulated alerts")) }
        item {
            Panel(padding = 8.dp) {
                if (alerts.isEmpty()) Text(tr("Aucune alerte : vos budgets sont respectés.", "No alert: your budgets are on track."), style = EssaType.body, color = c.textMuted, modifier = Modifier.padding(10.dp))
                alerts.forEach { n ->
                    ListRow(n.title, n.body + " · " + L.relative(n.timestamp, System.currentTimeMillis(), vm.zone, lang),
                        leading = { IconBadge(Icons.Rounded.Warning, if (n.type == NotificationType.BUDGET_EXCEEDED) c.negative else c.warning, 38.dp) })
                }
            }
        }
        item {
            Text(tr("Une alerte est émise à 80 % puis à 100 % de chaque budget, une seule fois par mois. Les alertes sont simulées dans l'application.",
                "An alert is issued at 80% then 100% of each budget, once per month. Alerts are simulated inside the app."), style = EssaType.small, color = c.textMuted)
        }
    }

    if (editingGlobal) BudgetSheet(tr("Budget mensuel global", "Overall monthly budget"), s.budgets.monthly, { editingGlobal = false }, onRemove = null) { amount ->
        vm.run(onOk = { editingGlobal = false }) { setMonthlyBudget(it, amount) }
    }
    editing?.let { cat ->
        BudgetSheet(L.category(cat, lang), s.budgets.categories[cat], { editing = null }, onRemove = { vm.run(onOk = { editing = null }) { setCategoryBudget(it, cat, null) } }) { amount ->
            vm.run(onOk = { editing = null }) { setCategoryBudget(it, cat, amount) }
        }
    }
    if (addCat) {
        EssaSheet({ addCat = false }) {
            Text(tr("Choisir une catégorie", "Choose a category"), style = EssaType.h2, color = c.text)
            Spacer(Modifier.height(10.dp))
            Category.spending.filter { it !in s.budgets.categories.keys }.forEach { cat ->
                ListRow(L.category(cat, lang), null, leading = { IconBadge(L.categoryIcon(cat), L.categoryColor(cat), 38.dp) }, onClick = { addCat = false; editing = cat })
            }
        }
    }
}

@Composable
private fun BudgetSheet(title: String, current: Money?, onDismiss: () -> Unit, onRemove: (() -> Unit)?, onSave: (Money) -> Unit) {
    val c = Essa.colors
    var text by remember { mutableStateOf(current?.toBigDecimal()?.stripTrailingZeros()?.toPlainString()?.replace('.', ',') ?: "") }
    val parsed = Money.parse(text)
    EssaSheet(onDismiss) {
        Text(title, style = EssaType.h2, color = c.text)
        Spacer(Modifier.height(12.dp))
        EssaTextField(text, { text = it }, tr("Montant mensuel (€)", "Monthly amount (€)"), keyboard = KeyboardType.Decimal, maxLength = 12,
            isError = text.isNotEmpty() && parsed !is ParseResult.Valid)
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(100, 250, 500, 1000, 2500).forEach { v -> Chip("$v", text == "$v") { text = "$v" } }
        }
        Spacer(Modifier.height(16.dp))
        PrimaryButton(tr("Enregistrer", "Save"), enabled = parsed is ParseResult.Valid, onClick = { (parsed as? ParseResult.Valid)?.let { onSave(it.money) } })
        if (onRemove != null && current != null) {
            Spacer(Modifier.height(8.dp))
            SecondaryButton(tr("Supprimer ce budget", "Delete this budget"), onRemove, Modifier.fillMaxWidth(), danger = true, icon = Icons.Rounded.Delete)
        }
    }
}
