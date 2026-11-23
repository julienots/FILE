package com.superessence.egide.ui

import android.content.Context
import android.provider.Settings
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.RadioButton
import androidx.compose.material3.RadioButtonDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.draw.scale
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.egide.core.Filter
import com.superessence.egide.core.LogEntry
import com.superessence.egide.core.Origin
import com.superessence.egide.core.Prefs
import com.superessence.egide.core.Sources
import com.superessence.egide.core.Stats
import com.superessence.egide.vpn.EgideVpnService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.text.DateFormat
import java.text.NumberFormat
import java.util.Date

private fun fmt(n: Number): String = NumberFormat.getIntegerInstance(java.util.Locale.FRANCE).format(n)

// ---------- composants ----------

@Composable
fun Panel(modifier: Modifier = Modifier, onClick: (() -> Unit)? = null, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(20.dp))
            .background(C.Surface)
            .border(1.dp, C.Line, RoundedCornerShape(20.dp))
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier)
            .padding(18.dp),
        content = content,
    )
}

@Composable
fun SectionTitle(text: String) {
    Text(text.uppercase(), color = C.Muted, fontSize = 12.sp, letterSpacing = 1.6.sp, fontWeight = FontWeight.SemiBold,
        modifier = Modifier.padding(start = 4.dp, top = 8.dp, bottom = 8.dp))
}

@Composable
private fun ScreenColumn(content: @Composable ColumnScope.() -> Unit) {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
        content = content,
    )
}

@Composable
private fun Header(subtitle: String) {
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 28.dp, bottom = 4.dp)) {
        EssenceLogo(Modifier.width(26.dp), stroke = 9f)
        Spacer(Modifier.width(12.dp))
        Column {
            Text("Égide", color = C.Text, fontSize = 22.sp, fontWeight = FontWeight.SemiBold)
            Text(subtitle, color = C.Muted, fontSize = 12.sp, letterSpacing = 0.5.sp)
        }
    }
}

@Composable
private fun switchColors() = SwitchDefaults.colors(
    checkedThumbColor = Color.White, checkedTrackColor = C.Violet,
    uncheckedThumbColor = C.Muted, uncheckedTrackColor = C.Surface2, uncheckedBorderColor = C.Line,
)

// ---------- Accueil ----------

@Composable
fun HomeScreen(onToggle: () -> Unit, openBrowser: (Boolean) -> Unit) {
    val ctx = LocalContext.current
    val state by EgideVpnService.state.collectAsState()
    val stats by Stats.state.collectAsState()
    val count by Filter.domainCount.collectAsState()
    val on = state == EgideVpnService.State.ON
    val strictDns = remember(state) { privateDnsHostname(ctx) }

    ScreenColumn {
        Header("by SuperEssence Studio")
        Spacer(Modifier.height(8.dp))
        PowerButton(state, onToggle)
        Text(
            when (state) {
                EgideVpnService.State.ON -> "Protection activée"
                EgideVpnService.State.STARTING -> "Démarrage…"
                EgideVpnService.State.OFF -> "Protection désactivée"
            },
            color = if (on) C.Text else C.Muted, fontSize = 22.sp, fontWeight = FontWeight.SemiBold,
            modifier = Modifier.align(Alignment.CenterHorizontally),
        )
        Text(
            if (on) "Pubs, pop-ups, redirections et sites dangereux sont bloqués dans toutes les applis."
            else "Touchez le bouton pour protéger tout le téléphone.",
            color = C.Muted, fontSize = 14.sp, textAlign = androidx.compose.ui.text.style.TextAlign.Center,
            modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp),
        )
        Spacer(Modifier.height(4.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            StatTile("Bloqués", fmt(stats.blocked), C.Violet)
            StatTile("Requêtes", fmt(stats.queries), C.Cyan)
            StatTile("Total", fmt(Stats.lifetimeBlocked), C.Green)
        }

        if (strictDns != null) {
            Panel(onClick = { (ctx as? MainActivity)?.openSystem(Settings.ACTION_WIRELESS_SETTINGS) }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Filled.Warning, null, tint = C.Amber)
                    Spacer(Modifier.width(10.dp))
                    Text("« DNS privé » est forcé sur $strictDns", color = C.Text, fontWeight = FontWeight.SemiBold)
                }
                Spacer(Modifier.height(6.dp))
                Text("Android contourne alors le filtrage d'Égide. Réglages › Réseau et Internet › DNS privé : choisissez « Automatique » ou « Désactivé ». Touchez ici pour ouvrir les réglages.",
                    color = C.Muted, fontSize = 13.sp)
            }
        }

        SectionTitle("Sans pub")
        FeatureCard(
            title = "YouTube sans pub",
            text = "Regardez YouTube dans Égide : publicités vidéo sautées automatiquement, bannières et vidéos sponsorisées masquées.",
            accent = C.Red,
        ) { openBrowser(true) }
        FeatureCard(
            title = "Navigateur protégé",
            text = "Navigation sans pubs, sans pop-ups et sans redirections forcées vers des sites douteux.",
            accent = C.Cyan,
        ) { openBrowser(false) }

        Panel {
            Text("Bon à savoir", color = C.Text, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(6.dp))
            Text(
                "Le blocage système (${fmt(count)} domaines) fonctionne dans toutes les applis, jeux et navigateurs. " +
                    "L'appli officielle YouTube, elle, envoie ses pubs par les mêmes serveurs que les vidéos : aucune appli " +
                    "ne peut les retirer sans casser les vidéos. Pour YouTube sans pub, utilisez le bouton « YouTube sans pub » ci-dessus.",
                color = C.Muted, fontSize = 13.sp, lineHeight = 18.sp,
            )
        }
        Spacer(Modifier.height(8.dp))
    }
}

