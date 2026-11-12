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
import androidx.compose.material.icons.rounded.Bookmark
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Notifications
import androidx.compose.material.icons.rounded.DoneAll
import androidx.compose.material.icons.rounded.DeleteSweep
import androidx.compose.material3.Icon
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.compositeOver
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.essabank.core.loan.LoanCalculator
import com.superessence.essabank.core.loan.LoanProfile
import com.superessence.essabank.core.model.LoanSimulation
import com.superessence.essabank.core.model.LoanType
import com.superessence.essabank.core.model.NotificationType
import com.superessence.essabank.core.money.Money
import com.superessence.essabank.core.money.sumOfMoney
import com.superessence.essabank.ui.AppViewModel
import com.superessence.essabank.ui.Nav
import com.superessence.essabank.ui.Routes
import com.superessence.essabank.ui.components.Chip
import com.superessence.essabank.ui.components.ConfirmDialog
import com.superessence.essabank.ui.components.DonutChart
import com.superessence.essabank.ui.components.EmptyState
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.KeyValue
import com.superessence.essabank.ui.components.LegendDot
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.Panel
import com.superessence.essabank.ui.components.PrimaryButton
import com.superessence.essabank.ui.components.RoundIconButton
import com.superessence.essabank.ui.components.ScreenScaffold
import com.superessence.essabank.ui.components.SecondaryButton
import com.superessence.essabank.ui.components.SectionHeader
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType

