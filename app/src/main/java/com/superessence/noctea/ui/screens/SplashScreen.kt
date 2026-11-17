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
import androidx.compose.animation.core.keyframes
import androidx.compose.animation.core.spring
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.width
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.RoundRect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.platform.LocalDensity
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

private const val LETTER_STAGGER = 75L
private const val DROP_MS = 300
private val Overshoot = CubicBezierEasing(0.34f, 1.45f, 0.64f, 1f)
private val Gravity = CubicBezierEasing(0.55f, 0f, 1f, 0.45f)
private val ShadowGrey = Color(0xFF3A3A3A)

/**
 * Écran de lancement façon studio de jeu : les lettres « SuperEssence. » tombent une à une et
 * rebondissent (écrasement à l'impact), le point final claque avec un flash, puis une barre de
 * chargement se remplit avant l'apparition du logo NOCTÉA. Toucher l'écran permet de passer.
 */
@Composable
fun SplashScreen(onFinished: () -> Unit) {
    val word = "SuperEssence."
    val drops = remember { word.map { Animatable(0f) } }
    val squash = remember { word.map { Animatable(0f) } }
    val dotPop = remember { Animatable(0f) }
    val flash = remember { Animatable(0f) }
    val shake = remember { Animatable(0f) }
    val shine = remember { Animatable(-0.3f) }
    val studio = remember { Animatable(0f) }
    val bar = remember { Animatable(0f) }
    val barIn = remember { Animatable(0f) }
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
        delay(350)
        // 1. Chute des lettres, une à une, avec écrasement à l'impact
        val last = word.lastIndex
        word.indices.forEach { i ->
            launch {
                delay(i * LETTER_STAGGER + if (i == last) 180 else 0)
                if (i == last) {
                    // le point « claque » : il surgit, grossit puis se pose
                    drops[i].snapTo(1f)
                    dotPop.animateTo(1f, spring(dampingRatio = 0.32f, stiffness = 420f))
                } else {
                    drops[i].animateTo(1f, tween(DROP_MS, easing = Gravity))
                    squash[i].animateTo(0f, keyframes {
                        durationMillis = 380
                        1f at 40
                        -0.55f at 150
                        0.2f at 250
                        0f at 380
                    })
                }
            }
        }
        delay(last * LETTER_STAGGER + 180 + 90)
        // 2. Impact final : flash jaune, secousse, reflet lumineux
        launch { flash.snapTo(1f); flash.animateTo(0f, tween(650)) }
        launch {
            shake.animateTo(0f, keyframes {
                durationMillis = 360
                1f at 40
                -0.8f at 100
                0.5f at 170
                -0.25f at 240
                0f at 360
            })
        }
        delay(280)
        launch { shine.animateTo(1.3f, tween(1000, easing = FastOutSlowInEasing)) }
        studio.animateTo(1f, tween(500, easing = Overshoot))
        // 3. Barre de chargement, par à-coups comme un vrai chargement
        barIn.animateTo(1f, tween(350))
        bar.animateTo(0.34f, tween(520, easing = FastOutSlowInEasing))
        delay(180)
        bar.animateTo(0.71f, tween(640, easing = FastOutSlowInEasing))
        delay(120)
        bar.animateTo(0.93f, tween(420, easing = FastOutSlowInEasing))
        bar.animateTo(1f, tween(260))
        delay(350)
        // 4. Transition vers NOCTÉA
        studioFade.animateTo(0f, tween(700))
        launch { noctea.animateTo(1f, tween(1100)) }
        launch { moonScale.animateTo(1f, tween(1500, easing = Overshoot)) }
        launch { spacing.animateTo(0.32f, tween(1800, easing = FastOutSlowInEasing)) }
        delay(1000)
        tagline.animateTo(1f, tween(900))
        delay(1300)
        finish()
    }

    val loop = rememberInfiniteTransition(label = "splash")
    val pulse by loop.animateFloat(
        0.6f, 1f, infiniteRepeatable(tween(1100, easing = LinearEasing), RepeatMode.Reverse), label = "p",
    )
    val stripes by loop.animateFloat(
        0f, 1f, infiniteRepeatable(tween(700, easing = LinearEasing)), label = "s",
    )

    Box(
        Modifier
            .fillMaxSize()
            .background(Color.Black)
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {
                if (dotPop.value > 0.5f) finish()
            },
    ) {
        // ---------------------------------------------------------- Superessence Studio
        if (studioFade.value > 0f) {
            // flash d'impact
            Box(
                Modifier.fillMaxSize().graphicsLayer { alpha = studioFade.value }.background(
                    Brush.radialGradient(listOf(SuperYellow.copy(alpha = 0.28f * flash.value), Color.Transparent)),
                ),
            )
            BoxWithConstraints(Modifier.fillMaxSize().graphicsLayer { alpha = studioFade.value }) {
                val density = LocalDensity.current
                // le mot occupe ~86 % de la largeur, comme sur le logo
                val fontSize = with(density) { (maxWidth * 0.87f / 6.9f).toSp() }
                val letterH = maxWidth * 0.87f / 6.9f
                Column(
                    Modifier.fillMaxSize(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center,
                ) {
                    Box(
                        contentAlignment = Alignment.Center,
                        modifier = Modifier.graphicsLayer {
                            translationX = shake.value * 7.dp.toPx()
                            translationY = -shake.value * 3.dp.toPx()
                            val s = 1f + 0.05f * shake.value.coerceAtLeast(0f)
                            scaleX = s; scaleY = s
                        },
                    ) {
                        // halo jaune centré sur le point final (hors du calque du reflet, pour ne pas être rogné)
                        Canvas(Modifier.matchParentSize()) {
                            val a = dotPop.value.coerceIn(0f, 1f)
                            val c = Offset(size.width - letterH.toPx() * 0.17f, size.height * 0.76f)
                            val rr = letterH.toPx() * 0.75f
                            drawCircle(
                                Brush.radialGradient(listOf(SuperYellow.copy(alpha = 0.45f * a * pulse), Color.Transparent), c, rr),
                                rr, c,
                            )
                        }
                        Row(
                            verticalAlignment = Alignment.Bottom,
                            modifier = Modifier
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
                                val isDot = i == word.lastIndex
                                val d = drops[i].value
                                val q = squash[i].value
                                val pop = dotPop.value
                                val color = if (i < 5) Color.White else SuperYellow
                                Box(
                                    Modifier
                                        .padding(horizontal = letterH * 0.012f)
                                        .graphicsLayer {
                                        transformOrigin = TransformOrigin(0.5f, 1f)
                                        if (isDot) {
                                            alpha = if (pop > 0.02f) 1f else 0f
                                            scaleX = pop * (1f + 0.6f * (1f - pop).coerceAtLeast(0f)); scaleY = scaleX
                                        } else {
                                            alpha = if (d > 0f) 1f else 0f
                                            translationY = -(1f - d) * 220.dp.toPx()
                                            // q > 0 : écrasé à l'impact ; q < 0 : étiré au rebond
                                            scaleY = 1f - 0.28f * q
                                            scaleX = 1f + 0.22f * q
                                            rotationZ = (1f - d) * (if (i % 2 == 0) -14f else 12f)
                                        }
                                    },
                                ) {
                                    // ombre portée façon logo
                                    Text(
                                        ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = fontSize,
                                        color = ShadowGrey,
                                        modifier = Modifier.offset(x = letterH * 0.045f, y = letterH * 0.07f),
                                    )
                                    Text(ch.toString(), fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = fontSize, color = color)
                                }
                            }
                        }
                    }
                    Spacer(Modifier.height(14.dp))
                    Text(
                        "S T U D I O",
                        fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 15.sp, letterSpacing = 0.45.em,
                        color = Color.White.copy(alpha = 0.85f * studio.value.coerceIn(0f, 1f)),
                        modifier = Modifier.graphicsLayer {
                            translationY = (1 - studio.value) * 14.dp.toPx()
                            scaleX = 0.8f + 0.2f * studio.value; scaleY = scaleX
                        },
                    )
                }
                // barre de chargement
                LoadingBar(
                    progress = bar.value, stripes = stripes,
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .navigationBarsPadding()
                        .padding(bottom = 64.dp)
                        .graphicsLayer {
                            alpha = barIn.value
                            translationY = (1f - barIn.value) * 24.dp.toPx()
                        },
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

/** Barre de chargement épaisse, contour blanc, remplissage jaune rayé qui défile. */
@Composable
private fun LoadingBar(progress: Float, stripes: Float, modifier: Modifier = Modifier) {
    Column(modifier.width(260.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            "CHARGEMENT… ${(progress * 100).toInt()} %",
            fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 0.08.em,
            color = Color.White.copy(alpha = 0.8f),
        )
        Spacer(Modifier.height(10.dp))
        Canvas(Modifier.fillMaxWidth().height(22.dp)) {
            val r = CornerRadius(size.height / 2)
            val stroke = 2.5.dp.toPx()
            drawRoundRect(Color(0xFF1A1A1A), cornerRadius = r)
            val inset = stroke + 1.5.dp.toPx()
            val innerH = size.height - inset * 2
            val fillW = (size.width - inset * 2) * progress.coerceIn(0f, 1f)
            if (fillW > 0.5f) {
                val clip = Path().apply {
                    addRoundRect(RoundRect(Rect(Offset(inset, inset), Size(fillW.coerceAtLeast(innerH), innerH)), CornerRadius(innerH / 2)))
                }
                clipPath(clip) {
                    drawRect(Brush.verticalGradient(listOf(Color(0xFFFFE07A), SuperYellow, Color(0xFFE0A800))))
                    // rayures diagonales qui défilent
                    val step = 14.dp.toPx()
                    var x = -size.height * 2 + stripes * step
                    while (x < size.width + size.height) {
                        val p = Path().apply {
                            moveTo(x, size.height); lineTo(x + size.height, 0f)
                            lineTo(x + size.height + step / 2, 0f); lineTo(x + step / 2, size.height); close()
                        }
                        drawPath(p, Color.Black.copy(alpha = 0.10f))
                        x += step
                    }
                    // reflet supérieur
                    drawRect(Color.White.copy(alpha = 0.28f), topLeft = Offset(0f, inset + innerH * 0.12f), size = Size(size.width, innerH * 0.22f))
                }
            }
            drawRoundRect(Color.White, cornerRadius = r, style = Stroke(stroke))
        }
    }
}
