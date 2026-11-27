package com.superessence.pleinphare.ui

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
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
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private const val LETTER_STAGGER = 130L
private const val LETTER_DURATION = 750
private val Overshoot = CubicBezierEasing(0.34f, 1.45f, 0.64f, 1f)

/**
 * Chargement : présentation du studio « SuperEssence. » (lettre par lettre, ombre portée,
 * reflet lumineux), puis le logo Plein Phare dont les faisceaux s'allument.
 * Toucher l'écran pour passer.
 */
@Composable
fun SplashScreen(onFinished: () -> Unit) {
    val word = "SuperEssence."
    val letters = remember { word.map { Animatable(0f) } }
    val shine = remember { Animatable(-0.3f) }
    val studio = remember { Animatable(0f) }
    val studioFade = remember { Animatable(1f) }
    val app = remember { Animatable(0f) }
    val beams = remember { Animatable(0f) }
    val title = remember { Animatable(0f) }
    val tagline = remember { Animatable(0f) }
    var done by remember { mutableStateOf(false) }

    fun finish() {
        if (!done) { done = true; onFinished() }
    }

    LaunchedEffect(Unit) {
        delay(300)
        // 1. SuperEssence. lettre par lettre
        letters.forEachIndexed { i, a ->
            launch {
                delay(i * LETTER_STAGGER + if (i >= 5) 200 else 0)
                a.animateTo(1f, tween(LETTER_DURATION, easing = Overshoot))
            }
        }
        delay(word.length * LETTER_STAGGER + 200 + LETTER_DURATION - 150L)
        // 2. Reflet lumineux + « STUDIO présente »
        launch { shine.animateTo(1.3f, tween(1200, easing = FastOutSlowInEasing)) }
        delay(300)
        studio.animateTo(1f, tween(800))
        delay(800)
        // 3. Plein Phare : les phares s'allument
        studioFade.animateTo(0f, tween(600))
        launch { app.animateTo(1f, tween(500)) }
        beams.animateTo(1f, tween(900, easing = Overshoot))
        launch { title.animateTo(1f, tween(700, easing = FastOutSlowInEasing)) }
        delay(500)
        tagline.animateTo(1f, tween(700))
        delay(900)
        finish()
    }

    val pulse by rememberInfiniteTransition(label = "pulse").animateFloat(
        0.6f, 1f, infiniteRepeatable(tween(1000, easing = LinearEasing), RepeatMode.Reverse), label = "p",
    )

    Box(
        Modifier
            .fillMaxSize()
            .background(Color.Black)
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {
                if (letters.last().value > 0.5f) finish()
            },
    ) {
        // -------------------------------------------------------------- SuperEssence Studio
        if (studioFade.value > 0f) {
            Column(
                Modifier.fillMaxSize().graphicsLayer { alpha = studioFade.value },
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Canvas(Modifier.size(width = 330.dp, height = 90.dp)) {
                        val a = letters.last().value.coerceIn(0f, 1f)
                        val c = Offset(size.width * 0.97f, size.height * 0.72f)
                        drawCircle(
                            Brush.radialGradient(listOf(SuperYellow.copy(alpha = 0.45f * a * pulse), Color.Transparent), c, size.height * 0.7f),
                            size.height * 0.7f, c,
                        )
                    }
                    Row(
                        Modifier
                            .graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)
                            .drawWithContent {
                                drawContent()
                                val x = shine.value * size.width
                                drawRect(
                                    Brush.linearGradient(
                                        listOf(Color.Transparent, Color.White.copy(alpha = 0.75f), Color.Transparent),
                                        start = Offset(x - size.width * 0.18f, 0f), end = Offset(x + size.width * 0.18f, size.height),
                                    ),
                                    blendMode = BlendMode.SrcAtop,
                                )
                            },
                    ) {
                        word.forEachIndexed { i, ch ->
                            val v = letters[i].value
                            val color = if (i < 5) Color.White else SuperYellow
                            Box(
                                Modifier.graphicsLayer {
                                    alpha = v.coerceIn(0f, 1f)
                                    translationY = (1f - v) * 38.dp.toPx()
                                    scaleX = 0.55f + 0.45f * v
                                    scaleY = 0.55f + 0.45f * v
                                },
                            ) {
                                Text(
                                    ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 44.sp,
                                    color = Color(0xFF2B2B2B), modifier = Modifier.padding(start = 3.dp, top = 4.dp),
                                )
                                Text(ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 44.sp, color = color)
                            }
                        }
                    }
                }
                Spacer(Modifier.height(18.dp))
                Text(
                    "S T U D I O",
                    fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 13.sp, letterSpacing = 0.5.em,
                    color = Color.White.copy(alpha = 0.75f * studio.value),
                    modifier = Modifier.graphicsLayer { translationY = (1 - studio.value) * 12.dp.toPx() },
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "présente", fontFamily = Nunito, fontSize = 16.sp,
                    color = SuperYellow.copy(alpha = 0.85f * studio.value),
                )
            }
        }

        // -------------------------------------------------------------- Plein Phare
        if (app.value > 0f) {
            Column(
                Modifier.fillMaxSize().graphicsLayer { alpha = app.value },
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) {
                Box(contentAlignment = Alignment.Center) {
                    // halo des phares
                    Canvas(Modifier.size(260.dp)) {
                        drawCircle(
                            Brush.radialGradient(
                                listOf(SuperYellow.copy(alpha = 0.28f * beams.value.coerceIn(0f, 1f) * pulse), Color.Transparent),
                                Offset(size.width * 0.62f, size.height / 2), size.width / 2,
                            ),
                        )
                    }
                    LogoMark(Modifier.size(180.dp), beams = beams.value)
                }
                Spacer(Modifier.height(6.dp))
                Row(
                    Modifier.graphicsLayer {
                        alpha = title.value
                        translationY = (1 - title.value) * 16.dp.toPx()
                    },
                ) {
                    Text("PLEIN ", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 40.sp, color = Color.White, letterSpacing = 0.06.em)
                    Text("PHARE", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 40.sp, color = SuperYellow, letterSpacing = 0.06.em)
                }
                Spacer(Modifier.height(10.dp))
                Text(
                    "Toute ta musique, sur la route.",
                    fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 17.sp,
                    color = TextMid.copy(alpha = tagline.value), textAlign = TextAlign.Center,
                )
            }
            Text(
                "Une création SuperEssence Studio",
                fontFamily = Nunito, fontSize = 12.sp, letterSpacing = 0.08.em,
                color = TextLow.copy(alpha = tagline.value),
                modifier = Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 28.dp),
            )
        }
    }
}
