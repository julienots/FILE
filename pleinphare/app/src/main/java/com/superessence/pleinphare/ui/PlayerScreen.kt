package com.superessence.pleinphare.ui

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.basicMarquee
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.KeyboardArrowDown
import androidx.compose.material.icons.rounded.LightMode
import androidx.compose.material.icons.rounded.Pause
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Repeat
import androidx.compose.material.icons.rounded.RepeatOne
import androidx.compose.material.icons.rounded.Shuffle
import androidx.compose.material.icons.rounded.SkipNext
import androidx.compose.material.icons.rounded.SkipPrevious
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.media3.common.Player
import com.superessence.pleinphare.data.Song
import com.superessence.pleinphare.data.formatDuration
import com.superessence.pleinphare.playback.PlayerUi

@Composable
fun PlayerScreen(
    p: PlayerUi,
    song: Song?,
    keepScreenOn: Boolean,
    onToggleKeepScreenOn: () -> Unit,
    onClose: () -> Unit,
    onTogglePlay: () -> Unit,
    onNext: () -> Unit,
    onPrevious: () -> Unit,
    onSeek: (Long) -> Unit,
    onShuffle: () -> Unit,
    onRepeat: () -> Unit,
) {
    Box(
        Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color20, Ink, Ink))).systemBarsPadding(),
    ) {
        BoxWithConstraints(Modifier.fillMaxSize().padding(horizontal = 20.dp, vertical = 8.dp)) {
            val landscape = maxWidth > maxHeight
            if (landscape) {
                Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically) {
                    Artwork(
                        p.currentId, song?.albumId ?: 0, 640, RoundedCornerShape(24.dp),
                        Modifier.fillMaxHeight(0.86f).aspectRatio(1f), iconFraction = 0.35f,
                    )
                    Spacer(Modifier.width(24.dp))
                    Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.Center) {
                        TopBar(p, keepScreenOn, onToggleKeepScreenOn, onClose)
                        Spacer(Modifier.weight(1f))
                        Titles(p)
                        Spacer(Modifier.height(10.dp))
                        SeekBar(p, onSeek)
                        Spacer(Modifier.weight(1f))
                        Controls(p, 96.dp, onTogglePlay, onNext, onPrevious, onShuffle, onRepeat)
                        Spacer(Modifier.height(8.dp))
                    }
                }
            } else {
                Column(Modifier.fillMaxSize(), horizontalAlignment = Alignment.CenterHorizontally) {
                    TopBar(p, keepScreenOn, onToggleKeepScreenOn, onClose)
                    Spacer(Modifier.height(12.dp))
                    Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                        Artwork(
                            p.currentId, song?.albumId ?: 0, 720, RoundedCornerShape(28.dp),
                            Modifier.fillMaxWidth().aspectRatio(1f, matchHeightConstraintsFirst = true), iconFraction = 0.35f,
                        )
                    }
                    Spacer(Modifier.height(22.dp))
                    Titles(p)
                    Spacer(Modifier.height(14.dp))
                    SeekBar(p, onSeek)
                    Spacer(Modifier.height(18.dp))
                    Controls(p, 112.dp, onTogglePlay, onNext, onPrevious, onShuffle, onRepeat)
                    Spacer(Modifier.height(24.dp))
                }
            }
        }
    }
}

private val Color20 = androidx.compose.ui.graphics.Color(0xFF2A2310)

@Composable
private fun TopBar(p: PlayerUi, keepScreenOn: Boolean, onToggleKeepScreenOn: () -> Unit, onClose: () -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        BigIcon(Icons.Rounded.KeyboardArrowDown, "Fermer", size = 64.dp, onClick = onClose)
        Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
            Text("EN LECTURE", fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 13.sp, color = SuperYellow, letterSpacing = 2.sp)
            if (p.count > 0) Text("${p.index + 1} / ${p.count}", fontFamily = Nunito, fontSize = 13.sp, color = TextLow)
        }
        BigIcon(Icons.Rounded.LightMode, "Garder l'écran allumé", active = keepScreenOn, size = 64.dp, onClick = onToggleKeepScreenOn)
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun Titles(p: PlayerUi) {
    Column(Modifier.fillMaxWidth()) {
        Text(
            p.title.ifBlank { "—" }, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 30.sp, color = TextHi,
            maxLines = 1, modifier = Modifier.basicMarquee(iterations = Int.MAX_VALUE, initialDelayMillis = 2000),
        )
        Spacer(Modifier.height(4.dp))
        Text(
            p.artist, fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 21.sp, color = TextMid,
            maxLines = 1, modifier = Modifier.basicMarquee(iterations = Int.MAX_VALUE, initialDelayMillis = 2000),
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SeekBar(p: PlayerUi, onSeek: (Long) -> Unit) {
    var dragging by remember { mutableStateOf<Float?>(null) }
    val dur = p.durationMs.coerceAtLeast(1)
    val value = dragging ?: (p.positionMs.toFloat() / dur).coerceIn(0f, 1f)
    Column(Modifier.fillMaxWidth()) {
        Slider(
            value = value,
            onValueChange = { dragging = it },
            onValueChangeFinished = {
                dragging?.let { onSeek((it * dur).toLong()) }
                dragging = null
            },
            colors = SliderDefaults.colors(
                thumbColor = SuperYellow, activeTrackColor = SuperYellow, inactiveTrackColor = PanelHi,
            ),
            modifier = Modifier.fillMaxWidth().height(44.dp),
        )
        Row(Modifier.fillMaxWidth()) {
            Text(formatDuration((value * dur).toLong()), fontFamily = Nunito, fontSize = 15.sp, color = TextMid)
            Spacer(Modifier.weight(1f))
            Text(formatDuration(p.durationMs), fontFamily = Nunito, fontSize = 15.sp, color = TextMid)
        }
    }
}

@Composable
private fun Controls(
    p: PlayerUi, maxPlaySize: Dp,
    onTogglePlay: () -> Unit, onNext: () -> Unit, onPrevious: () -> Unit, onShuffle: () -> Unit, onRepeat: () -> Unit,
) {
    BoxWithConstraints(Modifier.fillMaxWidth()) {
    // Les 5 boutons occupent 3,66 × la taille du bouton lecture : on l'adapte à la largeur.
    val playSize = minOf(maxPlaySize, maxWidth / 3.9f)
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        RoundButton(Icons.Rounded.Shuffle, "Aléatoire", playSize * 0.55f, filled = false, active = p.shuffle, onClick = onShuffle)
        RoundButton(Icons.Rounded.SkipPrevious, "Précédent", playSize * 0.78f, filled = false, onClick = onPrevious)
        RoundButton(
            if (p.isPlaying) Icons.Rounded.Pause else Icons.Rounded.PlayArrow,
            if (p.isPlaying) "Pause" else "Lecture", playSize, filled = true, onClick = onTogglePlay,
        )
        RoundButton(Icons.Rounded.SkipNext, "Suivant", playSize * 0.78f, filled = false, onClick = onNext)
        RoundButton(
            if (p.repeatMode == Player.REPEAT_MODE_ONE) Icons.Rounded.RepeatOne else Icons.Rounded.Repeat,
            "Répétition", playSize * 0.55f, filled = false, active = p.repeatMode != Player.REPEAT_MODE_OFF, onClick = onRepeat,
        )
    }
    }
}
