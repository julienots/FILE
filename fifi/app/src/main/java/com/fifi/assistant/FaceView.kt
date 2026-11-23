package com.fifi.assistant

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.*
import android.view.View
import android.view.animation.LinearInterpolator
import kotlin.math.*

/** Le visage de Fifi : une boule paresseuse qui dort, écoute ou parle. */
class FaceView(ctx: Context) : View(ctx) {
    var state = FifiEngine.State.IDLE
        set(v) { field = v; invalidate() }

    private var t = 0f
    private val anim = ValueAnimator.ofFloat(0f, 1000f).apply {
        duration = 1_000_000L
        repeatCount = ValueAnimator.INFINITE
        interpolator = LinearInterpolator()
        addUpdateListener { t = it.animatedValue as Float; invalidate() }
    }

    private val body = Paint(Paint.ANTI_ALIAS_FLAG)
    private val glow = Paint(Paint.ANTI_ALIAS_FLAG)
    private val dark = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF2B1B3D.toInt() }
    private val white = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE }
    private val cheek = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x66FF6F91 }
    private val lid = Paint(Paint.ANTI_ALIAS_FLAG)
    private val zzz = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xCCFFFFFF.toInt(); typeface = Typeface.DEFAULT_BOLD; textAlign = Paint.Align.CENTER
    }
    private val mouth = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF2B1B3D.toInt(); style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND
    }

    override fun onAttachedToWindow() { super.onAttachedToWindow(); anim.start() }
    override fun onDetachedFromWindow() { anim.cancel(); super.onDetachedFromWindow() }

    override fun onDraw(c: Canvas) {
        val w = width.toFloat(); val h = height.toFloat()
        val r = min(w, h) * 0.36f
        val breathe = if (state == FifiEngine.State.IDLE || state == FifiEngine.State.WAITING_WAKE) sin(t * 0.0025f * 1000 / 1.6f) * 0.03f else 0f
        val cx = w / 2; val cy = h / 2 + r * 0.05f
        val rr = r * (1 + breathe)

        // halo quand elle écoute
        if (state == FifiEngine.State.LISTENING) {
            val p = (t * 1.5f) % 1f
            glow.shader = null
            glow.color = Color.argb(((1 - p) * 120).toInt(), 120, 230, 255)
            c.drawCircle(cx, cy, rr * (1.05f + p * 0.35f), glow)
        }

        body.shader = RadialGradient(cx - rr * 0.3f, cy - rr * 0.4f, rr * 1.6f,
            intArrayOf(0xFFFFE27A.toInt(), 0xFFFFB347.toInt(), 0xFFFF8A3D.toInt()), null, Shader.TileMode.CLAMP)
        c.drawCircle(cx, cy, rr, body)

        // joues
        c.drawCircle(cx - rr * 0.55f, cy + rr * 0.2f, rr * 0.14f, cheek)
        c.drawCircle(cx + rr * 0.55f, cy + rr * 0.2f, rr * 0.14f, cheek)

        // yeux
        val eyeY = cy - rr * 0.12f
        val eyeDx = rr * 0.33f
        val eyeR = rr * 0.17f
        val lidClose = when (state) {
            FifiEngine.State.IDLE -> 0.92f
            FifiEngine.State.WAITING_WAKE -> 0.75f + 0.1f * sin(t * 0.8f)
            FifiEngine.State.LISTENING -> 0.15f
            FifiEngine.State.SPEAKING -> 0.5f   // l'air blasé
        }
        for (s in intArrayOf(-1, 1)) {
            val ex = cx + s * eyeDx
            c.drawCircle(ex, eyeY, eyeR, white)
            val look = if (state == FifiEngine.State.SPEAKING) eyeR * 0.3f * sin(t * 0.3f) else 0f
            c.drawCircle(ex + look, eyeY + eyeR * 0.25f, eyeR * 0.5f, dark)
            // paupière (couleur du corps)
            lid.color = 0xFFFFB347.toInt()
            c.save()
            c.clipRect(ex - eyeR * 1.2f, eyeY - eyeR * 1.2f, ex + eyeR * 1.2f, eyeY - eyeR + 2 * eyeR * lidClose)
            c.drawCircle(ex, eyeY, eyeR * 1.05f, lid)
            c.restore()
            mouth.strokeWidth = rr * 0.035f
            c.drawLine(ex - eyeR, eyeY - eyeR + 2 * eyeR * lidClose, ex + eyeR, eyeY - eyeR + 2 * eyeR * lidClose, mouth)
        }

        // bouche
        val my = cy + rr * 0.42f
        mouth.strokeWidth = rr * 0.06f
        when (state) {
            FifiEngine.State.SPEAKING -> {
                val open = (abs(sin(t * 9f)) * 0.6f + abs(sin(t * 23f)) * 0.4f) * rr * 0.22f + rr * 0.03f
                c.drawOval(cx - rr * 0.2f, my - open / 2, cx + rr * 0.2f, my + open / 2, dark)
            }
            FifiEngine.State.LISTENING -> c.drawCircle(cx, my, rr * 0.07f, dark)
            else -> {
                // petite moue blasée
                val path = Path().apply {
                    moveTo(cx - rr * 0.2f, my); quadTo(cx, my + rr * 0.04f, cx + rr * 0.2f, my - rr * 0.03f)
                }
                c.drawPath(path, mouth)
            }
        }

        // Zzz
        if (state == FifiEngine.State.IDLE || state == FifiEngine.State.WAITING_WAKE) {
            for (k in 0..2) {
                val p = ((t * 0.35f) + k / 3f) % 1f
                zzz.textSize = rr * (0.18f + 0.12f * p)
                zzz.alpha = ((1 - p) * 220).toInt()
                c.drawText("z", cx + rr * (0.8f + p * 0.5f), cy - rr * (0.6f + p * 0.8f), zzz)
            }
        }
    }
}
