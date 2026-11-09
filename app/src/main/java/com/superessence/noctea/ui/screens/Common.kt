package com.superessence.noctea.ui.screens

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.superessence.noctea.NocteaApp
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.data.Catalog
import com.superessence.noctea.data.Track
import com.superessence.noctea.ui.LocalNav
import com.superessence.noctea.ui.Routes
import com.superessence.noctea.ui.components.CardData
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.theme.LocalPalette

fun Track.card(catalog: Catalog, favorites: Set<String>) = CardData(
    id = id, title = title, subtitle = catalog.category(category)?.title ?: "", art = art,
    durationMs = durationMs, favorite = MediaIds.track(id) in favorites, premium = premium,
)

/** Lance une séance de la bibliothèque et ouvre le lecteur. */
fun playTrack(app: NocteaApp, nav: androidx.navigation.NavHostController, trackId: String) {
    app.engine.play(MediaIds.track(trackId))
    nav.navigate(Routes.PLAYER) { launchSingleTop = true }
}

/** Écran secondaire : fond nocturne, en-tête avec retour, contenu défilant. */
@Composable
fun SubScreen(title: String, scroll: Boolean = true, actions: @Composable () -> Unit = {}, content: @Composable ColumnScope.() -> Unit) {
    val nav = LocalNav.current
    NightBackground(stars = 40) {
        Column(Modifier.fillMaxSize().statusBarsPadding()) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 6.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton(onClick = { nav.popBackStack() }) {
                    Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Retour", tint = LocalPalette.current.text)
                }
                Text(title, style = MaterialTheme.typography.headlineMedium, modifier = Modifier.weight(1f))
                actions()
            }
            val m = if (scroll) Modifier.verticalScroll(rememberScrollState()) else Modifier
            Column(Modifier.fillMaxSize().then(m).navigationBarsPadding().padding(bottom = 24.dp), content = content)
        }
    }
}

/** Contenu d'un onglet principal (marge basse pour la barre de navigation et le mini-lecteur). */
val TabContentPadding = PaddingValues(bottom = 170.dp)

@Composable
fun SettingSwitch(title: String, subtitle: String?, checked: Boolean, onChange: (Boolean) -> Unit) {
    val p = LocalPalette.current
    Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            if (subtitle != null) Text(subtitle, style = MaterialTheme.typography.bodySmall)
        }
        Switch(
            checked = checked, onCheckedChange = onChange,
            colors = SwitchDefaults.colors(checkedThumbColor = p.moon, checkedTrackColor = p.accent.copy(alpha = 0.5f), uncheckedTrackColor = Color.White.copy(alpha = 0.08f)),
        )
    }
}

@Composable
fun LabeledSlider(label: String, value: Float, valueText: String, range: ClosedFloatingPointRange<Float> = 0f..1f, steps: Int = 0, onChange: (Float) -> Unit) {
    val p = LocalPalette.current
    Column(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 4.dp)) {
        Row {
            Text(label, style = MaterialTheme.typography.titleSmall, modifier = Modifier.weight(1f))
            Text(valueText, style = MaterialTheme.typography.bodySmall)
        }
        Slider(
            value = value, onValueChange = onChange, valueRange = range, steps = steps,
            colors = SliderDefaults.colors(thumbColor = p.moon, activeTrackColor = p.accent, inactiveTrackColor = Color.White.copy(alpha = 0.12f)),
        )
    }
}

@Composable
fun EmptyState(text: String, modifier: Modifier = Modifier) {
    Box(modifier.fillMaxWidth().padding(32.dp), contentAlignment = Alignment.Center) {
        Text(text, style = MaterialTheme.typography.bodyMedium, textAlign = androidx.compose.ui.text.style.TextAlign.Center)
    }
}

@Composable
fun VSpace(h: Int) = Spacer(Modifier.height(h.dp))
