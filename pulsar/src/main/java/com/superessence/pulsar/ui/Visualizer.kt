package com.superessence.pulsar.ui

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.lerp
import com.superessence.pulsar.engine.Engine
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.ln
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/** FFT radix-2 en place (réel → magnitudes), sans allocation par image. */
class Spectrum(private val n: Int = 1024, private val bands: Int = 72) {
    private val re = FloatArray(n); private val im = FloatArray(n)
    private val window = FloatArray(n) { (0.5 - 0.5 * cos(2 * PI * it / (n - 1))).toFloat() }
    val values = FloatArray(bands)
    private val peaks = FloatArray(bands)
    private val edges = IntArray(bands + 1) { i ->
        val lo = ln(2.0); val hi = ln(n / 2.0)
        kotlin.math.exp(lo + (hi - lo) * i / bands).toInt().coerceIn(1, n / 2)
    }

    fun update(engine: Engine, playing: Boolean) {
        val src = engine.scope
        val start = engine.scopeIdx
        for (i in 0 until n) { re[i] = src[(start - n + i) and 2047] * window[i]; im[i] = 0f }
        fft()
        for (b in 0 until bands) {
            var m = 0f
            val a = edges[b]; val z = max(edges[b + 1], a + 1)
            for (k in a until z) { val v = sqrt(re[k] * re[k] + im[k] * im[k]); if (v > m) m = v }
            val db = (20f * kotlin.math.log10(m + 1e-6f) + 10f) / 60f
            val target = if (playing) db.coerceIn(0f, 1f) else 0f
            values[b] = if (target > values[b]) values[b] * 0.4f + target * 0.6f else values[b] * 0.86f + target * 0.14f
            peaks[b] = max(values[b], peaks[b] - 0.01f)
        }
    }

    private fun fft() {
        var j = 0
        for (i in 1 until n) {
            var bit = n shr 1
            while (j and bit != 0) { j = j xor bit; bit = bit shr 1 }
            j = j xor bit
            if (i < j) { var t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t }
        }
        var len = 2
        while (len <= n) {
            val ang = (-2 * PI / len).toFloat()
            val wr = cos(ang); val wi = sin(ang)
            var i = 0
            while (i < n) {
                var cr = 1f; var ci = 0f
                for (k in 0 until len / 2) {
                    val ur = re[i + k]; val ui = im[i + k]
                    val xr = re[i + k + len / 2]; val xi = im[i + k + len / 2]
                    val vr = xr * cr - xi * ci; val vi = xr * ci + xi * cr
                    re[i + k] = ur + vr; im[i + k] = ui + vi
                    re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi
                    val ncr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = ncr
                }
                i += len
            }
            len = len shl 1
        }
    }
}

@Composable
fun Visualizer(engine: Engine, playing: Boolean, colorA: Color, colorB: Color, modifier: Modifier = Modifier) {
    val spec = remember { Spectrum() }
    var frame by remember { mutableLongStateOf(0L) }
    val wave = remember { FloatArray(256) }
    LaunchedEffect(playing) {
        while (true) {
            withFrameNanos { t ->
                spec.update(engine, playing)
                val src = engine.scope; val s = engine.scopeIdx
                for (i in wave.indices) wave[i] = if (playing) src[(s - 512 + i * 2) and 2047] else wave[i] * 0.9f
                frame = t
            }
        }
    }
    Canvas(modifier) {
        frame // lecture pour invalider à chaque image
        val c = Offset(size.width / 2, size.height / 2)
        val pulse = if (playing) engine.kickPulse else 0f
        val baseR = min(size.width, size.height) * 0.22f
        val t = frame / 1e9f

        // halo de fond
        drawCircle(
            Brush.radialGradient(listOf(colorA.copy(alpha = 0.35f + 0.35f * pulse), colorB.copy(alpha = 0.08f), Color.Transparent), c, baseR * (3.2f + pulse)),
            radius = baseR * (3.2f + pulse), center = c,
        )
        // spectre circulaire
        val bands = spec.values.size
        for (i in 0 until bands) {
            val v = spec.values[i]
            val a = (i.toFloat() / bands) * 2f * PI.toFloat() - PI.toFloat() / 2 + t * 0.08f
            val r0 = baseR * (1.12f + pulse * 0.12f)
            val r1 = r0 + 6f + v * baseR * 1.35f
            val col = lerp(colorA, colorB, i.toFloat() / bands)
            drawLine(col.copy(alpha = 0.35f + 0.65f * v), Offset(c.x + cos(a) * r0, c.y + sin(a) * r0), Offset(c.x + cos(a) * r1, c.y + sin(a) * r1), strokeWidth = 5f, cap = StrokeCap.Round)
            // miroir
            val a2 = -a + PI.toFloat()
            drawLine(col.copy(alpha = 0.18f + 0.4f * v), Offset(c.x + cos(a2) * r0, c.y + sin(a2) * r0), Offset(c.x + cos(a2) * (r0 + (r1 - r0) * 0.6f), c.y + sin(a2) * (r0 + (r1 - r0) * 0.6f)), strokeWidth = 3f, cap = StrokeCap.Round)
        }
        // orbe central
        val orbR = baseR * (0.92f + pulse * 0.22f)
        drawCircle(Brush.radialGradient(listOf(Color.White.copy(alpha = 0.9f), colorA, colorB.copy(alpha = 0.85f)), c, orbR), orbR, c)
        // oscilloscope à l'intérieur de l'orbe
        val path = Path()
        for (i in wave.indices) {
            val x = c.x - orbR * 0.8f + i * (orbR * 1.6f / wave.size)
            val y = c.y + wave[i] * orbR * 0.9f
            if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
        }
        drawPath(path, Color.Black.copy(alpha = 0.55f), style = Stroke(width = 3.5f, cap = StrokeCap.Round))
        drawCircle(Color.White.copy(alpha = 0.25f + 0.5f * pulse), orbR + 3f, c, style = Stroke(2f))
    }
}
