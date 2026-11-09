package com.superessence.noctea.ui.screens

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
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
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bedtime
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.GraphicEq
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Shuffle
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.data.Discovery
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.LocalNav
import com.superessence.noctea.ui.Routes
import com.superessence.noctea.ui.components.GlassCard
import com.superessence.noctea.ui.components.MoonMark
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.components.NocteaWordmark
import com.superessence.noctea.ui.components.PrimaryButton
import com.superessence.noctea.ui.components.SectionTitle
import com.superessence.noctea.ui.components.TrackCardLarge
import com.superessence.noctea.ui.components.TrackRow
import com.superessence.noctea.ui.components.formatDuration
import com.superessence.noctea.ui.theme.Cormorant
import com.superessence.noctea.ui.theme.LocalPalette
import java.time.LocalDate
import java.time.LocalTime

@Composable
fun HomeScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    val catalog = app.catalog
    val hour = remember { LocalTime.now().hour }
    var mood by rememberSaveable { mutableIntStateOf(0) }
    val quote = remember { catalog.quotes[(LocalDate.now().dayOfYear) % catalog.quotes.size] }
    val recommended = remember(snap.history.size, snap.favorites) { Discovery.recommended(catalog, hour, snap.history, snap.favorites) }
    val popular = remember { listOf("sleep_deep", "breath_478", "story_cabin", "breath_coherence", "sleep_journey", "relax_deep").mapNotNull { catalog.track(it) } }
    val resume = snap.positions.entries.filter { it.value > 15_000 }.mapNotNull { e ->
        app.resolver.resolve(e.key)?.let { it to e.value }
    }.take(3)
    val float by rememberInfiniteTransition(label = "moon").animateFloat(-6f, 6f, infiniteRepeatable(tween(4200), RepeatMode.Reverse), label = "f")

    NightBackground {
        LazyColumn(Modifier.fillMaxSize(), contentPadding = TabContentPadding) {
            item {
                Row(Modifier.fillMaxWidth().statusBarsPadding().padding(start = 20.dp, end = 8.dp, top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    MoonMark(34.dp, glow = false)
                    Spacer(Modifier.width(8.dp))
                    NocteaWordmark(fontSize = 22)
                    Spacer(Modifier.weight(1f))
                    IconButton(onClick = { nav.navigate(Routes.SEARCH) }) { Icon(Icons.Filled.Search, "Rechercher", tint = p.text) }
                }
            }
            // ------------------------------------------------ héros
            item {
                Box(Modifier.fillMaxWidth().height(300.dp)) {
                    MoonMark(210.dp, Modifier.align(Alignment.TopEnd).padding(end = 4.dp).graphicsLayer { translationY = float })
                    Column(Modifier.align(Alignment.BottomStart).padding(horizontal = 24.dp)) {
                        Text(Discovery.greeting(hour, snap.settings.userName), style = MaterialTheme.typography.displayMedium)
                        Spacer(Modifier.height(6.dp))
                        Text(
                            if (hour in 5..17) "Prends un moment pour respirer." else "La nuit t'attend, tout en douceur.",
                            style = MaterialTheme.typography.bodyLarge, color = p.textDim,
                        )
                        Spacer(Modifier.height(18.dp))
                        PrimaryButton("Commencer ma méditation", icon = { Icon(Icons.Filled.PlayArrow, null, tint = Color(0xFF1A1430)) }) {
                            val t = recommended.firstOrNull() ?: catalog.tracks.first()
                            playTrack(app, nav, t.id)
                        }
                    }
                }
            }
            // ------------------------------------------------ humeur
            item {
                Spacer(Modifier.height(26.dp))
                SectionTitle("Comment te sens-tu ?")
                LazyRow(contentPadding = PaddingValues(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    items(Discovery.moods) { m ->
                        Column(
                            Modifier
                                .clip(RoundedCornerShape(18.dp))
                                .background(if (mood == m.id) p.accent.copy(alpha = 0.3f) else Color.White.copy(alpha = 0.06f))
                                .clickable { mood = m.id }
                                .padding(horizontal = 14.dp, vertical = 10.dp),
                            horizontalAlignment = Alignment.CenterHorizontally,
                        ) {
                            Text(m.emoji, fontSize = 24.sp)
                            Text(m.label, style = MaterialTheme.typography.labelMedium, color = p.text)
                        }
                    }
                }
                if (mood != 0) {
                    Spacer(Modifier.height(12.dp))
                    Text(Discovery.moodMessage(mood), style = MaterialTheme.typography.bodyLarge, fontStyle = FontStyle.Italic, modifier = Modifier.padding(horizontal = 24.dp), color = p.accent)
                    Discovery.forMood(catalog, mood).forEach { t ->
                        TrackRow(t.card(catalog, snap.favorites), onClick = { playTrack(app, nav, t.id) }, onFav = { app.store.toggleFavorite(MediaIds.track(t.id)) })
                    }
                }
            }
            // ------------------------------------------------ reprise
            if (resume.isNotEmpty()) {
                item {
                    Spacer(Modifier.height(22.dp))
                    SectionTitle("Reprendre")
                    resume.forEach { (pl, pos) ->
                        GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 5.dp), onClick = {
                            app.engine.play(pl.mediaId); nav.navigate(Routes.PLAYER)
                        }, padding = 14.dp) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(Icons.Filled.PlayArrow, null, tint = p.moon)
                                Spacer(Modifier.width(10.dp))
                                Column(Modifier.weight(1f)) {
                                    Text(pl.title, style = MaterialTheme.typography.titleSmall)
                                    Text("Il reste ${formatDuration(pl.durationMs - pos)}", style = MaterialTheme.typography.bodySmall)
                                }
                            }
                        }
                    }
                }
            }
            // ------------------------------------------------ rituel + mixeur + aléatoire
            item {
                Spacer(Modifier.height(22.dp))
                Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    QuickTile("Mon rituel", "Ce soir à ${"%02d:%02d".format(snap.ritual.bedtimeMinutes / 60, snap.ritual.bedtimeMinutes % 60)}", Icons.Filled.Bedtime, Modifier.weight(1f)) { nav.navigate(Routes.RITUAL) }
                    QuickTile("Mixeur nocturne", "Compose ton ambiance", Icons.Filled.GraphicEq, Modifier.weight(1f)) { nav.navigate(Routes.MIXER) }
                }
                Spacer(Modifier.height(12.dp))
                QuickTile("Méditation aléatoire", "Laisse la nuit choisir pour toi", Icons.Filled.Shuffle, Modifier.padding(horizontal = 20.dp).fillMaxWidth()) {
                    playTrack(app, nav, catalog.tracks.random().id)
                }
            }
            // ------------------------------------------------ sélection
            item {
                Spacer(Modifier.height(26.dp))
                SectionTitle("Sélectionné pour toi", action = "Explorer", onAction = { nav.navigate(Routes.EXPLORE) })
                LazyRow(contentPadding = PaddingValues(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    items(recommended, key = { it.id }) { t ->
                        TrackCardLarge(t.card(catalog, snap.favorites), onClick = { playTrack(app, nav, t.id) }, onFav = { app.store.toggleFavorite(MediaIds.track(t.id)) })
                    }
                }
            }
            // ------------------------------------------------ citation
            item {
                Spacer(Modifier.height(26.dp))
                GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp)) {
                    Text("“", fontFamily = Cormorant, fontSize = 48.sp, color = p.moon, modifier = Modifier.height(36.dp))
                    Text(quote.text, fontFamily = Cormorant, fontStyle = FontStyle.Italic, fontSize = 22.sp, color = p.text, lineHeight = 28.sp)
                    Spacer(Modifier.height(8.dp))
                    Text("— ${quote.author}", style = MaterialTheme.typography.bodySmall)
                }
            }
            // ------------------------------------------------ populaires
            item {
                Spacer(Modifier.height(26.dp))
                SectionTitle("Les plus écoutées")
            }
            items(popular, key = { "pop_" + it.id }) { t ->
                TrackRow(t.card(catalog, snap.favorites), onClick = { playTrack(app, nav, t.id) }, onFav = { app.store.toggleFavorite(MediaIds.track(t.id)) })
            }
            item {
                Spacer(Modifier.height(24.dp))
                Text(
                    "Une création Superessence Studio",
                    style = MaterialTheme.typography.labelSmall, color = p.textDim.copy(alpha = 0.6f),
                    modifier = Modifier.fillMaxWidth().clickable { nav.navigate(Routes.ABOUT) }.padding(12.dp),
                    textAlign = androidx.compose.ui.text.style.TextAlign.Center,
                )
            }
        }
    }
}

@Composable
private fun QuickTile(title: String, subtitle: String, icon: ImageVector, modifier: Modifier = Modifier, onClick: () -> Unit) {
    val p = LocalPalette.current
    GlassCard(modifier, onClick = onClick, padding = 16.dp) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier.size(40.dp).clip(CircleShape).background(Brush.linearGradient(listOf(p.accent.copy(alpha = 0.5f), p.moon.copy(alpha = 0.4f)))),
                contentAlignment = Alignment.Center,
            ) { Icon(icon, null, tint = Color.White, modifier = Modifier.size(22.dp)) }
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(title, style = MaterialTheme.typography.titleSmall)
                Text(subtitle, style = MaterialTheme.typography.bodySmall, maxLines = 1)
            }
            Icon(Icons.Filled.ChevronRight, null, tint = p.textDim)
        }
    }
}
