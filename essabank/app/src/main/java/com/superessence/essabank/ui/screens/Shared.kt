package com.superessence.essabank.ui.screens

import android.content.Context
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.superessence.essabank.core.model.Account
import com.superessence.essabank.core.model.BankState
import com.superessence.essabank.core.model.Transaction
import com.superessence.essabank.core.model.TxKind
import com.superessence.essabank.core.model.TxStatus
import com.superessence.essabank.core.money.AppLanguage
import com.superessence.essabank.ui.components.AccountGlyph
import com.superessence.essabank.ui.components.IconBadge
import com.superessence.essabank.ui.components.ListRow
import com.superessence.essabank.ui.components.LocalPrefs
import com.superessence.essabank.ui.components.TxAmount
import com.superessence.essabank.ui.components.fmt
import com.superessence.essabank.ui.components.pressable
import com.superessence.essabank.ui.i18n.L
import com.superessence.essabank.ui.i18n.LocalLang
import com.superessence.essabank.ui.i18n.tr
import com.superessence.essabank.ui.theme.Brand
import com.superessence.essabank.ui.theme.Essa
import com.superessence.essabank.ui.theme.EssaType
import java.time.LocalDate
import java.time.ZoneId

/** Ligne de transaction standard. */
@Composable
fun TransactionRow(tx: Transaction, state: BankState, zone: ZoneId, onClick: () -> Unit, showAccount: Boolean = false) {
    val c = Essa.colors
    val lang = LocalLang.current
    val color = L.categoryColor(tx.category)
    val subtitle = buildString {
        append(L.category(tx.category, lang))
        append(" · ")
        append(L.time(tx.timestamp, zone))
        if (showAccount) state.account(tx.accountId)?.let { append(" · "); append(it.name) }
        if (tx.status != TxStatus.COMPLETED) { append(" · "); append(statusShort(tx.status, lang)) }
    }
    ListRow(
        title = tx.label, subtitle = subtitle,
        leading = { IconBadge(L.categoryIcon(tx.category), color, 44.dp) },
        trailing = {
            Column(horizontalAlignment = Alignment.End) {
                TxAmount(tx.signed)
                if (tx.kind == TxKind.INTERNAL_TRANSFER || tx.kind == TxKind.SAVINGS_DEPOSIT || tx.kind == TxKind.SAVINGS_WITHDRAWAL) {
                    Text(tr("Interne", "Internal"), style = EssaType.small, color = c.textMuted)
                }
            }
        },
        onClick = onClick, chevron = false,
    )
}

fun statusShort(s: TxStatus, lang: AppLanguage) = when (s) {
    TxStatus.COMPLETED -> if (lang == AppLanguage.FR) "Exécutée" else "Completed"
    TxStatus.PENDING -> if (lang == AppLanguage.FR) "En attente" else "Pending"
    TxStatus.DECLINED -> if (lang == AppLanguage.FR) "Refusée" else "Declined"
}

/** En-tête de jour dans une liste chronologique. */
@Composable
fun DayHeader(day: LocalDate, today: LocalDate, total: String? = null) {
    val c = Essa.colors
    Row(Modifier.fillMaxWidth().padding(top = 12.dp, bottom = 2.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(L.dayHeader(day, today, LocalLang.current), style = EssaType.caption, color = c.textMuted, modifier = Modifier.weight(1f))
        if (total != null) Text(total, style = EssaType.caption, color = c.textMuted)
    }
}

/** Carte de compte (carrousel de l'accueil et liste des comptes). */
@Composable
fun AccountTile(account: Account, onClick: () -> Unit, modifier: Modifier = Modifier) {
    val c = Essa.colors
    val lang = LocalLang.current
    val col = Brand.color(account.colorIndex)
    Column(
        modifier.width(176.dp).clip(RoundedCornerShape(22.dp))
            .background(Brush.linearGradient(listOf(col.copy(alpha = if (c.isDark) 0.30f else 0.14f), c.surface)))
            .background(Color.Transparent)
            .pressable(onClick = onClick).padding(16.dp),
    ) {
        AccountGlyph(account.icon, account.colorIndex, 40.dp)
        Spacer(Modifier.height(18.dp))
        Text(account.name, style = EssaType.bodyStrong, color = c.text, maxLines = 1, overflow = TextOverflow.Ellipsis)
        Text(L.accountType(account.type, lang), style = EssaType.small, color = c.textMuted, maxLines = 1, overflow = TextOverflow.Ellipsis)
        Spacer(Modifier.height(10.dp))
        Text(fmt(account.balance), style = EssaType.amount.copy(fontWeight = FontWeight.Bold), color = c.text, maxLines = 1)
    }
}

/** Ligne de compte (sélecteurs et listes). */
@Composable
fun AccountRow(account: Account, onClick: (() -> Unit)?, selected: Boolean = false, trailingText: String? = null) {
    val c = Essa.colors
    val lang = LocalLang.current
    Box(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp))
            .background(if (selected) c.accentSoft else Color.Transparent).padding(horizontal = 6.dp),
    ) {
        ListRow(
            title = account.name, subtitle = L.accountType(account.type, lang) + " · " + account.reference,
            leading = { AccountGlyph(account.icon, account.colorIndex) },
            trailing = { Text(trailingText ?: fmt(account.balance), style = EssaType.amount, color = c.text) },
            onClick = onClick, chevron = false,
        )
    }
}

/** Regroupe des transactions par jour (ordre antichronologique). */
fun groupByDay(txs: List<Transaction>, zone: ZoneId): List<Pair<LocalDate, List<Transaction>>> =
    txs.sortedWith(compareByDescending<Transaction> { it.timestamp }.thenByDescending { it.seq })
        .groupBy { java.time.Instant.ofEpochMilli(it.timestamp).atZone(zone).toLocalDate() }
        .toList()

/** Enregistre un fichier via le sélecteur système (Storage Access Framework). */
@Composable
fun rememberFileSaver(mime: String, onResult: (Boolean) -> Unit = {}): (String, ByteArray) -> Unit {
    val context = LocalContext.current
    var pending by remember { mutableStateOf<ByteArray?>(null) }
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument(mime)) { uri ->
        val data = pending
        pending = null
        if (uri != null && data != null) onResult(writeUri(context, uri, data)) else onResult(false)
    }
    return { name, bytes -> pending = bytes; launcher.launch(name) }
}

fun writeUri(context: Context, uri: Uri, data: ByteArray): Boolean = try {
    context.contentResolver.openOutputStream(uri, "w")?.use { it.write(data) } != null
} catch (e: Exception) { false }

fun readUri(context: Context, uri: Uri): ByteArray? = try {
    context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
} catch (e: Exception) { null }

/** Petit indicateur coloré « +2,3 % ». */
@Composable
fun ChangeBadge(text: String, positive: Boolean) {
    val c = Essa.colors
    val col = if (positive) c.positive else c.negative
    Box(Modifier.clip(RoundedCornerShape(8.dp)).background(col.copy(alpha = 0.14f)).padding(horizontal = 8.dp, vertical = 3.dp)) {
        Text(text, style = EssaType.small.copy(fontWeight = FontWeight.Bold), color = col)
    }
}

@Composable
fun BulletLine(text: String) {
    val c = Essa.colors
    Row(Modifier.padding(vertical = 3.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Box(Modifier.padding(top = 7.dp).size(5.dp).clip(RoundedCornerShape(3.dp)).background(c.accent))
        Text(text, style = EssaType.body, color = c.textSecondary)
    }
}

@Composable
fun prefsLang(): AppLanguage = LocalPrefs.current.language
