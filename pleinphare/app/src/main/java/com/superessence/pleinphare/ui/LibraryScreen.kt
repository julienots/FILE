package com.superessence.pleinphare.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowDownward
import androidx.compose.material.icons.rounded.ArrowUpward
import androidx.compose.material.icons.rounded.Close
import androidx.compose.material.icons.rounded.KeyboardDoubleArrowUp
import androidx.compose.material.icons.rounded.LightMode
import androidx.compose.material.icons.rounded.Pause
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material.icons.rounded.Shuffle
import androidx.compose.material.icons.rounded.SkipNext
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.pleinphare.data.DayGroup
import com.superessence.pleinphare.data.Song
import com.superessence.pleinphare.data.formatDuration
import com.superessence.pleinphare.playback.PlayerUi
import kotlinx.coroutines.launch

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun LibraryScreen(
    groups: List<DayGroup>,
    totalCount: Int,
    loading: Boolean,
    newestFirst: Boolean,
    onToggleSort: () -> Unit,
    searchOpen: Boolean,
    onSearchOpen: (Boolean) -> Unit,
    query: String,
    onQuery: (String) -> Unit,
    keepScreenOn: Boolean,
    onToggleKeepScreenOn: () -> Unit,
    player: PlayerUi,
    currentSong: Song?,
    onPlaySong: (Song) -> Unit,
    onPlayAll: () -> Unit,
    onShuffleAll: () -> Unit,
    onOpenPlayer: () -> Unit,
    onTogglePlay: () -> Unit,
    onNext: () -> Unit,
) {
    val listState = rememberLazyListState()
    val scope = rememberCoroutineScope()
    val showTop by remember { derivedStateOf { listState.firstVisibleItemIndex > 12 } }
    val visibleCount = remember(groups) { groups.sumOf { it.songs.size } }

    Column(Modifier.fillMaxSize().background(Ink).statusBarsPadding()) {
        // ----------------------------------------------------------- en-tête
        Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 8.dp, top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            LogoMark(Modifier.size(46.dp))
            Spacer(Modifier.width(10.dp))
            Column(Modifier.weight(1f)) {
                Row {
                    Text("PLEIN ", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 24.sp, color = TextHi)
                    Text("PHARE", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 24.sp, color = SuperYellow)
                }
                Text(
                    if (loading) "Recherche de ta musique…" else "$totalCount titres",
                    fontFamily = Nunito, fontSize = 14.sp, color = TextMid,
                )
            }
            BigIcon(Icons.Rounded.LightMode, "Garder l'écran allumé", active = keepScreenOn, onClick = onToggleKeepScreenOn)
            BigIcon(if (searchOpen) Icons.Rounded.Close else Icons.Rounded.Search, "Rechercher", active = searchOpen) {
                if (searchOpen) onQuery("")
                onSearchOpen(!searchOpen)
            }
        }

        if (searchOpen) {
            TextField(
                value = query, onValueChange = onQuery, singleLine = true,
                placeholder = { Text("Titre, artiste, album…", fontSize = 18.sp) },
                leadingIcon = { Icon(Icons.Rounded.Search, null) },
                textStyle = TextStyle(fontFamily = Nunito, fontSize = 20.sp),
                shape = RoundedCornerShape(18.dp),
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = PanelHi, unfocusedContainerColor = PanelHi,
                    focusedIndicatorColor = Color.Transparent, unfocusedIndicatorColor = Color.Transparent,
                    cursorColor = SuperYellow,
                ),
                modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
            )
        }

        // ----------------------------------------------------------- grosses commandes
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 10.dp),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            PillButton("Tout lire", Icons.Rounded.PlayArrow, filled = true, modifier = Modifier.weight(1f), onClick = onPlayAll)
            PillButton("Aléatoire", Icons.Rounded.Shuffle, filled = false, modifier = Modifier.weight(1f), onClick = onShuffleAll)
            PillButton(
                if (newestFirst) "Récentes" else "Anciennes",
                if (newestFirst) Icons.Rounded.ArrowDownward else Icons.Rounded.ArrowUpward,
                filled = false, modifier = Modifier.weight(1f), onClick = onToggleSort,
            )
        }

        // ----------------------------------------------------------- liste par date
        Box(Modifier.weight(1f)) {
            if (!loading && visibleCount == 0) {
                Column(Modifier.fillMaxSize().padding(32.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
                    LogoMark(Modifier.size(110.dp), beams = 0.25f)
                    Spacer(Modifier.height(16.dp))
                    Text(
                        if (query.isNotBlank()) "Aucun titre ne correspond à « $query »." else "Aucune musique trouvée sur ce téléphone.",
                        fontFamily = Nunito, fontSize = 18.sp, color = TextMid,
                    )
                }
            }
            LazyColumn(state = listState, contentPadding = PaddingValues(bottom = 16.dp), modifier = Modifier.fillMaxSize()) {
                groups.forEach { g ->
                    stickyHeader(key = "h" + g.day.toEpochDay()) { DayHeader(g) }
                    items(g.songs, key = { it.id }) { s ->
                        SongRow(s, playing = player.currentId == s.id, isPlaying = player.isPlaying) { onPlaySong(s) }
                    }
                }
            }
            if (showTop) {
                Box(
                    Modifier.align(Alignment.BottomEnd).padding(16.dp).size(60.dp).clip(CircleShape)
                        .background(PanelHi).border(1.dp, SuperYellow.copy(alpha = 0.5f), CircleShape)
                        .clickable { scope.launch { listState.scrollToItem(0) } },
                    contentAlignment = Alignment.Center,
                ) { Icon(Icons.Rounded.KeyboardDoubleArrowUp, "Revenir en haut", tint = SuperYellow, modifier = Modifier.size(32.dp)) }
            }
        }

        if (player.hasMedia) MiniPlayer(player, currentSong, onOpenPlayer, onTogglePlay, onNext)
        else Spacer(Modifier.navigationBarsPadding())
    }
}