private fun privateDnsHostname(ctx: Context): String? = runCatching {
    val mode = Settings.Global.getString(ctx.contentResolver, "private_dns_mode")
    if (mode == "hostname") Settings.Global.getString(ctx.contentResolver, "private_dns_specifier") ?: "un serveur" else null
}.getOrNull()

@Composable
private fun ColumnScope.PowerButton(state: EgideVpnService.State, onToggle: () -> Unit) {
    val on = state == EgideVpnService.State.ON
    val inf = rememberInfiniteTransition(label = "ring")
    val angle by inf.animateFloat(0f, 360f, infiniteRepeatable(tween(6000, easing = LinearEasing)), label = "a")
    val pulse by inf.animateFloat(0.97f, 1.03f, infiniteRepeatable(tween(1600), RepeatMode.Reverse), label = "p")
    val level by animateFloatAsState(if (on) 1f else 0f, tween(600), label = "l")
    val starting = state == EgideVpnService.State.STARTING
    Box(
        Modifier
            .align(Alignment.CenterHorizontally)
            .size(220.dp)
            .clickable(remember { MutableInteractionSource() }, indication = null, onClick = onToggle),
        contentAlignment = Alignment.Center,
    ) {
        Canvas(Modifier.size(220.dp).scale(if (on) pulse else 1f)) {
            val r = size.minDimension / 2
            drawCircle(
                Brush.radialGradient(listOf(C.Violet.copy(alpha = 0.30f * level), Color.Transparent), radius = r),
                radius = r,
            )
        }
        Canvas(Modifier.size(180.dp).rotate(if (on || starting) angle else 0f)) {
            val w = 6.dp.toPx()
            drawCircle(C.Surface2, radius = size.minDimension / 2 - w, style = Stroke(w))
            drawArc(
                Brush.sweepGradient(listOf(C.Violet, C.Cyan, C.Lilac, C.Violet)),
                startAngle = 0f, sweepAngle = if (starting) 120f else 360f * level,
                useCenter = false, style = Stroke(w, cap = androidx.compose.ui.graphics.StrokeCap.Round),
                topLeft = Offset(w, w), size = androidx.compose.ui.geometry.Size(size.width - 2 * w, size.height - 2 * w),
            )
        }
        Box(
            Modifier.size(150.dp).clip(CircleShape).background(C.Surface),
            contentAlignment = Alignment.Center,
        ) {
            EssenceLogo(Modifier.width(58.dp), glow = level, stroke = 7f)
            if (!on) Box(Modifier.size(150.dp).background(C.Surface.copy(alpha = 0.55f)))
        }
    }
}

@Composable
private fun RowScope.StatTile(label: String, value: String, accent: Color) {
    Column(
        Modifier.weight(1f).clip(RoundedCornerShape(18.dp)).background(C.Surface)
            .border(1.dp, C.Line, RoundedCornerShape(18.dp)).padding(vertical = 14.dp, horizontal = 12.dp),
    ) {
        Box(Modifier.size(width = 18.dp, height = 3.dp).clip(CircleShape).background(accent))
        Spacer(Modifier.height(10.dp))
        Text(value, color = C.Text, fontSize = 20.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
        Text(label, color = C.Muted, fontSize = 12.sp)
    }
}

@Composable
private fun FeatureCard(title: String, text: String, accent: Color, onClick: () -> Unit) {
    Panel(onClick = onClick) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.size(44.dp).clip(RoundedCornerShape(14.dp)).background(accent.copy(alpha = 0.16f)),
                contentAlignment = Alignment.Center) {
                Icon(Icons.Filled.PlayArrow, null, tint = accent)
            }
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) {
                Text(title, color = C.Text, fontWeight = FontWeight.SemiBold, fontSize = 16.sp)
                Spacer(Modifier.height(2.dp))
                Text(text, color = C.Muted, fontSize = 13.sp, lineHeight = 17.sp)
            }
        }
    }
}

