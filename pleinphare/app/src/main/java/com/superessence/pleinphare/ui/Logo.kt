package com.superessence.pleinphare.ui

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke

/** Géométrie du logo, identique à l'icône (repère 108 × 108). */
private val BEAM_Y = floatArrayOf(39f, 46.5f, 54f, 61.5f, 69f)
private val BEAM_LEN = floatArrayOf(12f, 22f, 27f, 20f, 10f)

/**
 * Logo Plein Phare : un phare de voiture dont les faisceaux sont un égaliseur.
 * [beams] (0..1) déploie les faisceaux ; [levels] module chaque barre (animation égaliseur).
 */
@Composable
fun LogoMark(
    modifier: Modifier,
    beams: Float = 1f,
    levels: FloatArray? = null,
    color: Color = SuperYellow,
    centerBeam: Color = Color.White,
) {
    Canvas(modifier) {
        val k = size.minDimension / 66f
        // On cadre la zone utile 21..87 du repère 108.
        fun x(v: Float) = (v - 21f) * k + (size.width - 66f * k) / 2f
        fun y(v: Float) = (v - 21f) * k + (size.height - 66f * k) / 2f
        val d = Path().apply {
            moveTo(x(49.5f), y(36f))
            lineTo(x(41.5f), y(36f))
            arcTo(Rect(x(23.5f), y(36f), x(59.5f), y(72f)), -90f, -180f, false)
            lineTo(x(49.5f), y(72f))
            close()
        }
        drawPath(d, color)
        drawArc(
            Color.Black.copy(alpha = 0.35f), 90f, 180f, false,
            topLeft = Offset(x(32.5f), y(45f)), size = androidx.compose.ui.geometry.Size(18f * k, 18f * k),
            style = Stroke(3f * k, cap = StrokeCap.Round),
        )
        for (i in BEAM_Y.indices) {
            val lvl = levels?.getOrNull(i) ?: 1f
            val len = BEAM_LEN[i] * beams.coerceIn(0f, 1f) * lvl
            if (len <= 0.05f) continue
            drawLine(
                if (i == 2) centerBeam else color,
                Offset(x(57f), y(BEAM_Y[i])), Offset(x(57f + len), y(BEAM_Y[i])),
                strokeWidth = 4.6f * k, cap = StrokeCap.Round,
                alpha = (0.35f + 0.65f * beams).coerceIn(0f, 1f),
            )
        }
    }
}
