package com.superessence.focus.ui

import android.Manifest
import android.app.AlarmManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Check
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import com.superessence.focus.core.Apps
import com.superessence.focus.core.BlockerService
import com.superessence.focus.core.NotifBlocker
import com.superessence.focus.core.Session
import com.superessence.focus.core.Sms
import com.superessence.focus.core.Store

@Composable
fun BlockScreen() {
    val ctx = LocalContext.current
    @Suppress("UNUSED_VARIABLE") val rev = Store.rev.intValue
    val now = rememberNow()
    var tick by remember { mutableIntStateOf(0) }
    val owner = LocalLifecycleOwner.current
    DisposableEffect(owner) {
        val o = LifecycleEventObserver { _, e -> if (e == Lifecycle.Event.ON_RESUME) { tick++; Apps.invalidate() } }
        owner.lifecycle.addObserver(o)
        onDispose { owner.lifecycle.removeObserver(o) }
    }
    val perms = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { tick++ }
    @Suppress("UNUSED_EXPRESSION") tick

    val apps = remember { Apps.installed(ctx) }
    var query by remember { mutableStateOf("") }
    val locked = Session.blocking

    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        item {
            Title("Blocage")
            Spacer(Modifier.height(4.dp))
            Panel(Modifier.fillMaxWidth(), color = if (locked) C.Yellow else C.Surface) {
                if (locked) {
                    Title("🔒 Blocage actif", 20.sp, color = Color.Black)
                    Body("Encore ${fmt(Session.remainingMs(now))}. Tout est bloqué sauf la calculatrice, tes applis autorisées et ${Store.romaneName}. Seule l'appli Focus. peut arrêter la session.", color = Color.Black)
                } else {
                    Title("Prêt", 20.sp)
                    Body("Lance une session depuis l'onglet Réviser. Pendant la session, les Réglages eux-mêmes sont bloqués : on ne peut arrêter que depuis Focus.")
                }
            }
            Spacer(Modifier.height(8.dp))
            Title("Configuration (une seule fois)", 18.sp)
        }
        item {
            Step(
                "1. Service de blocage", "Paramètres › Accessibilité › Focus. › Activer", BlockerService.isEnabled(ctx),
            ) { ctx.startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)) }
        }
        item {
            Step(
                "2. Silence des autres conversations", "Masque les notifications WhatsApp, Snap, Insta… pendant la session", NotifBlocker.isEnabled(ctx),
            ) {
                val i = if (Build.VERSION.SDK_INT >= 30) Intent(Settings.ACTION_NOTIFICATION_LISTENER_DETAIL_SETTINGS)
                    .putExtra(Settings.EXTRA_NOTIFICATION_LISTENER_COMPONENT_NAME, NotifBlocker.component(ctx).flattenToString())
                else Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
                runCatching { ctx.startActivity(i) }.onFailure { ctx.startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }
            }
        }
        item {
            Step("3. Messages avec ${Store.romaneName}", "Accès aux SMS (uniquement votre conversation)", Sms.granted(ctx)) { perms.launch(Sms.PERMS) }
        }
        if (Build.VERSION.SDK_INT >= 33) item {
            val ok = ContextCompat.checkSelfPermission(ctx, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED
            Step("4. Notifications de Focus.", "Fin de session et messages de ${Store.romaneName}", ok) { perms.launch(arrayOf(Manifest.permission.POST_NOTIFICATIONS)) }
        }
        if (Build.VERSION.SDK_INT >= 31) item {
            val ok = ctx.getSystemService(AlarmManager::class.java).canScheduleExactAlarms()
            Step("5. Fin de session à la seconde près", "Facultatif : autorise les alarmes exactes", ok) {
                runCatching { ctx.startActivity(Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM, Uri.parse("package:" + ctx.packageName))) }
            }
        }
        if (Build.VERSION.SDK_INT >= 33 && !BlockerService.isEnabled(ctx)) item {
            Panel(Modifier.fillMaxWidth(), color = C.Surface2) {
                Body("⚠️ Option grisée ou « paramètre restreint » ?", color = C.Text, weight = FontWeight.Bold)
                Spacer(Modifier.height(4.dp))
                Body("Android protège l'accessibilité pour les applis installées hors Play Store. Ouvre Infos sur l'appli › menu ⋮ en haut à droite › « Autoriser les paramètres restreints », puis reviens à l'étape 1.")
                Spacer(Modifier.height(10.dp))
                Chip("Ouvrir Infos sur l'appli", true) { openAppInfo(ctx) }
            }
        }
        item {
            Spacer(Modifier.height(10.dp))
            Title("Toujours autorisé", 18.sp)
            Body("Calculatrice · ${Store.romaneName} (dans Focus.) · Téléphone pendant un appel · Appels d'urgence · Clavier et écran d'accueil", size = 13.sp)
            Spacer(Modifier.height(14.dp))
            Title("Applis autorisées", 18.sp)
            Body(if (locked) "Verrouillé pendant la session." else "Ajoute ici tes applis de révision (ex. ATI). Tout le reste sera bloqué.", size = 13.sp)
            Spacer(Modifier.height(8.dp))
            OutlinedTextField(
                query, { query = it }, Modifier.fillMaxWidth(), placeholder = { Text("Rechercher une appli") },
                leadingIcon = { Icon(Icons.Rounded.Search, null, tint = C.Muted) }, singleLine = true, colors = fieldColors(), shape = RoundedCornerShape(16.dp),
            )
        }
        items(apps.filter { query.isBlank() || it.label.contains(query, ignoreCase = true) }, key = { it.pkg }) { a ->
            Row(
                Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(C.Surface).padding(horizontal = 14.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                val bmp = remember(a.pkg) { a.icon?.toBitmap(80, 80)?.asImageBitmap() }
                if (bmp != null) Image(bmp, null, Modifier.size(34.dp)) else Box(Modifier.size(34.dp))
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Body(a.label, color = C.Text, size = 15.sp, weight = FontWeight.SemiBold)
                    if (a.auto) Body("Calculatrice · toujours autorisée", size = 11.sp, color = C.Yellow)
                }
                Switch(
                    checked = a.auto || a.pkg in Store.allowedApps,
                    onCheckedChange = { on -> Store.allowedApps = if (on) Store.allowedApps + a.pkg else Store.allowedApps - a.pkg },
                    enabled = !a.auto && !locked,
                    colors = SwitchDefaults.colors(checkedTrackColor = C.Yellow, checkedThumbColor = Color.Black, uncheckedTrackColor = C.Surface2, uncheckedBorderColor = C.Line),
                )
            }
        }
    }
}

@Composable
private fun Step(title: String, sub: String, done: Boolean, onClick: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(C.Surface).clickable(enabled = !done, onClick = onClick).padding(14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier.size(30.dp).clip(CircleShape).background(if (done) C.Green else C.Surface2),
            contentAlignment = Alignment.Center,
        ) { if (done) Icon(Icons.Rounded.Check, null, tint = Color.Black, modifier = Modifier.size(18.dp)) }
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Body(title, color = C.Text, size = 15.sp, weight = FontWeight.Bold)
            Body(sub, size = 12.sp)
        }
        if (!done) Text("Activer", color = C.Yellow, fontFamily = Nunito, fontWeight = FontWeight.Bold)
    }
}

private fun openAppInfo(ctx: Context) =
    ctx.startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + ctx.packageName)))
