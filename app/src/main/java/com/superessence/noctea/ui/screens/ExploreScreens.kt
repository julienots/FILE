package com.superessence.noctea.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.data.Discovery
import com.superessence.noctea.data.Sounds
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.LocalNav
import com.superessence.noctea.ui.Routes
import com.superessence.noctea.ui.components.ArtIllustration
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.components.SectionTitle
import com.superessence.noctea.ui.components.TrackCardLarge
import com.superessence.noctea.ui.components.TrackRow
import com.superessence.noctea.ui.theme.LocalPalette

private val categoryArt = mapOf(
    "sommeil" to "moon", "relaxation" to "lotus", "respiration" to "breath",
    "meditation" to "stars", "histoires" to "cabin", "reveil" to "sun",
)

@Composable
fun ExploreScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    val catalog = app.catalog
    NightBackground {
        LazyColumn(Modifier.fillMaxSize(), contentPadding = TabContentPadding) {
            item {
                Column(Modifier.statusBarsPadding().padding(horizontal = 20.dp, vertical = 12.dp)) {
                    Text("Explorer", style = MaterialTheme.typography.displayMedium)
                    Text("${catalog.tracks.size} séances guidées, ${Sounds.all.size} ambiances", style = MaterialTheme.typography.bodyMedium)
                    Spacer(Modifier.height(14.dp))
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(50))
                            .background(Color.White.copy(alpha = 0.07f))
                            .border(1.dp, p.cardBorder, RoundedCornerShape(50))
                            .clickable { nav.navigate(Routes.SEARCH) }
                            .padding(horizontal = 18.dp, vertical = 14.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(Icons.Filled.Search, null, tint = p.textDim)
                        Spacer(Modifier.padding(4.dp))
                        Text("Sommeil, pluie, stress, histoire…", style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }
            item {
                // grille de catégories (2 colonnes)
                Column(Modifier.padding(horizontal = 20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    catalog.categories.chunked(2).forEach { row ->
                        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                            row.forEach { c ->
                                Box(
                                    Modifier.weight(1f).height(118.dp).clip(RoundedCornerShape(22.dp))
                                        .clickable { nav.navigate(Routes.category(c.id)) },
                                ) {
                                    ArtIllustration(categoryArt[c.id] ?: "moon", Modifier.fillMaxSize())
                                    Box(Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color.Transparent, Color.Black.copy(alpha = 0.6f)))))
                                    Column(Modifier.align(Alignment.BottomStart).padding(12.dp)) {
                                        Text(c.title, style = MaterialTheme.typography.titleMedium, color = Color.White)
                                        Text("${catalog.inCategory(c.id).size} séances", style = MaterialTheme.typography.bodySmall, color = Color.White.copy(alpha = 0.75f))
                                    }
                                }
                            }
                            if (row.size == 1) Spacer(Modifier.weight(1f))
                        }
                    }
                }
            }
            catalog.categories.forEach { c ->
                item(key = "row_" + c.id) {
                    Spacer(Modifier.height(24.dp))
                    SectionTitle(c.title, action = "Tout voir", onAction = { nav.navigate(Routes.category(c.id)) })
                    Text(c.subtitle, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 20.dp))
                    Spacer(Modifier.height(10.dp))
                    LazyRow(contentPadding = PaddingValues(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                        items(catalog.inCategory(c.id), key = { it.id }) { t ->
                            TrackCardLarge(t.card(catalog, snap.favorites), onClick = { playTrack(app, nav, t.id) }, onFav = { app.store.toggleFavorite(MediaIds.track(t.id)) }, width = 170.dp)
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun CategoryScreen(id: String) {
    val app = LocalApp.current
    val nav = LocalNav.current
    val snap by app.store.state.collectAsState()
    val cat = app.catalog.category(id)
    SubScreen(cat?.title ?: "Catégorie") {
        Box(Modifier.fillMaxWidth().height(180.dp).padding(horizontal = 20.dp).clip(RoundedCornerShape(26.dp))) {
            ArtIllustration(categoryArt[id] ?: "moon", Modifier.fillMaxSize())
            Text(cat?.subtitle ?: "", style = MaterialTheme.typography.titleMedium, color = Color.White, modifier = Modifier.align(Alignment.BottomStart).padding(16.dp))
        }
        Spacer(Modifier.height(12.dp))
        app.catalog.inCategory(id).forEach { t ->
            TrackRow(t.card(app.catalog, snap.favorites), onClick = { playTrack(app, nav, t.id) }, onFav = { app.store.toggleFavorite(MediaIds.track(t.id)) })
            Text(t.description, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(start = 98.dp, end = 20.dp, bottom = 8.dp))
        }
    }
}

@Composable
fun SearchScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    var q by rememberSaveable { mutableStateOf("") }
    val hits = remember(q) { Discovery.search(app.catalog, q) }
    val sounds = remember(q) { Sounds.all.filter { q.length >= 2 && Discovery.normalize(it.name).contains(Discovery.normalize(q)) } }
    val focus = remember { FocusRequester() }
    LaunchedEffect(Unit) { runCatching { focus.requestFocus() } }
    SubScreen("Recherche", scroll = false) {
        OutlinedTextField(
            value = q, onValueChange = { q = it },
            modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp).focusRequester(focus),
            placeholder = { Text("Que cherches-tu ce soir ?") },
            leadingIcon = { Icon(Icons.Filled.Search, null) },
            singleLine = true, shape = RoundedCornerShape(50),
            colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = p.accent, unfocusedBorderColor = p.cardBorder, cursorColor = p.moon),
        )
        if (q.length < 2) {
            Spacer(Modifier.height(16.dp))
            Text("Suggestions", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(horizontal = 20.dp))
            Spacer(Modifier.height(8.dp))
            LazyRow(contentPadding = PaddingValues(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                items(listOf("dormir", "stress", "pluie", "histoire", "respiration", "confiance", "océan", "matin")) { s ->
                    com.superessence.noctea.ui.components.NightChip(s, false) { q = s }
                }
            }
        }
        LazyColumn(Modifier.fillMaxSize().imePadding(), contentPadding = PaddingValues(bottom = 40.dp)) {
            if (q.length >= 2 && hits.isEmpty() && sounds.isEmpty()) {
                item {
                    EmptyState("Rien trouvé pour « $q ».\nEssaie « sommeil », « respiration » ou crée ta propre méditation.")
                    Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                        com.superessence.noctea.ui.components.SecondaryButton("Créer ma méditation") { nav.navigate(Routes.CREATE) }
                    }
                }
            }
            items(hits, key = { it.track.id }) { h ->
                TrackRow(h.track.card(app.catalog, snap.favorites), onClick = { playTrack(app, nav, h.track.id) }, onFav = { app.store.toggleFavorite(MediaIds.track(h.track.id)) })
            }
            if (sounds.isNotEmpty()) {
                item { SectionTitle("Ambiances") }
                items(sounds, key = { "s_" + it.id }) { s ->
                    Text(
                        "${s.name} — ${s.description}",
                        style = MaterialTheme.typography.bodyLarge,
                        modifier = Modifier.fillMaxWidth().clickable {
                            app.store.edit { it.copy(lastMix = mapOf(s.id to 0.6f)) }
                            nav.navigate(Routes.MIXER)
                        }.padding(horizontal = 20.dp, vertical = 12.dp),
                    )
                }
            }
        }
    }
}

@Composable
fun OnboardingScreen() {
    val app = LocalApp.current
    val p = LocalPalette.current
    var name by rememberSaveable { mutableStateOf("") }
    NightBackground {
        Column(
            Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding().imePadding().padding(28.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            com.superessence.noctea.ui.components.MoonMark(120.dp)
            Spacer(Modifier.height(16.dp))
            Text("Bienvenue dans NOCTÉA", style = MaterialTheme.typography.displayMedium)
            Spacer(Modifier.height(10.dp))
            Text(
                "Ton accompagnatrice du soir : méditations guidées, respirations, histoires et ambiances pour retrouver le calme et le sommeil.",
                style = MaterialTheme.typography.bodyLarge, color = p.textDim,
            )
            Spacer(Modifier.height(28.dp))
            Text("Comment souhaites-tu que l'on t'appelle ?", style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(8.dp))
            OutlinedTextField(
                value = name, onValueChange = { name = it.take(24) }, singleLine = true,
                placeholder = { Text("Ton prénom (facultatif)") },
                modifier = Modifier.fillMaxWidth(), shape = RoundedCornerShape(18.dp),
                colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = p.accent, unfocusedBorderColor = p.cardBorder, cursorColor = p.moon),
            )
            Spacer(Modifier.height(28.dp))
            com.superessence.noctea.ui.components.PrimaryButton("Entrer dans la nuit", Modifier.fillMaxWidth()) {
                app.store.settings { it.copy(userName = name.trim(), onboardingDone = true) }
            }
            Spacer(Modifier.height(18.dp))
            Text("Une création Superessence Studio", style = MaterialTheme.typography.labelSmall, color = p.textDim, modifier = Modifier.align(Alignment.CenterHorizontally))
        }
    }
}
