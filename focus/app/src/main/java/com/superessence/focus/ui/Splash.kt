package com.superessence.focus.ui

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
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
import androidx.compose.foundation.layout.offset
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
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
private val SplashYellow = Color(0xFFFFCB2B)
private val Shadow = Color(0xFF3B3B40)

/**
 * Lancement : « SuperEssence. » apparaît lettre par lettre (ombre portée, reflet lumineux, « STUDIO »),
 * puis le logo Focus. se dessine. Toucher l'écran pour passer.
 */
@Composable
fun Splash(onDone: () -> Unit) {
    val word = "SuperEssence."
    val letters = remember { word.map { Animatable(0f) } }
    val shine = remember { Animatable(-0.3f) }
    val studio = remember { Animatable(0f) }
    val studioOut = remember { Animatable(1f) }
    val ring = remember { Animatable(0f) }
    val dot = remember { Animatable(0f) }
    val spark = remember { Animatable(0f) }
    val name = remember { Animatable(0f) }
    val tag = remember { Animatable(0f) }
    val done = remember { booleanArrayOf(false) }
    fun finish() { if (!done[0]) { done[0] = true; onDone() } }

    LaunchedEffect(Unit) {
        delay(300)
        letters.forEachIndexed { i, a ->
            launch { delay(i * 95L + if (i >= 5) 160 else 0); a.animateTo(1f, tween(650, easing = Overshoot)) }
        }
        delay(word.length * 95L + 160 + 500)
        launch { shine.animateTo(1.3f, tween(1100, easing = FastOutSlowInEasing)) }
        delay(250)
        studio.animateTo(1f, tween(700))
        delay(900)
        studioOut.animateTo(0f, tween(550))
        launch { ring.animateTo(1f, tween(900, easing = FastOutSlowInEasing)) }
        delay(350)
        launch { dot.animateTo(1f, tween(600, easing = Overshoot)) }
        delay(450)
        launch { spark.animateTo(1f, tween(500, easing = Overshoot)) }
        name.animateTo(1f, tween(600))
        tag.animateTo(1f, tween(500))
        delay(900)
        finish()
    }

    Box(
        Modifier.fillMaxSize().background(Color.Black)
            .clickable(remember { MutableInteractionSource() }, null) { if (letters.last().value > 0.5f) finish() },
        contentAlignment = Alignment.Center,
    ) {
        if (studioOut.value > 0f) {
            Column(
                Modifier.graphicsLayer { alpha = studioOut.value; val k = 0.92f + 0.08f * studioOut.value; scaleX = k; scaleY = k },
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Row(
                    Modifier
                        .graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)
                        .drawWithContent {
                            drawContent()
                            val x = shine.value * size.width
                            drawRect(
                                Brush.linearGradient(
                                    listOf(Color.Transparent, Color.White.copy(alpha = 0.7f), Color.Transparent),
                                    start = Offset(x - size.width * 0.15f, 0f), end = Offset(x + size.width * 0.15f, size.height),
                                ),
                                blendMode = BlendMode.SrcAtop,
                            )
                        },
                ) {
                    word.forEachIndexed { i, ch ->
                        val p = letters[i].value
                        val color = if (i < 5) Color.White else SplashYellow
                        Box(Modifier.graphicsLayer {
                            alpha = p.coerceIn(0f, 1f); translationY = (1f - p) * 40f
                            scaleX = 0.6f + 0.4f * p; scaleY = 0.6f + 0.4f * p
                        }) {
                            Text(ch.toString(), Modifier.offset(2.5.dp, 3.dp), color = Shadow, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 40.sp)
                            Text(ch.toString(), color = color, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 40.sp)
                        }
                    }
                }
                Spacer(Modifier.height(10.dp))
                Text(
                    "S T U D I O", Modifier.graphicsLayer { alpha = studio.value; translationY = (1 - studio.value) * 12f },
                    color = Color.White.copy(alpha = 0.55f), fontFamily = Nunito, fontWeight = FontWeight.Bold,
                    fontSize = 12.sp, letterSpacing = 0.3.em,
                )
            }
        }
        if (studioOut.value < 1f) {
            Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
                FocusMark(150.dp, ring = ring.value, dot = dot.value, spark = spark.value, glow = dot.value)
                Spacer(Modifier.height(18.dp))
                Wordmark(46.sp, Modifier.graphicsLayer { alpha = name.value; translationY = (1 - name.value) * 20f })
                Spacer(Modifier.height(6.dp))
                Text(
                    "Révise. Concentre-toi. Réussis.", Modifier.graphicsLayer { alpha = tag.value },
                    color = C.Muted, fontFamily = Nunito, fontSize = 14.sp,
                )
            }
        }
    }
}