// ---------- Journal ----------

@Composable
fun JournalScreen() {
    val stats by Stats.state.collectAsState()
    val rev by Prefs.revision.collectAsState()
    var onlyBlocked by remember { mutableStateOf(false) }
    var query by remember { mutableStateOf("") }
    var selected by remember { mutableStateOf<String?>(null) }
    val items = remember(stats, onlyBlocked, query, rev) {
        stats.recent.filter { (!onlyBlocked || it.blocked) && (query.isBlank() || it.host.contains(query.trim(), true)) }
    }
    Column(Modifier.padding(horizontal = 16.dp)) {
        Header("Journal en direct")
        Spacer(Modifier.height(10.dp))
        OutlinedTextField(
            value = query, onValueChange = { query = it }, singleLine = true,
            placeholder = { Text("Rechercher un domaine") },
            leadingIcon = { Icon(Icons.Filled.Search, null) },
            modifier = Modifier.fillMaxWidth(), shape = RoundedCornerShape(16.dp),
            colors = fieldColors(),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(vertical = 8.dp)) {
            Chip("Tout", !onlyBlocked) { onlyBlocked = false }
            Chip("Bloqués", onlyBlocked) { onlyBlocked = true }
        }
        if (items.isEmpty()) {
            Text(
                if (stats.recent.isEmpty()) "Rien pour l'instant. Activez la protection puis utilisez votre téléphone : chaque connexion s'affichera ici."
                else "Aucun résultat.",
                color = C.Muted, fontSize = 14.sp, modifier = Modifier.padding(top = 24.dp),
            )
        }
        LazyColumn(contentPadding = PaddingValues(bottom = 16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            items(items, key = { "${it.time}-${it.host}-${it.blocked}-${it.origin}" }) { e -> LogRow(e) { selected = e.host } }
        }
    }
    selected?.let { host -> DomainDialog(host) { selected = null } }
}

@Composable
private fun Chip(label: String, sel: Boolean, onClick: () -> Unit) {
    FilterChip(
        selected = sel, onClick = onClick, label = { Text(label) },
        colors = FilterChipDefaults.filterChipColors(
            selectedContainerColor = C.Violet.copy(alpha = 0.3f), selectedLabelColor = C.Text,
            containerColor = C.Surface, labelColor = C.Muted,
        ),
    )
}

@Composable
private fun LogRow(e: LogEntry, onClick: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(C.Surface).clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(8.dp).clip(CircleShape).background(if (e.blocked) C.Red else C.Green))
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(e.host, color = C.Text, fontSize = 14.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(
                buildString {
                    append(DateFormat.getTimeInstance(DateFormat.MEDIUM).format(Date(e.time)))
                    append(if (e.origin == Origin.BROWSER) " · navigateur" else " · système")
                    e.note?.let { append(" · ").append(it) }
                },
                color = C.Muted, fontSize = 11.sp,
            )
        }
        Text(if (e.blocked) "Bloqué" else "Autorisé", color = if (e.blocked) C.Red else C.Muted, fontSize = 12.sp)
    }
}

@Composable
private fun DomainDialog(host: String, onDismiss: () -> Unit) {
    val allowed = host in Prefs.allowList
    val denied = host in Prefs.denyList
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = C.Surface2,
        title = { Text(host, fontSize = 17.sp) },
        text = {
            Text(
                when {
                    allowed -> "Ce domaine est dans votre liste blanche (toujours autorisé)."
                    denied -> "Ce domaine est dans votre liste de blocage personnelle."
                    Filter.isBlocked(host) -> "Ce domaine est bloqué par les listes. Autorisez-le si une appli ou un site ne fonctionne plus."
                    else -> "Ce domaine est autorisé. Bloquez-le si vous le jugez indésirable."
                },
                color = C.Muted,
            )
        },
        confirmButton = {
            if (allowed || denied) TextButton({ Prefs.reset(host); onDismiss() }) { Text("Réinitialiser") }
            else if (Filter.isBlocked(host)) TextButton({ Prefs.allow(host); onDismiss() }) { Text("Autoriser") }
            else TextButton({ Prefs.deny(host); onDismiss() }) { Text("Bloquer") }
        },
        dismissButton = { TextButton(onDismiss) { Text("Fermer") } },
    )
}