@Composable
private fun DayHeader(g: DayGroup) {
    Row(
        Modifier.fillMaxWidth().background(Ink).padding(start = 18.dp, end = 18.dp, top = 14.dp, bottom = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(width = 4.dp, height = 20.dp).clip(RoundedCornerShape(2.dp)).background(SuperYellow))
        Spacer(Modifier.width(10.dp))
        Text(g.label, fontFamily = Fredoka, fontWeight = FontWeight.Medium, fontSize = 19.sp, color = TextHi, modifier = Modifier.weight(1f))
        Text("${g.songs.size} titre${if (g.songs.size > 1) "s" else ""}", fontFamily = Nunito, fontSize = 14.sp, color = TextLow)
    }
}

@Composable
private fun SongRow(song: Song, playing: Boolean, isPlaying: Boolean, onClick: () -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .background(if (playing) SuperYellow.copy(alpha = 0.10f) else Color.Transparent)
            .padding(horizontal = 16.dp, vertical = 9.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(contentAlignment = Alignment.Center) {
            SongArtwork(song, 160, RoundedCornerShape(12.dp), Modifier.size(60.dp))
            if (playing) {
                Box(Modifier.size(60.dp).clip(RoundedCornerShape(12.dp)).background(Color.Black.copy(alpha = 0.55f)), contentAlignment = Alignment.Center) {
                    EqualizerBars(isPlaying, Modifier.size(28.dp))
                }
            }
        }
        Spacer(Modifier.width(14.dp))
        Column(Modifier.weight(1f)) {
            Text(
                song.title, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 19.sp,
                color = if (playing) SuperYellow else TextHi, maxLines = 1, overflow = TextOverflow.Ellipsis,
            )
            Text(
                song.artist, fontFamily = Nunito, fontSize = 15.sp, color = TextMid,
                maxLines = 1, overflow = TextOverflow.Ellipsis,
            )
        }
        Spacer(Modifier.width(8.dp))
        Text(formatDuration(song.durationMs), fontFamily = Nunito, fontSize = 14.sp, color = TextLow)
    }
}