@Composable
fun LoansScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    var type by remember { mutableStateOf(LoanType.PERSONAL) }
    val profile = LoanProfile.of(type)
    var principal by remember { mutableFloatStateOf(profile.defaultPrincipal.cents / 100f) }
    var months by remember { mutableFloatStateOf(profile.defaultMonths.toFloat()) }
    var rate by remember { mutableFloatStateOf(profile.defaultRateBp.toFloat()) }
    var insurance by remember { mutableFloatStateOf(profile.defaultInsuranceBp.toFloat()) }
    fun select(t: LoanType) {
        val d = LoanProfile.of(t)
        type = t; principal = d.defaultPrincipal.cents / 100f; months = d.defaultMonths.toFloat()
        rate = d.defaultRateBp.toFloat(); insurance = d.defaultInsuranceBp.toFloat()
    }
    var showAll by remember { mutableStateOf(false) }
    var yearly by remember { mutableStateOf(true) }
    val step = if (type == LoanType.HOME) 5000 else 500
    val p = Money.units((principal / step).toLong() * step)
    val m = months.toInt()
    val r = (rate / 5).toInt() * 5
    val ins = insurance.toInt()
    val result = remember(p, m, r, ins) { LoanCalculator.compute(p, r, m, ins) }
    val savedMsg = tr("Simulation enregistrée.", "Simulation saved.")
    val pct = { bp: Int -> "%.2f %%".format(bp / 100.0) }

    ScreenScaffold(tr("Simulateur de crédit", "Loan simulator"), nav::back) {
        item {
            Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(c.accentSoft).padding(12.dp), verticalAlignment = Alignment.Top) {
                Icon(Icons.Rounded.Info, null, tint = c.accent)
                Spacer(Modifier.width(10.dp))
                Text(tr("Simulation pédagogique à taux fictifs. ESSABANK n'accorde aucun crédit et ne collecte aucune information financière.",
                    "Educational simulation with fictional rates. ESSABANK grants no loan and collects no financial information."), style = EssaType.small, color = c.text)
            }
        }
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                LoanType.entries.forEach { t ->
                    Column(
                        Modifier.weight(1f).clip(RoundedCornerShape(16.dp)).background(if (t == type) c.accent else c.surface).pressable { select(t) }.padding(vertical = 12.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                    ) {
                        Icon(L.loanIcon(t), null, tint = if (t == type) c.onAccent else c.accent)
                        Spacer(Modifier.height(4.dp))
                        Text(L.loanType(t, lang), style = EssaType.small.copy(fontWeight = FontWeight.SemiBold), color = if (t == type) c.onAccent else c.textSecondary, maxLines = 1)
                    }
                }
            }
        }
        item {
            Panel {
                LoanSlider(tr("Capital emprunté", "Amount borrowed"), fmt(p, hideable = false, decimals = false), principal, profile.minPrincipal.cents / 100f..profile.maxPrincipal.cents / 100f) { principal = it }
                LoanSlider(tr("Durée", "Duration"), if (m % 12 == 0) tr("${m / 12} an(s)", "${m / 12} year(s)") else tr("$m mois", "$m months"), months, profile.minMonths.toFloat()..profile.maxMonths.toFloat()) { months = it }
                LoanSlider(tr("Taux annuel fictif", "Fictional annual rate"), pct(r), rate, profile.minRateBp.toFloat()..profile.maxRateBp.toFloat()) { rate = it }
                LoanSlider(tr("Assurance fictive", "Fictional insurance"), pct(ins), insurance, 0f..100f) { insurance = it }
            }
        }
        item {
            Panel {
                Text(tr("Mensualité", "Monthly payment"), style = EssaType.small, color = c.textMuted, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                Text(fmt(result.monthlyTotal, hideable = false), style = EssaType.amountLarge, color = c.text, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                Text(tr("dont assurance ", "including insurance ") + fmt(result.monthlyInsurance, hideable = false), style = EssaType.small, color = c.textMuted, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                Spacer(Modifier.height(14.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    DonutChart(listOf(result.principal.cents.toFloat() to c.accent, result.totalInterest.cents.toFloat() to c.warning, result.totalInsurance.cents.toFloat() to c.positive), 110.dp, 14.dp)
                    Spacer(Modifier.width(14.dp))
                    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        LegendDot(c.accent, tr("Capital ", "Principal ") + fmt(result.principal, hideable = false, decimals = false))
                        LegendDot(c.warning, tr("Intérêts ", "Interest ") + fmt(result.totalInterest, hideable = false, decimals = false))
                        LegendDot(c.positive, tr("Assurance ", "Insurance ") + fmt(result.totalInsurance, hideable = false, decimals = false))
                    }
                }
                Spacer(Modifier.height(10.dp))
                KeyValue(tr("Taux fictif", "Fictional rate"), pct(r))
                KeyValue(tr("Durée", "Duration"), tr("$m mois", "$m months"))
                KeyValue(tr("Coût total du crédit", "Total cost of credit"), fmt(result.totalCost, hideable = false))
                KeyValue(tr("Montant total remboursé", "Total repaid"), fmt(result.totalRepaid, hideable = false))
                Spacer(Modifier.height(10.dp))
                SecondaryButton(tr("Enregistrer la simulation", "Save simulation"), {
                    vm.run(savedMsg) { saveLoan(it, LoanSimulation("", type, p, r, m, ins, 0, L.loanType(type, lang))) }
                }, Modifier.fillMaxWidth(), icon = Icons.Rounded.Bookmark)
            }
        }
        item {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f)) { SectionHeader(tr("Tableau d'amortissement", "Amortization schedule")) }
                Chip(tr("Par année", "Yearly"), yearly) { yearly = true }
                Spacer(Modifier.width(6.dp))
                Chip(tr("Par mois", "Monthly"), !yearly) { yearly = false }
            }
        }
        item {
            Panel(padding = 12.dp) {
                Row(Modifier.fillMaxWidth().padding(bottom = 6.dp)) {
                    listOf(if (yearly) tr("Année", "Year") else tr("Mois", "Month"), tr("Intérêts", "Interest"), tr("Capital", "Principal"), tr("Restant", "Remaining")).forEachIndexed { i, h ->
                        Text(h, style = EssaType.caption, color = c.textMuted, modifier = Modifier.weight(if (i == 0) 0.6f else 1f), textAlign = if (i == 0) TextAlign.Start else TextAlign.End)
                    }
                }
                val rows: List<List<String>> = if (yearly) {
                    result.schedule.chunked(12).mapIndexed { y, chunk ->
                        listOf("${y + 1}", fmt(chunk.sumOfMoney { it.interest }, hideable = false, decimals = false), fmt(chunk.sumOfMoney { it.principal }, hideable = false, decimals = false), fmt(chunk.last().remaining, hideable = false, decimals = false))
                    }
                } else result.schedule.map { row ->
                    listOf("${row.index}", fmt(row.interest, hideable = false), fmt(row.principal, hideable = false), fmt(row.remaining, hideable = false, decimals = false))
                }
                val visible = if (showAll) rows else rows.take(12)
                visible.forEach { cells ->
                    Row(Modifier.fillMaxWidth().padding(vertical = 5.dp)) {
                        cells.forEachIndexed { i, v ->
                            Text(v, style = EssaType.small.copy(fontFeatureSettings = "tnum"), color = if (i == 0) c.textSecondary else c.text,
                                modifier = Modifier.weight(if (i == 0) 0.6f else 1f), textAlign = if (i == 0) TextAlign.Start else TextAlign.End, maxLines = 1)
                        }
                    }
                }
                if (rows.size > 12) {
                    Spacer(Modifier.height(6.dp))
                    Text(if (showAll) tr("Réduire", "Show less") else tr("Afficher les ${rows.size} lignes", "Show all ${rows.size} rows"), style = EssaType.bodyStrong, color = c.accent,
                        modifier = Modifier.clip(RoundedCornerShape(8.dp)).pressable { showAll = !showAll }.padding(6.dp))
                }
            }
        }
        if (s.loans.isNotEmpty()) {
            item { SectionHeader(tr("Simulations enregistrées", "Saved simulations")) }
            items(s.loans, key = { it.id }) { sim ->
                val res = remember(sim.id) { LoanCalculator.compute(sim.principal, sim.rateBp, sim.months, sim.insuranceBp) }
                Panel(padding = 8.dp) {
                    ListRow(L.loanType(sim.type, lang) + " · " + fmt(sim.principal, hideable = false, decimals = false),
                        "${sim.months} " + tr("mois", "months") + " · " + pct(sim.rateBp) + " · " + fmt(res.monthlyTotal, hideable = false) + tr("/mois", "/month"),
                        leading = { IconBadge(L.loanIcon(sim.type), c.accent, 40.dp) },
                        trailing = { RoundIconButton(Icons.Rounded.Delete, tr("Supprimer", "Delete"), { vm.run { deleteLoan(it, sim.id) } }, tint = c.negative, size = 36.dp) },
                        onClick = {
                            type = sim.type; principal = sim.principal.cents / 100f; months = sim.months.toFloat()
                            rate = sim.rateBp.toFloat(); insurance = sim.insuranceBp.toFloat()
                        }, chevron = false)
                }
            }
        }
    }
}

