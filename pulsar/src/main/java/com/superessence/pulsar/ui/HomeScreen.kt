package com.superessence.pulsar.ui

import androidx.compose.animation.animateColorAsState
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Radio
import androidx.compose.material.icons.filled.SkipNext
import androidx.compose.material.icons.outlined.History
import androidx.compose.material.icons.outlined.Pin
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.pulsar.engine.Family
import com.superessence.pulsar.engine.Genre
import com.superessence.pulsar.engine.Genres
import com.superessence.pulsar.player.Favorite
import com.superessence.pulsar.player.Player
import com.superessence.pulsar.player.PlayerState

fun Genre.a() = Color(colorA)
fun Genre.b() = Color(colorB)

/** Couleur d'accent lisible (la plus claire des deux) puis la seconde. */
fun Genre.accents(): Pair<Color, Color> {
    fun lum(c: Color) = 0.2126f * c.red + 0.7152f * c.green + 0.0722f * c.blue
    val a = a(); val b = b()
    return if (lum(a) >= lum(b) || lum(a) > 0.35f) a to b else b to a
}

@Composable
fun HomeScreen(st: PlayerState, initialFamily: Family = Family.CHILL, openStudio: () -> Unit) {
    var family by rememberSaveable { mutableStateOf(initialFamily) }
    var showCode by remember { mutableStateOf(false) }
    val accent by animateColorAsState(if (family == Family.CHILL) P.chill else P.mad, label = "acc")
    val accent2 by animateColorAsState(if (family == Family.CHILL) P.chill2 else P.mad2, label = "acc2")
    val favs = remember(st.revision) { Player.store.favorites() }
    val recents = remember(st.revision) { Player.store.recents() }

    Box(Modifier.fillMaxSize().background(P.bg)) {
        Box(Modifier.fillMaxWidth().height(380.dp).background(Brush.verticalGradient(listOf(accent.copy(alpha = 0.28f), Color.Transparent))))
        LazyColumn(contentPadding = PaddingValues(bottom = if (st.song != null) 110.dp else 32.dp)) {
            item {
                Column(Modifier.statusBarsPadding().padding(start = 20.dp, end = 20.dp, top = 20.dp)) {
                    Text("PULSAR", style = MaterialTheme.typography.displaySmall.copy(brush = Brush.horizontalGradient(listOf(accent, accent2))))
                    Text("Générateur de musique infini", color = P.dim, style = MaterialTheme.typography.bodyMedium)
                }
            }
            item {
                Row(Modifier.padding(20.dp).fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(P.surface).padding(5.dp)) {
                    FamilyTab("🌙  CHILL", family == Family.CHILL, P.chill, Modifier.weight(1f)) { family = Family.CHILL }
                    FamilyTab("🔥  DE MALADE", family == Family.MALADE, P.mad, Modifier.weight(1f)) { family = Family.MALADE }
                }
            }
            item {
                val active = st.radio != com.superessence.pulsar.player.RadioMode.OFF && st.radioFamily == family && st.playing
                Row(
                    Modifier.padding(horizontal = 20.dp).fillMaxWidth().clip(RoundedCornerShape(22.dp))
                        .background(Brush.linearGradient(listOf(accent, accent2)))
                        .clickable { Player.startRadio(family); openStudio() }
                        .padding(20.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(if (family == Family.CHILL) "RADIO CHILL INFINIE" else "RADIO DE MALADE INFINIE", color = Color.Black, fontWeight = FontWeight.Black, fontSize = 18.sp)
                        Text(
                            if (family == Family.CHILL) "Des morceaux doux à l'infini, tous les styles chill enchaînés." else "Bangers non-stop : trap, dubstep, hardstyle, techno… sans fin.",
                            color = Color.Black.copy(alpha = 0.75f), style = MaterialTheme.typography.bodyMedium,
                        )
                        if (active) Text("● EN COURS", color = Color.Black, fontWeight = FontWeight.Bold, fontSize = 12.sp, modifier = Modifier.padding(top = 6.dp))
                    }
                    Box(Modifier.size(56.dp).clip(CircleShape).background(Color.Black.copy(alpha = 0.85f)), contentAlignment = Alignment.Center) {
                        Icon(Icons.Filled.Radio, null, tint = accent2, modifier = Modifier.size(28.dp))
                    }
                }
            }
            item { SectionTitle("Styles", "${Genres.of(family).size} genres · un morceau unique à chaque fois") }
            val genres = Genres.of(family)
            items(genres.chunked(2)) { row ->
                Row(Modifier.padding(horizontal = 14.dp), horizontalArrangement = Arrangement.spacedBy(0.dp)) {
                    row.forEach { g -> GenreCard(g, st.song?.genre?.id == g.id && st.playing, Modifier.weight(1f).padding(6.dp)) { Player.playGenre(g.id); openStudio() } }
                    if (row.size == 1) Spacer(Modifier.weight(1f))
                }
            }
            item {
                Row(Modifier.padding(20.dp).fillMaxWidth().clip(RoundedCornerShape(16.dp)).border(1.dp, P.line, RoundedCornerShape(16.dp)).clickable { showCode = true }.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Outlined.Pin, null, tint = accent)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text("Entrer un code de morceau", style = MaterialTheme.typography.titleMedium)
                        Text("Ex. TRAP-1A2B3C : retrouve exactement le même son", color = P.dim, style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }
            if (favs.isNotEmpty()) {
                item { SectionTitle("❤  Favoris", "${favs.size} morceau(x) sauvegardé(s)") }
                items(favs, key = { "f" + it.code + it.time }) { f -> SongRow(f, true) { Player.loadFavorite(f); openStudio() } }
            }
            if (recents.isNotEmpty()) {
                item { SectionTitle("Écoutés récemment", null, Icons.Outlined.History) }
                items(recents.take(10), key = { "r" + it.code + it.time }) { f -> SongRow(f, false) { Player.loadFavorite(f); openStudio() } }
            }
        }
        if (st.song != null) MiniPlayer(st, Modifier.align(Alignment.BottomCenter).navigationBarsPadding(), openStudio)
    }

    if (showCode) {
        var code by remember { mutableStateOf("") }
        var err by remember { mutableStateOf(false) }
        AlertDialog(
            onDismissRequest = { showCode = false },
            title = { Text("Code de morceau") },
            text = {
                Column {
                    OutlinedTextField(code, { code = it; err = false }, singleLine = true, placeholder = { Text("TRAP-1A2B3C") }, isError = err)
                    if (err) Text("Code invalide", color = P.mad, modifier = Modifier.padding(top = 6.dp))
                }
            },
            confirmButton = { TextButton({ if (Player.loadCode(code)) { showCode = false; openStudio() } else err = true }) { Text("Écouter") } },
            dismissButton = { TextButton({ showCode = false }) { Text("Annuler") } },
        )
    }
}

@Composable
private fun FamilyTab(label: String, selected: Boolean, color: Color, modifier: Modifier, onClick: () -> Unit) {
    val bg by animateColorAsState(if (selected) color else Color.Transparent, label = "tab")
    Box(modifier.clip(RoundedCornerShape(14.dp)).background(bg).clickable(onClick = onClick).padding(vertical = 14.dp), contentAlignment = Alignment.Center) {
        Text(label, color = if (selected) Color.Black else P.dim, fontWeight = FontWeight.Black, letterSpacing = 1.sp)
    }
}

@Composable
fun SectionTitle(title: String, sub: String?, icon: androidx.compose.ui.graphics.vector.ImageVector? = null) {
    Row(Modifier.padding(start = 20.dp, end = 20.dp, top = 22.dp, bottom = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        if (icon != null) { Icon(icon, null, tint = P.dim); Spacer(Modifier.width(8.dp)) }
        Column {
            Text(title, style = MaterialTheme.typography.titleLarge)
            if (sub != null) Text(sub, color = P.dim, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun GenreCard(g: Genre, playing: Boolean, modifier: Modifier, onClick: () -> Unit) {
    Column(
        modifier.height(150.dp).clip(RoundedCornerShape(20.dp))
            .background(Brush.linearGradient(listOf(g.a(), g.b())))
            .then(if (playing) Modifier.border(3.dp, Color.White, RoundedCornerShape(20.dp)) else Modifier)
            .clickable(onClick = onClick)
            .padding(14.dp),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(g.emoji, fontSize = 30.sp)
            Spacer(Modifier.weight(1f))
            Box(Modifier.clip(RoundedCornerShape(8.dp)).background(Color.Black.copy(alpha = 0.45f)).padding(horizontal = 7.dp, vertical = 3.dp)) {
                Text("${g.bpmMin}–${g.bpmMax}", color = Color.White, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            }
        }
        Column(Modifier.clip(RoundedCornerShape(10.dp)).background(Color.Black.copy(alpha = 0.38f)).padding(8.dp).fillMaxWidth()) {
            Text(g.name, color = Color.White, fontWeight = FontWeight.Black, fontSize = 15.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(g.tagline, color = Color.White.copy(alpha = 0.85f), fontSize = 11.sp, maxLines = 2, lineHeight = 13.sp, overflow = TextOverflow.Ellipsis)
        }
    }
}

@Composable
private fun SongRow(f: Favorite, fav: Boolean, onClick: () -> Unit) {
    val g = Genres.byId(f.genreId)
    Row(Modifier.fillMaxWidth().clickable(onClick = onClick).padding(horizontal = 20.dp, vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(46.dp).clip(RoundedCornerShape(12.dp)).background(Brush.linearGradient(listOf(g.a(), g.b()))), contentAlignment = Alignment.Center) { Text(g.emoji, fontSize = 22.sp) }
        Spacer(Modifier.width(14.dp))
        Column(Modifier.weight(1f)) {
            Text(f.title, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text("${g.name} · ${f.bpm} BPM · ${f.key}", color = P.dim, style = MaterialTheme.typography.bodyMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        if (fav) IconButton({ Player.removeFavorite(f) }) { Icon(Icons.Filled.Favorite, "Retirer", tint = P.mad) }
        else Text(f.code.substringAfter('-'), color = P.dim, fontSize = 11.sp)
    }
}

@Composable
private fun MiniPlayer(st: PlayerState, modifier: Modifier, open: () -> Unit) {
    val s = st.song ?: return
    Row(
        modifier.padding(12.dp).fillMaxWidth().clip(RoundedCornerShape(20.dp))
            .background(Brush.horizontalGradient(listOf(s.genre.a().copy(alpha = 0.95f), s.genre.b().copy(alpha = 0.95f))))
            .clickable(onClick = open).padding(start = 16.dp, end = 6.dp, top = 10.dp, bottom = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(s.genre.emoji, fontSize = 26.sp)
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(s.title, color = Color.White, fontWeight = FontWeight.Black, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text("${s.genre.name} · ${s.bpm.toInt()} BPM", color = Color.White.copy(alpha = 0.85f), fontSize = 12.sp, maxLines = 1)
        }
        IconButton({ Player.toggle() }) { Icon(if (st.playing) Icons.Filled.Pause else Icons.Filled.PlayArrow, "Lecture", tint = Color.White, modifier = Modifier.size(32.dp)) }
        IconButton({ Player.next() }) { Icon(Icons.Filled.SkipNext, "Suivant", tint = Color.White) }
        if (!st.playing) IconButton({ Player.stopAll() }) { Icon(Icons.Filled.Close, "Fermer", tint = Color.White) }
    }
}
