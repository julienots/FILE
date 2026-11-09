package com.superessence.noctea.ui.screens

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
import com.superessence.noctea.ui.components.MoonMark
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.theme.Cormorant
import com.superessence.noctea.ui.theme.Fredoka
import com.superessence.noctea.ui.theme.LocalPalette
import com.superessence.noctea.ui.theme.Nunito
import com.superessence.noctea.ui.theme.SuperYellow
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private const val LETTER_STAGGER = 170L
private const val LETTER_DURATION = 900
private val Overshoot = CubicBezierEasing(0.34f, 1.45f, 0.64f, 1f)

/**
 * Écran de lancement : présentation lente du studio « SuperEssence. »,
 * puis apparition du logo NOCTÉA. Toucher l'écran permet de passer.
 */
@Composable
fun SplashScreen(onFinished: () -> Unit) {
    val word = "SuperEssence."
    val letters = remember { word.map { Animatable(0f) } }
    val shine = remember { Animatable(-0.3f) }
    val studio = remember { Animatable(0f) }
    val studioFade = remember { Animatable(1f) }
    val noctea = remember { Animatable(0f) }
    val moonScale = remember { Animatable(0.4f) }
    val spacing = remember { Animatable(0.9f) }
    val tagline = remember { Animatable(0f) }
    var done by remember { mutableStateOf(false) }

    fun finish() {
        if (!done) { done = true; onFinished() }
    }

    LaunchedEffect(Unit) {
        delay(450)
        // 1. Les lettres apparaissent une à une, lentement
        letters.forEachIndexed { i, a ->
            launch {
                delay(i * LETTER_STAGGER + if (i >= 5) 260 else 0)
                a.animateTo(1f, tween(LETTER_DURATION, easing = Overshoot))
            }
        }
        delay(word.length * LETTER_STAGGER + 260 + LETTER_DURATION - 200L)
        // 2. Reflet lumineux qui traverse le logo + signature « STUDIO »
        launch { shine.animateTo(1.3f, tween(1400, easing = FastOutSlowInEasing)) }
        delay(350)
        studio.animateTo(1f, tween(1100))
        delay(1100)
        // 3. Transition vers NOCTÉA
        studioFade.animateTo(0f, tween(900))
        launch { noctea.animateTo(1f, tween(1200)) }
        launch { moonScale.animateTo(1f, tween(1600, easing = Overshoot)) }
        launch { spacing.animateTo(0.32f, tween(2000, easing = FastOutSlowInEasing)) }
        delay(1100)
        tagline.animateTo(1f, tween(1000))
        delay(1400)
        finish()
    }

    val pulse by rememberInfiniteTransition(label = "dot").animateFloat(
        0.6f, 1f, infiniteRepeatable(tween(1100, easing = LinearEasing), RepeatMode.Reverse), label = "p",
    )

    Box(
        Modifier
            .fillMaxSize()
            .background(Color.Black)
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {
                if (letters.last().value > 0.5f) finish()
            },
    ) {
        // ---------------------------------------------------------- Superessence Studio
        if (studioFade.value > 0f) {
            Column(
                Modifier.fillMaxSize().graphicsLayer { alpha = studioFade.value },
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) {
                Box(contentAlignment = Alignment.Center) {
                    // halo jaune derrière le point final
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
                                // ombre portée façon logo
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
                Spacer(Modifier.height(10.dp))
                Text(
                    "présente",
                    fontFamily = Cormorant, fontSize = 18.sp,
                    fontStyle = androidx.compose.ui.text.font.FontStyle.Italic,
                    color = SuperYellow.copy(alpha = 0.8f * studio.value),
                )
            }
        }

        // ---------------------------------------------------------- NOCTÉA
        if (noctea.value > 0f) {
            val p = LocalPalette.current
            NightBackground(Modifier.graphicsLayer { alpha = noctea.value }) {
                Column(
                    Modifier.fillMaxSize(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center,
                ) {
                    MoonMark(170.dp, Modifier.graphicsLayer { scaleX = moonScale.value; scaleY = moonScale.value })
                    Spacer(Modifier.height(12.dp))
                    Text(
                        "NOCTÉA", fontFamily = Cormorant, fontWeight = FontWeight.Medium, fontSize = 44.sp,
                        letterSpacing = spacing.value.em, color = p.text,
                    )
                    Spacer(Modifier.height(14.dp))
                    Text(
                        "Retrouvez le calme. Retrouvez le sommeil.",
                        fontFamily = Cormorant, fontStyle = androidx.compose.ui.text.font.FontStyle.Italic, fontSize = 19.sp,
                        color = p.accent.copy(alpha = tagline.value), textAlign = TextAlign.Center,
                    )
                }
                Text(
                    "Une création Superessence Studio",
                    style = androidx.compose.ui.text.TextStyle(fontFamily = Nunito, fontSize = 12.sp, letterSpacing = 0.08.em),
                    color = p.textDim.copy(alpha = tagline.value),
                    modifier = Modifier.align(Alignment.BottomCenter).navigationBarsPadding().padding(bottom = 28.dp),
                )
            }
        }
    }
}