@Composable
private fun fieldColors() = OutlinedTextFieldDefaults.colors(
    focusedBorderColor = C.Violet, unfocusedBorderColor = C.Line,
    focusedContainerColor = C.Surface, unfocusedContainerColor = C.Surface,
    cursorColor = C.Violet, focusedTextColor = C.Text, unfocusedTextColor = C.Text,
    focusedPlaceholderColor = C.Muted, unfocusedPlaceholderColor = C.Muted,
    focusedLeadingIconColor = C.Muted, unfocusedLeadingIconColor = C.Muted,
)

// ---------- Listes ----------

@Composable
fun ListsScreen() {
    val rev by Prefs.revision.collectAsState()
    val loaded by Filter.loaded.collectAsState()
    val count by Filter.domainCount.collectAsState()
    val scope = rememberCoroutineScope()
    var updating by remember { mutableStateOf<String?>(null) }
    var result by remember { mutableStateOf<String?>(null) }

    ScreenColumn {
        Header("${fmt(count)} domaines bloqués")
        SectionTitle("Listes de blocage")
        for (src in Sources.all) {
            val enabled = remember(rev) { Prefs.isListEnabled(src.id) }
            Panel {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(src.title, color = C.Text, fontWeight = FontWeight.SemiBold)
                        Text(
                            if (loaded) "${fmt(Filter.listSize(src.id))} domaines" else "chargement…",
                            color = C.Cyan, fontSize = 12.sp,
                        )
                    }
                    Switch(enabled, { Prefs.setListEnabled(src.id, it) }, colors = switchColors())
                }
                Spacer(Modifier.height(6.dp))
                Text(src.description, color = C.Muted, fontSize = 13.sp, lineHeight = 17.sp)
                Spacer(Modifier.height(6.dp))
                val upd = Filter.lastUpdate(src.id)
                Text(
                    (if (upd > 0) "Mise à jour : " + DateFormat.getDateInstance(DateFormat.MEDIUM).format(Date(upd)) else "Version intégrée à l'appli") +
                        " · " + src.credit,
                    color = C.Muted.copy(alpha = 0.7f), fontSize = 11.sp,
                )
            }
        }
        if (updating != null) {
            LinearProgressIndicator(Modifier.fillMaxWidth(), color = C.Violet, trackColor = C.Surface2)
            Text("Téléchargement : $updating", color = C.Muted, fontSize = 13.sp)
        }
        result?.let { Text(it, color = C.Muted, fontSize = 13.sp) }
        Button(
            onClick = {
                if (updating != null) return@Button
                updating = "…"; result = null
                scope.launch {
                    val (ok, ko) = withContext(Dispatchers.IO) { Filter.updateAll { t -> updating = t } }
                    updating = null
                    result = if (ko == 0) "Les $ok listes sont à jour." else "$ok liste(s) à jour, $ko échec(s) (connexion ?)."
                }
            },
            modifier = Modifier.fillMaxWidth().height(52.dp), shape = RoundedCornerShape(16.dp),
            colors = ButtonDefaults.buttonColors(containerColor = C.Violet),
        ) { Text("Mettre à jour les listes") }

        RuleEditor(
            "Liste blanche (toujours autorisés)",
            "Ajoutez un site qui ne fonctionne plus à cause du blocage.",
            remember(rev) { Prefs.allowList },
            onAdd = { Prefs.allow(it) }, onRemove = { Prefs.reset(it) },
        )
        RuleEditor(
            "Mes blocages",
            "Ajoutez un domaine à bloquer (ses sous-domaines le seront aussi).",
            remember(rev) { Prefs.denyList },
            onAdd = { Prefs.deny(it) }, onRemove = { Prefs.reset(it) },
        )
        Spacer(Modifier.height(8.dp))
    }
}