@Composable
private fun LoanSlider(label: String, value: String, current: Float, range: ClosedFloatingPointRange<Float>, onChange: (Float) -> Unit) {
    val c = Essa.colors
    Column(Modifier.padding(vertical = 4.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(label, style = EssaType.body, color = c.textSecondary, modifier = Modifier.weight(1f))
            Text(value, style = EssaType.bodyStrong, color = c.text)
        }
        Slider(current.coerceIn(range.start, range.endInclusive), onChange, valueRange = range,
            colors = SliderDefaults.colors(thumbColor = c.accent, activeTrackColor = c.accent, inactiveTrackColor = c.surfaceAlt))
    }
}

// ================================================================ Notifications

@Composable
fun NotificationsScreen(vm: AppViewModel, nav: Nav) {
    val s = vm.state.collectAsStateWithLifecycle().value ?: return
    val c = Essa.colors
    val lang = LocalLang.current
    var filter by remember { mutableIntStateOf(0) }
    var clear by remember { mutableStateOf(false) }
    val now = System.currentTimeMillis()
    val list = s.notifications.sortedByDescending { it.timestamp }.filter {
        when (filter) {
            1 -> !it.read
            2 -> it.type in listOf(NotificationType.PAYMENT, NotificationType.TRANSFER_RECEIVED, NotificationType.TRANSFER_SENT, NotificationType.NEW_TRANSACTION, NotificationType.SCHEDULED_FAILED)
            3 -> it.type in listOf(NotificationType.BUDGET_EXCEEDED, NotificationType.BUDGET_WARNING, NotificationType.GOAL_REACHED, NotificationType.MONTHLY_SUMMARY)
            else -> true
        }
    }
    ScreenScaffold(tr("Notifications", "Notifications"), nav::back, actions = {
        RoundIconButton(Icons.Rounded.DoneAll, tr("Tout marquer comme lu", "Mark all as read"), { vm.run { markAllNotificationsRead(it) } })
        Spacer(Modifier.width(8.dp))
        RoundIconButton(Icons.Rounded.DeleteSweep, tr("Tout effacer", "Clear all"), { clear = true })
        Spacer(Modifier.width(8.dp))
    }) {
        item {
            com.superessence.essabank.ui.components.ChipRow(listOf(tr("Toutes", "All"), tr("Non lues (${s.unreadNotifications})", "Unread (${s.unreadNotifications})"),
                tr("Opérations", "Operations"), tr("Budget et épargne", "Budget & savings")), filter, { filter = it })
        }
        if (list.isEmpty()) item { EmptyState(Icons.Rounded.Notifications, tr("Rien de nouveau", "Nothing new"), tr("Les alertes simulées de vos opérations apparaîtront ici.", "Simulated alerts for your operations will appear here.")) }
        items(list, key = { it.id }) { n ->
            Panel(padding = 12.dp, onClick = {
                vm.run { markNotificationRead(it, n.id) }
                val ref = n.refId
                when {
                    ref == null -> Unit
                    s.transactions.any { it.id == ref } -> nav.go(Routes.tx(ref))
                    s.goal(ref) != null -> nav.go(Routes.goal(ref))
                    s.card(ref) != null -> nav.go(Routes.card(ref))
                    n.type == NotificationType.BUDGET_EXCEEDED || n.type == NotificationType.BUDGET_WARNING -> nav.go(Routes.BUDGETS)
                    n.type == NotificationType.MONTHLY_SUMMARY -> nav.tab(Routes.STATS)
                    n.type == NotificationType.INVESTMENT -> nav.go(Routes.INVEST)
                    else -> Unit
                }
            }, background = if (n.read) c.surface else c.accentSoft.copy(alpha = 0.6f).compositeOver(c.surface)) {
                Row(verticalAlignment = Alignment.Top) {
                    IconBadge(L.notificationIcon(n.type), notificationColor(n.type), 42.dp)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(n.title, style = EssaType.title, color = c.text, modifier = Modifier.weight(1f))
                            if (!n.read) Box(Modifier.width(8.dp).height(8.dp).clip(RoundedCornerShape(4.dp)).background(c.accent))
                        }
                        Text(n.body, style = EssaType.body, color = c.textSecondary)
                        Spacer(Modifier.height(4.dp))
                        Text(L.relative(n.timestamp, now, vm.zone, lang), style = EssaType.small, color = c.textMuted)
                    }
                    RoundIconButton(Icons.Rounded.Delete, tr("Supprimer", "Delete"), { vm.run { deleteNotification(it, n.id) } }, tint = c.textMuted, size = 32.dp, background = androidx.compose.ui.graphics.Color.Transparent)
                }
            }
        }
        item {
            Text(tr("Notifications simulées internes à l'application. Réglez les types reçus dans Profil › Préférences.", "In-app simulated notifications. Choose which types you receive in Profile › Preferences."),
                style = EssaType.small, color = c.textMuted)
        }
    }
    if (clear) {
        ConfirmDialog(tr("Effacer toutes les notifications ?", "Clear all notifications?"), tr("L'historique des notifications sera vidé.", "The notification history will be emptied."),
            tr("Effacer", "Clear"), danger = true, onConfirm = { vm.run { clearNotifications(it) } }, onDismiss = { clear = false })
    }
}

@Composable
private fun notificationColor(t: NotificationType) = when (t) {
    NotificationType.BUDGET_EXCEEDED, NotificationType.SCHEDULED_FAILED -> Essa.colors.negative
    NotificationType.BUDGET_WARNING -> Essa.colors.warning
    NotificationType.GOAL_REACHED, NotificationType.TRANSFER_RECEIVED -> Essa.colors.positive
    else -> Essa.colors.accent
}