@Composable
fun EqualizerBars(animate: Boolean, modifier: Modifier) {
    val t = rememberInfiniteTransition(label = "eq")
    val a by t.animateFloat(0.3f, 1f, infiniteRepeatable(tween(420, easing = LinearEasing), RepeatMode.Reverse), label = "a")
    val b by t.animateFloat(1f, 0.35f, infiniteRepeatable(tween(560, easing = LinearEasing), RepeatMode.Reverse), label = "b")
    val c by t.animateFloat(0.5f, 0.95f, infiniteRepeatable(tween(330, easing = LinearEasing), RepeatMode.Reverse), label = "c")
    val levels = if (animate) listOf(a, b, c) else listOf(0.4f, 0.7f, 0.5f)
    Row(modifier, horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.Bottom) {
        levels.forEach { l ->
            Box(Modifier.width(5.dp).fillMaxHeight(l).clip(RoundedCornerShape(2.dp)).background(SuperYellow))
        }
    }
}


@Composable
private fun MiniPlayer(p: PlayerUi, song: Song?, onOpen: () -> Unit, onToggle: () -> Unit, onNext: () -> Unit) {
    Column(Modifier.fillMaxWidth().background(Panel).navigationBarsPadding()) {
        // fine barre de progression
        Box(Modifier.fillMaxWidth().height(3.dp).background(PanelHi)) {
            val frac = if (p.durationMs > 0) (p.positionMs.toFloat() / p.durationMs).coerceIn(0f, 1f) else 0f
            Box(Modifier.fillMaxWidth(frac).height(3.dp).background(SuperYellow))
        }
        Row(
            Modifier.fillMaxWidth().clickable(onClick = onOpen).padding(horizontal = 12.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Artwork(p.currentId, song?.albumId ?: 0, 160, RoundedCornerShape(12.dp), Modifier.size(62.dp))
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(p.title, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 18.sp, color = TextHi, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(p.artist, fontFamily = Nunito, fontSize = 15.sp, color = TextMid, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            RoundButton(if (p.isPlaying) Icons.Rounded.Pause else Icons.Rounded.PlayArrow, if (p.isPlaying) "Pause" else "Lecture", 64.dp, filled = true, onClick = onToggle)
            Spacer(Modifier.width(6.dp))
            RoundButton(Icons.Rounded.SkipNext, "Suivant", 64.dp, filled = false, onClick = onNext)
        }
    }
}

// ------------------------------------------------------------------- éléments communs
@Composable
fun BigIcon(icon: ImageVector, label: String, active: Boolean = false, size: Dp = 56.dp, onClick: () -> Unit) {
    Box(
        Modifier.size(size).clip(CircleShape).clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Icon(icon, label, tint = if (active) SuperYellow else TextMid, modifier = Modifier.size(size * 0.5f))
    }
}

@Composable
fun RoundButton(icon: ImageVector, label: String, size: Dp, filled: Boolean, active: Boolean = false, onClick: () -> Unit) {
    Box(
        Modifier
            .size(size)
            .clip(CircleShape)
            .background(if (filled) SuperYellow else PanelHi)
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            icon, label,
            tint = when {
                filled -> Ink
                active -> SuperYellow
                else -> TextHi
            },
            modifier = Modifier.size(size * 0.55f),
        )
    }
}

@Composable
private fun PillButton(text: String, icon: ImageVector, filled: Boolean, modifier: Modifier, onClick: () -> Unit) {
    Row(
        modifier
            .height(58.dp)
            .clip(RoundedCornerShape(18.dp))
            .background(if (filled) SuperYellow else PanelHi)
            .clickable(onClick = onClick)
            .padding(horizontal = 8.dp),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(icon, null, tint = if (filled) Ink else SuperYellow, modifier = Modifier.size(24.dp))
        Spacer(Modifier.width(6.dp))
        Text(
            text, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 16.sp,
            color = if (filled) Ink else TextHi, maxLines = 1, overflow = TextOverflow.Ellipsis,
        )
    }
}