@Composable
private fun RuleEditor(title: String, hint: String, items: Set<String>, onAdd: (String) -> Unit, onRemove: (String) -> Unit) {
    var text by remember { mutableStateOf("") }
    SectionTitle(title)
    Panel {
        Text(hint, color = C.Muted, fontSize = 13.sp)
        Spacer(Modifier.height(10.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(
                value = text, onValueChange = { text = it }, singleLine = true,
                placeholder = { Text("exemple.com") }, modifier = Modifier.weight(1f),
                shape = RoundedCornerShape(14.dp), colors = fieldColors(),
            )
            Spacer(Modifier.width(8.dp))
            TextButton({
                val d = normalizeDomain(text)
                if (d != null) { onAdd(d); text = "" }
            }) { Text("Ajouter", color = C.Lilac) }
        }
        for (d in items) {
            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 6.dp)) {
                Text(d, color = C.Text, fontSize = 14.sp, modifier = Modifier.weight(1f))
                IconButton({ onRemove(d) }) { Icon(Icons.Filled.Close, "Retirer", tint = C.Muted) }
            }
        }
    }
}

private fun normalizeDomain(input: String): String? {
    var s = input.trim().lowercase()
    s = s.substringAfter("://").substringBefore('/').substringBefore(':').removePrefix("www.").removePrefix("*.")
    return s.takeIf { it.contains('.') && it.all { c -> c.isLetterOrDigit() || c == '.' || c == '-' || c == '_' } }
}

// ---------- Réglages ----------

@Composable
fun SettingsScreen() {
    val ctx = LocalContext.current
    val rev by Prefs.revision.collectAsState()
    val state by EgideVpnService.state.collectAsState()
    ScreenColumn {
        Header("Réglages")
        SectionTitle("Serveur DNS")
        Panel {
            for (u in Prefs.upstreams) {
                val sel = remember(rev) { Prefs.upstreamId == u.id }
                Row(
                    Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).clickable { Prefs.upstreamId = u.id }.padding(vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    RadioButton(sel, { Prefs.upstreamId = u.id }, colors = RadioButtonDefaults.colors(selectedColor = C.Violet, unselectedColor = C.Muted))
                    Column {
                        Text(u.title, color = C.Text)
                        Text(u.detail, color = C.Muted, fontSize = 12.sp)
                    }
                }
            }
            if (state == EgideVpnService.State.ON) {
                Text("Pris en compte immédiatement.", color = C.Muted, fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
            }
        }
        SectionTitle("Fonctionnement")
        ToggleRow("Démarrer avec le téléphone", "Relance la protection après un redémarrage.", remember(rev) { Prefs.startOnBoot }) { Prefs.startOnBoot = it }
        ToggleRow("Bloquer pop-ups et redirections", "Dans le navigateur Égide : nouvelles fenêtres et redirections non demandées.", remember(rev) { Prefs.browserBlockPopups }) { Prefs.browserBlockPopups = it }
        Panel(onClick = { (ctx as? MainActivity)?.openSystem(Settings.ACTION_VPN_SETTINGS) }) {
            Text("VPN permanent", color = C.Text, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(4.dp))
            Text("Pour une protection qui ne s'arrête jamais : Réglages Android › VPN › Égide › « VPN permanent ». Touchez ici.",
                color = C.Muted, fontSize = 13.sp)
        }
        Panel {
            Text("Astuce : réglages rapides", color = C.Text, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(4.dp))
            Text("Ajoutez la tuile « Égide » dans le volet des réglages rapides pour activer ou couper la protection d'un geste.",
                color = C.Muted, fontSize = 13.sp)
        }
        SectionTitle("À propos")
        Panel {
            Row(verticalAlignment = Alignment.CenterVertically) {
                EssenceLogo(Modifier.width(34.dp), stroke = 8f)
                Spacer(Modifier.width(14.dp))
                Column {
                    Text("Égide 1.0.0", color = C.Text, fontWeight = FontWeight.SemiBold)
                    Text("SuperEssence Studio", color = C.Lilac, fontSize = 13.sp)
                }
            }
            Spacer(Modifier.height(10.dp))
            Text(
                "Égide filtre localement, sur votre téléphone. Aucune donnée n'est envoyée à SuperEssence Studio : " +
                    "le VPN ne sert qu'à intercepter les requêtes DNS, qui partent ensuite vers le serveur DNS choisi ci-dessus.\n\n" +
                    "Listes : StevenBlack/hosts (MIT), HaGeZi DNS Blocklists (GPL-3.0).",
                color = C.Muted, fontSize = 12.sp, lineHeight = 17.sp,
            )
        }
        Spacer(Modifier.height(8.dp))
    }
}

@Composable
private fun ToggleRow(title: String, sub: String, value: Boolean, onChange: (Boolean) -> Unit) {
    Panel(onClick = { onChange(!value) }) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(title, color = C.Text, fontWeight = FontWeight.SemiBold)
                Text(sub, color = C.Muted, fontSize = 13.sp)
            }
            Switch(value, onChange, colors = switchColors())
        }
    }
}
