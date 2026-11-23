package com.superessence.squad.render

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import com.superessence.squad.model.Act
import com.superessence.squad.model.Look
import com.superessence.squad.model.PropType
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin

/** Occupant d'un poste (une mascotte en train d'utiliser le matériel). */
class Occupant(val look: Look, val fitness: Float, val seed: Int)

/**
 * Dessine le matériel de muscu et de fête, dans une boîte de PropType.w x PropType.h unités,
 * avec la mascotte qui l'utilise le cas échéant.
 */
object PropPainter {
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val s = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND }
    private val txt = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD); textAlign = Paint.Align.CENTER }
    private val r = RectF()
    private val path = Path()
    private val pose = Pose()

    private const val STEEL = 0xFF9AA0A6.toInt()
    private const val DARK = 0xFF3A3F44.toInt()
    private const val PAD = 0xFF1E1E1E.toInt()
    private const val YEL = 0xFFFFC928.toInt()
    private const val RED = 0xFFC0392B.toInt()

    private fun fill(c: Int) = p.apply { color = c; style = Paint.Style.FILL }
    private fun rr(c: Canvas, l: Float, t: Float, rg: Float, b: Float, rad: Float, col: Int) { r.set(l, t, rg, b); c.drawRoundRect(r, rad, rad, fill(col)) }
    private fun line(c: Canvas, x1: Float, y1: Float, x2: Float, y2: Float, w: Float, col: Int) { s.color = col; s.strokeWidth = w; c.drawLine(x1, y1, x2, y2, s) }
    private fun shadow(c: Canvas, x1: Float, x2: Float, y: Float) { r.set(x1, y - 4f, x2, y + 4f); c.drawOval(r, fill(0x40000000)) }

    /** Mini-version portée au-dessus de la tête, dans le rectangle donné. */
    fun icon(c: Canvas, type: PropType, l: Float, t: Float, rg: Float, b: Float, time: Float) {
        val sc = minOf((rg - l) / type.w, (b - t) / type.h)
        c.save()
        c.translate(l + ((rg - l) - type.w * sc) / 2f, b - type.h * sc)
        c.scale(sc, sc)
        draw(c, type, time, null, carried = true)
        c.restore()
    }

    /** Dessine dans (w, h) pixels. [appear] 0..1 pour l'animation de pose. */
    fun draw(c: Canvas, type: PropType, w: Float, h: Float, time: Float, occ: Occupant?, appear: Float = 1f) {
        val sc = minOf(w / type.w, h / type.h)
        c.save()
        c.translate((w - type.w * sc) / 2f, h - type.h * sc)
        c.scale(sc, sc)
        if (appear < 1f) {
            val k = appear.coerceIn(0f, 1f)
            val bounce = 1f + sin(k * PI.toFloat()) * 0.12f
            c.scale(k * bounce, k * bounce, type.w / 2f, type.h.toFloat())
        }
        draw(c, type, time, occ, carried = false)
        c.restore()
    }

    private fun occupantBox(c: Canvas, o: Occupant, act: Act, l: Float, t: Float, time: Float, facing: Float = 1f, punch: Float = 0.1f) {
        pose.act = act; pose.t = time; pose.fitness = o.fitness; pose.seed = o.seed; pose.facing = facing; pose.punch = punch
        pose.fx = null; pose.carry = null; pose.talking = false
        c.save(); c.translate(l, t)
        MascotPainter.draw(c, o.look, pose, 78f, 110f)
        c.restore()
    }

    private fun draw(c: Canvas, type: PropType, t: Float, occ: Occupant?, carried: Boolean) {
        when (type) {
            PropType.BENCH -> {
                if (!carried) shadow(c, 14f, 136f, 90f)
                // montants et repose-barre
                rr(c, 24f, 14f, 29f, 64f, 2f, DARK); rr(c, 40f, 14f, 45f, 64f, 2f, DARK)
                rr(c, 22f, 26f, 47f, 30f, 1.5f, STEEL)
                // pieds
                rr(c, 28f, 60f, 34f, 90f, 2f, DARK); rr(c, 112f, 60f, 118f, 90f, 2f, DARK)
                rr(c, 20f, 86f, 42f, 90f, 2f, DARK); rr(c, 104f, 86f, 126f, 90f, 2f, DARK)
                // assise
                rr(c, 20f, 54f, 126f, 64f, 4f, PAD); rr(c, 20f, 54f, 126f, 56f, 2f, 0xFF333333.toInt())
                rr(c, 60f, 64f, 64f, 70f, 1f, YEL)
                if (occ != null) MascotPainter.drawBenchPress(c, occ.look, t, occ.fitness)
                else {
                    fill(RED); c.drawCircle(35f, 22f, 13f, p); fill(PAD); c.drawCircle(35f, 22f, 4f, p); fill(STEEL); c.drawCircle(35f, 22f, 1.8f, p)
                }
            }
            PropType.PULLEY -> {
                if (!carried) shadow(c, 20f, 118f, 146f)
                val k = if (occ != null) (sin(t * 2.4f) + 1f) / 2f else 0f
                // structure
                rr(c, 92f, 2f, 98f, 146f, 2f, DARK)
                rr(c, 38f, 2f, 98f, 8f, 2f, DARK)
                rr(c, 84f, 140f, 118f, 146f, 2f, DARK)
                line(c, 104f, 10f, 104f, 140f, 1.2f, STEEL); line(c, 114f, 10f, 114f, 140f, 1.2f, STEEL)
                // pile de poids
                val lift = k * 22f
                for (i in 0..5) rr(c, 101f, 106f - lift + i * 5.5f, 117f, 110.5f - lift + i * 5.5f, 1f, if (i == 0) YEL else 0xFF2A2A2A.toInt())
                for (i in 6..7) rr(c, 101f, 106f + i * 5.5f, 117f, 110.5f + i * 5.5f, 1f, 0xFF2A2A2A.toInt())
                line(c, 98f, 8f, 109f, 104f - lift, 1f, 0xFF6B7178.toInt())
                fill(STEEL); c.drawCircle(60f, 8f, 5f, p); fill(DARK); c.drawCircle(60f, 8f, 1.6f, p)
                // siège
                rr(c, 40f, 118f, 80f, 124f, 3f, PAD); rr(c, 57f, 124f, 63f, 146f, 1f, DARK)
                if (occ != null) {
                    line(c, 60f, 12f, 60f, 40f, 1.2f, 0xFF6B7178.toInt())
                    occupantBox(c, occ, Act.PULLDOWN, 21f, 40f, t)
                } else {
                    line(c, 60f, 12f, 60f, 60f, 1.2f, 0xFF6B7178.toInt())
                    rr(c, 36f, 58f, 84f, 62f, 2f, DARK)
                }
            }
            PropType.RACK -> {
                if (!carried) shadow(c, 6f, 124f, 146f)
                rr(c, 12f, 8f, 19f, 146f, 2f, DARK); rr(c, 111f, 8f, 118f, 146f, 2f, DARK)
                rr(c, 12f, 8f, 118f, 13f, 2f, DARK)
                rr(c, 4f, 140f, 28f, 146f, 2f, DARK); rr(c, 102f, 140f, 126f, 146f, 2f, DARK)
                for (y in 24..130 step 8) { fill(STEEL); c.drawCircle(15.5f, y.toFloat(), 1f, p); c.drawCircle(114.5f, y.toFloat(), 1f, p) }
                rr(c, 8f, 96f, 23f, 99f, 1f, STEEL); rr(c, 107f, 96f, 122f, 99f, 1f, STEEL)
                rr(c, 19f, 54f, 24f, 60f, 1f, STEEL); rr(c, 106f, 54f, 111f, 60f, 1f, STEEL)
                if (occ != null) occupantBox(c, occ, Act.SQUAT, 26f, 40f, t)
                else {
                    rr(c, 2f, 51f, 128f, 54.5f, 1.5f, STEEL)
                    rr(c, 4f, 39f, 13f, 66f, 2f, RED); rr(c, 117f, 39f, 126f, 66f, 2f, RED)
                }
            }
            PropType.BAG -> {
                if (!carried) shadow(c, 10f, 134f, 146f)
                val ph = (t * 1.6f) % 1f
                val swing = if (occ != null) sin((t * 1.6f - 0.15f) * 2f * PI.toFloat()).coerceAtLeast(0f) * 9f else sin(t * 1.2f) * 1.5f
                rr(c, 66f, 2f, 136f, 8f, 2f, DARK); rr(c, 130f, 2f, 136f, 24f, 2f, DARK)
                c.save(); c.rotate(-swing, 108f, 8f)
                line(c, 108f, 8f, 108f, 30f, 1.4f, STEEL)
                line(c, 108f, 22f, 98f, 32f, 1.2f, STEEL); line(c, 108f, 22f, 118f, 32f, 1.2f, STEEL)
                rr(c, 92f, 30f, 124f, 122f, 12f, RED)
                rr(c, 92f, 44f, 124f, 50f, 0f, PAD); rr(c, 92f, 104f, 124f, 110f, 0f, PAD)
                rr(c, 97f, 56f, 102f, 98f, 2.5f, 0x33FFFFFF)
                txt.color = YEL; txt.textSize = 9f; c.drawText("SQD", 108f, 80f, txt)
                c.restore()
                if (occ != null) occupantBox(c, occ, Act.PUNCH, 16f, 40f, t, 1f, if (ph < 0.22f) 1f else 0.1f)
            }
            PropType.DUMBBELLS -> {
                if (!carried) shadow(c, 6f, 114f, 146f)
                rr(c, 8f, 112f, 112f, 116f, 2f, DARK); rr(c, 8f, 128f, 112f, 132f, 2f, DARK)
                rr(c, 10f, 112f, 15f, 146f, 1f, DARK); rr(c, 105f, 112f, 110f, 146f, 1f, DARK)
                for (row in 0..1) for (i in 0..4) {
                    val x = 18f + i * 19f; val y = 106f + row * 16f
                    rr(c, x, y, x + 4f, y + 6f, 1f, PAD); rr(c, x + 12f, y, x + 16f, y + 6f, 1f, PAD); rr(c, x + 4f, y + 2f, x + 12f, y + 4f, 1f, STEEL)
                }
                if (occ != null) occupantBox(c, occ, Act.TRAIN, 21f, 34f, t)
            }
            PropType.SPEAKER -> {
                if (!carried) shadow(c, 4f, 66f, 126f)
                val beat = abs(sin(t * PI.toFloat() * 2.1f)).pow(4)
                rr(c, 8f, 30f, 62f, 126f, 5f, 0xFF161616.toInt())
                s.color = YEL; s.strokeWidth = 1.5f; r.set(8f, 30f, 62f, 126f); c.drawRoundRect(r, 5f, 5f, s)
                fill(0xFF2C2C2C.toInt()); c.drawCircle(35f, 52f, 9f, p); fill(0xFF0D0D0D.toInt()); c.drawCircle(35f, 52f, 4f + beat, p)
                val wr = 19f * (1f + 0.07f * beat)
                fill(0xFF2C2C2C.toInt()); c.drawCircle(35f, 94f, wr, p)
                fill(0xFF0D0D0D.toInt()); c.drawCircle(35f, 94f, wr * 0.62f, p)
                fill(0xFF3A3A3A.toInt()); c.drawCircle(35f, 94f, wr * 0.25f, p)
                fill(if (beat > 0.5f) YEL else 0xFF5A4A10.toInt()); c.drawCircle(52f, 36f, 2f, p)
                if (!carried) {
                    txt.textSize = 16f
                    for (i in 0..2) {
                        val ph = (t * 0.6f + i / 3f) % 1f
                        txt.color = intArrayOf(YEL, 0xFFFFFFFF.toInt(), 0xFF4AA3FF.toInt())[i]
                        txt.alpha = ((1f - ph) * 255).toInt()
                        c.drawText(if (i == 1) "♫" else "♪", 20f + i * 15f + sin(ph * 7f + i) * 6f, 30f - ph * 26f, txt)
                    }
                    txt.alpha = 255
                }
            }
            PropType.TABLE -> {
                if (!carried) shadow(c, 4f, 126f, 90f)
                rr(c, 14f, 54f, 19f, 90f, 1f, 0xFF5C3A21.toInt()); rr(c, 111f, 54f, 116f, 90f, 1f, 0xFF5C3A21.toInt())
                rr(c, 4f, 48f, 126f, 56f, 2f, 0xFF8E5B2E.toInt())
                fill(YEL); path.reset(); path.moveTo(4f, 52f); path.lineTo(126f, 52f); path.lineTo(120f, 68f); path.lineTo(10f, 68f); path.close(); c.drawPath(path, p)
                for (i in 0..5) { fill(0xFFE6B400.toInt()); c.drawCircle(18f + i * 19f, 68f, 3f, p) }
                // bouteilles
                rr(c, 16f, 20f, 26f, 48f, 3f, 0xFF2E7D32.toInt()); rr(c, 19f, 12f, 23f, 22f, 1f, 0xFF2E7D32.toInt())
                rr(c, 30f, 24f, 39f, 48f, 3f, 0xFF6D3B1E.toInt()); rr(c, 33f, 16f, 36f, 26f, 1f, 0xFF6D3B1E.toInt())
                rr(c, 17f, 30f, 25f, 36f, 0f, 0xFFF2F2F2.toInt())
                // gobelets
                for (i in 0..2) {
                    val x = 48f + i * 12f
                    fill(RED); path.reset(); path.moveTo(x, 36f); path.lineTo(x + 9f, 36f); path.lineTo(x + 7.5f, 48f); path.lineTo(x + 1.5f, 48f); path.close(); c.drawPath(path, p)
                    rr(c, x, 36f, x + 9f, 38f, 0.5f, 0xFFF2F2F2.toInt())
                }
                // saladier de chips
                fill(0xFFF4C542.toInt()); for (i in 0..5) c.drawCircle(96f + (i % 3) * 7f, 36f - (i / 3) * 4f, 4f, p)
                fill(0xFF1F3A5F.toInt()); r.set(88f, 36f, 122f, 50f); c.drawArc(r, 0f, 180f, true, p)
            }
            PropType.DISCO -> {
                if (!carried) line(c, 45f, 0f, 45f, 32f, 1.2f, 0xFFB0B0B0.toInt())
                fill(0xFFB8BCC2.toInt()); c.drawCircle(45f, 56f, 25f, p)
                c.save()
                path.reset(); path.addCircle(45f, 56f, 25f, Path.Direction.CW); c.clipPath(path)
                for (row in 0..9) for (col in 0..9) {
                    val x = 20f + col * 5f + ((t * 12f) % 5f); val y = 31f + row * 5f
                    val v = ((row * 7 + col * 3 + (t * 6f).toInt()) % 9)
                    val col2 = when (v) { 0 -> 0xFFFFFFFF.toInt(); 1 -> YEL; 2 -> 0xFF4AA3FF.toInt(); 3 -> 0xFFE57373.toInt(); 4, 5 -> 0xFF8E939A.toInt(); else -> 0xFFC9CDD2.toInt() }
                    rr(c, x, y, x + 4.3f, y + 4.3f, 0.5f, col2)
                }
                c.restore()
                fill(0x40FFFFFF); c.drawCircle(37f, 47f, 7f, p)
                if (!carried) for (i in 0..3) {
                    val a = t * 1.3f + i * 1.57f
                    val k = abs(sin(t * 3f + i))
                    val x = 45f + cos(a) * 36f; val y = 60f + sin(a) * 30f
                    sparkle(c, x, y, 3f + 4f * k, if (i % 2 == 0) YEL else 0xFFFFFFFF.toInt())
                }
            }
        }
    }

    private fun sparkle(c: Canvas, x: Float, y: Float, rad: Float, col: Int) {
        path.reset()
        path.moveTo(x, y - rad); path.lineTo(x + rad * 0.25f, y - rad * 0.25f); path.lineTo(x + rad, y); path.lineTo(x + rad * 0.25f, y + rad * 0.25f)
        path.lineTo(x, y + rad); path.lineTo(x - rad * 0.25f, y + rad * 0.25f); path.lineTo(x - rad, y); path.lineTo(x - rad * 0.25f, y - rad * 0.25f); path.close()
        c.drawPath(path, fill(col))
    }
}
