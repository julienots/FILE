package com.superessence.promzy.ui

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
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
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

private val Overshoot = CubicBezierEasing(0.34f, 1.56f, 0.64f, 1f)

/**
 * Lancement : 1) « SuperEssence. » (logo du studio, lettre par lettre + reflet) ;
 * 2) l'étiquette Promzy tombe, pivote, l'éclair frappe ; 3) l'app s'ouvre. Toucher pour passer.
 */
@Composable
fun SplashScreen(onFinished: () -> Unit) {
    val word = "SuperEssence."
    val letters = remember { word.map { Animatable(0f) } }
    val shine = remember { Animatable(-0.3f) }
    val studio = remember { Animatable(0f) }
    val studioOut = remember { Animatable(1f) }
    val mark = remember { Animatable(0f) }
    val bolt = remember { Animatable(0f) }
    val flash = remember { Animatable(0f) }
    val wordmark = remember { Animatable(0f) }
    val tagline = remember { Animatable(0f) }
    var done by remember { mutableStateOf(false) }
    fun finish() { if (!done) { done = true; onFinished() } }

    LaunchedEffect(Unit) {
        delay(300)
        letters.forEachIndexed { i, a ->
            launch { delay(i * 85L + if (i >= 5) 140 else 0); a.animateTo(1f, tween(650, easing = Overshoot)) }
        }
        delay(word.length * 85L + 140 + 500)
        launch { shine.animateTo(1.3f, tween(900, easing = FastOutSlowInEasing)) }
        studio.animateTo(1f, tween(600))
        delay(700)
        studioOut.animateTo(0f, tween(450))
        // Promzy
        mark.animateTo(1f, tween(700, easing = Overshoot))
        launch { flash.animateTo(1f, tween(120)); flash.animateTo(0f, tween(500)) }
        bolt.animateTo(1f, tween(320, easing = Overshoot))
        wordmark.animateTo(1f, tween(500, easing = Overshoot))
        tagline.animateTo(1f, tween(450))
        delay(900)
        finish()
    }

    Box(
        Modifier.fillMaxSize().background(Color.Black)
            .clickable(remember { MutableInteractionSource() }, null) { if (letters.last().value > 0.5f) finish() },
    ) {
        if (studioOut.value > 0f) {
            Column(
                Modifier.fillMaxSize().graphicsLayer { alpha = studioOut.value; scaleX = 0.9f + 0.1f * studioOut.value; scaleY = scaleX },
                horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
            ) {
                Row(
                    Modifier.graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen).drawWithContent {
                        drawContent()
                        val x = shine.value * size.width
                        drawRect(
                            Brush.linearGradient(
                                listOf(Color.Transparent, Color.White.copy(alpha = 0.8f), Color.Transparent),
                                start = Offset(x - size.width * 0.18f, 0f), end = Offset(x + size.width * 0.18f, size.height),
                            ),
                            blendMode = BlendMode.SrcAtop,
                        )
                    },
                ) {
                    word.forEachIndexed { i, ch ->
                        val v = letters[i].value
                        Box(Modifier.graphicsLayer {
                            alpha = v.coerceIn(0f, 1f); translationY = (1f - v) * 34.dp.toPx()
                            scaleX = 0.5f + 0.5f * v; scaleY = scaleX
                        }) {
                            Text(ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 42.sp, color = Color(0xFF2E2E2E), modifier = Modifier.padding(start = 3.dp, top = 4.dp))
                            Text(ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 42.sp, color = if (i < 5) Color.White else Yellow)
                        }
                    }
                }
                Spacer(Modifier.height(14.dp))
                Text(
                    "S T U D I O", fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 13.sp, letterSpacing = 0.45.em,
                    color = Color.White.copy(alpha = 0.7f * studio.value),
                    modifier = Modifier.graphicsLayer { translationY = (1 - studio.value) * 10.dp.toPx() },
                )
            }
        }
        if (mark.value > 0f) {
            Column(Modifier.fillMaxSize(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
                Box(contentAlignment = Alignment.Center) {
                    Canvas(Modifier.size(260.dp)) {
                        drawCircle(Brush.radialGradient(listOf(Yellow.copy(alpha = 0.28f * mark.value + 0.35f * flash.value), Color.Transparent)))
                    }
                    PromzyMark(
                        150.dp, bolt = bolt.value,
                        tilt = -14f - 200f * (1f - mark.value),
                        modifier = Modifier.graphicsLayer {
                            scaleX = 0.2f + 0.8f * mark.value; scaleY = scaleX
                            alpha = mark.value.coerceIn(0f, 1f)
                            translationY = -(1f - mark.value) * 120.dp.toPx()
                        },
                    )
                }
                PromzyWordmark(46.sp, Modifier.graphicsLayer {
                    alpha = wordmark.value.coerceIn(0f, 1f); translationY = (1 - wordmark.value) * 24.dp.toPx()
                })
                Spacer(Modifier.height(8.dp))
                Text(
                    "Les meilleures promos, en direct.", fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 16.sp,
                    color = TextMid.copy(alpha = tagline.value),
                )
            }
            Text(
                "par SuperEssence Studio", fontFamily = Nunito, fontSize = 12.sp, letterSpacing = 0.06.em,
                color = TextLow.copy(alpha = tagline.value),
                modifier = Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 28.dp),
            )
        }
    }
}
