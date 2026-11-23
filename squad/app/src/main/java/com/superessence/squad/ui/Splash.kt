package com.superessence.squad.ui

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private val Overshoot = CubicBezierEasing(0.34f, 1.5f, 0.64f, 1f)

/**
 * Ouverture SuperEssence Studio : l'hexagone tourne et se pose, le S se découpe,
 * le nom apparaît lettre par lettre, un reflet traverse le tout. Toucher pour passer.
 */
@Composable
fun StudioSplash(onDone: () -> Unit) {
    val word = "SuperEssence."
    val mark = remember { Animatable(0f) }
    val cut = remember { Animatable(0f) }
    val lift = remember { Animatable(0f) }
    val letters = remember { word.map { Animatable(0f) } }
    val studio = remember { Animatable(0f) }
    val shine = remember { Animatable(-0.4f) }
    val out = remember { Animatable(1f) }
    var done by remember { mutableStateOf(false) }
    fun finish() { if (!done) { done = true; onDone() } }

    LaunchedEffect(Unit) {
        delay(250)
        mark.animateTo(1f, tween(900, easing = Overshoot))
        cut.animateTo(1f, tween(350))
        delay(150)
        launch { lift.animateTo(1f, tween(700, easing = FastOutSlowInEasing)) }
        delay(250)
        letters.forEachIndexed { i, a -> launch { delay(i * 55L); a.animateTo(1f, tween(520, easing = Overshoot)) } }
        delay(word.length * 55L + 420)
        launch { shine.animateTo(1.4f, tween(1000, easing = FastOutSlowInEasing)) }
        studio.animateTo(1f, tween(650))
        delay(1100)
        out.animateTo(0f, tween(450))
        finish()
    }

    val t = animTime()
    Box(
        Modifier.fillMaxSize().background(Color.Black)
            .clickable(remember { MutableInteractionSource() }, null) { finish() },
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier.graphicsLayer { alpha = out.value },
            horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
        ) {
            // ---- Marque
            Box(Modifier.graphicsLayer { translationY = -lift.value * 18.dp.toPx() }, contentAlignment = Alignment.Center) {
                Canvas(Modifier.size(150.dp)) {
                    val m = mark.value
                    // halo
                    drawCircle(Brush.radialGradient(listOf(C.yellow.copy(alpha = 0.28f * m * (0.85f + 0.15f * kotlin.math.sin(t * 2.4f))), Color.Transparent), center, size.minDimension * 0.55f))
                    studioMark(size.minDimension * 0.62f * m.coerceAtLeast(0.001f), center, cut.value, rotation = (1f - m) * -120f)
                }
            }
            Spacer(Modifier.height(6.dp))
            // ---- Nom
            Row(
                Modifier
                    .graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)
                    .drawWithContent {
                        drawContent()
                        val x = shine.value * size.width
                        drawRect(
                            Brush.linearGradient(
                                listOf(Color.Transparent, Color.White.copy(alpha = 0.8f), Color.Transparent),
                                start = Offset(x - size.width * 0.15f, 0f), end = Offset(x + size.width * 0.15f, size.height),
                            ),
                            blendMode = BlendMode.SrcAtop,
                        )
                    },
            ) {
                word.forEachIndexed { i, ch ->
                    val v = letters[i].value
                    val col = if (i < 5) Color.White else C.yellow
                    Box(Modifier.graphicsLayer {
                        alpha = v.coerceIn(0f, 1f)
                        translationY = (1f - v) * 26.dp.toPx()
                        scaleX = 0.4f + 0.6f * v; scaleY = 0.4f + 0.6f * v
                    }) {
                        Text(ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 38.sp, color = Color(0xFF3A3A3A), modifier = Modifier.padding(start = 2.5.dp, top = 3.dp))
                        Text(ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 38.sp, color = col)
                    }
                }
            }
            Spacer(Modifier.height(12.dp))
            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.graphicsLayer { alpha = studio.value }) {
                Box(Modifier.width(28.dp * studio.value).height(2.dp).background(C.yellow))
                Spacer(Modifier.width(10.dp))
                Text("STUDIO", fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 0.6.em, color = Color.White.copy(alpha = 0.85f))
                Box(Modifier.width(28.dp * studio.value).height(2.dp).background(C.yellow))
            }
        }
    }
}
