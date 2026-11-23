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
class Occupant(
    val look: Look, val fitness: Float, val seed: Int,
    /** Tentative de record : progression de la barre (0..1), -1 sinon. */
    val grind: Float = -1f,
    val strain: Float = 0f,
    val label: String? = null,
)

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

    private fun label(c: Canvas, text: String?, x: Float, y: Float) {
        if (text == null) return
        txt.textSize = 11f
        txt.color = 0xFF000000.toInt(); c.drawText(text, x + 0.8f, y + 0.8f, txt)
        txt.color = YEL; c.drawText(text, x, y, txt)
    }

    private fun occupantBox(c: Canvas, o: Occupant, act: Act, l: Float, t: Float, time: Float, facing: Float = 1f, punch: Float = 0.1f) {
        pose.act = act; pose.t = time; pose.fitness = o.fitness; pose.seed = o.seed; pose.facing = facing; pose.punch = punch
        pose.fx = null; pose.carry = null; pose.talking = false; pose.strain = o.strain; pose.k = o.grind
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
                if (occ != null) MascotPainter.drawBenchPress(c, occ.look, t, occ.fitness, occ.grind, occ.strain, occ.label)
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
                if (occ != null) { occupantBox(c, occ, Act.SQUAT, 26f, 40f, t); label(c, occ.label, 65f, 30f) }
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
            PropType.TREADMILL -> {
                if (!carried) shadow(c, 6f, 146f, 114f)
                // console et montants
                line(c, 128f, 102f, 136f, 34f, 5f, DARK)
                rr(c, 112f, 22f, 146f, 40f, 4f, 0xFF161616.toInt())
                rr(c, 116f, 26f, 142f, 36f, 2f, 0xFF0F3D1F.toInt())
                txt.color = 0xFF5BC26A.toInt(); txt.textSize = 7f
                c.drawText(if (occ != null) "12,5 km/h" else "0,0", 129f, 34f, txt)
                line(c, 116f, 46f, 90f, 50f, 3f, STEEL)
                // tapis
                rr(c, 8f, 98f, 142f, 112f, 6f, 0xFF242424.toInt())
                rr(c, 12f, 96f, 138f, 100f, 2f, 0xFF3D3D3D.toInt())
                if (occ != null) { val off = (t * 60f) % 12f; var x = 14f - off; while (x < 136f) { if (x > 12f) line(c, x, 97f, x, 99f, 1f, 0xFF5A5A5A.toInt()); x += 12f } }
                fill(STEEL); c.drawCircle(16f, 105f, 5f, p); c.drawCircle(134f, 105f, 5f, p)
                if (occ != null) { occupantBox(c, occ, Act.RUN, 32f, -6f, t); }
            }
            PropType.LEG_PRESS -> {
                if (!carried) shadow(c, 6f, 146f, 116f)
                val k = if (occ != null) (sin(t * 2f) + 1f) / 2f else 1f
                // rail incliné
                line(c, 60f, 104f, 140f, 24f, 6f, DARK)
                line(c, 60f, 112f, 146f, 112f, 6f, DARK)
                line(c, 140f, 24f, 146f, 112f, 5f, DARK)
                // chariot
                val cx = 96f + k * 20f; val cy = 68f - k * 20f
                c.save(); c.rotate(-45f, cx, cy)
                rr(c, cx - 3f, cy - 22f, cx + 3f, cy + 22f, 2f, 0xFF2A2A2A.toInt())
                rr(c, cx + 3f, cy - 6f, cx + 16f, cy + 6f, 1f, STEEL)
                rr(c, cx + 8f, cy - 18f, cx + 14f, cy + 18f, 2f, RED); rr(c, cx + 15f, cy - 15f, cx + 20f, cy + 15f, 2f, RED)
                c.restore()
                // siège incliné
                line(c, 22f, 64f, 44f, 100f, 8f, PAD)
                line(c, 44f, 100f, 70f, 100f, 8f, PAD)
                rr(c, 40f, 100f, 46f, 114f, 1f, DARK)
                if (occ != null) {
                    pose.t = t; pose.strain = occ.strain; pose.fitness = occ.fitness
                    MascotPainter.sideBody(c, occ.look, pose, 34f, 74f, 55f, 0f, 0f, 50f, 102f, faceUp = true, armsBehind = false,
                        legTarget = (cx - 6f) to (cy + 6f), kneeBend = -1f)
                    label(c, occ.label, 100f, 14f)
                }
            }
            PropType.ROWER -> {
                if (!carried) shadow(c, 6f, 156f, 86f)
                val k = if (occ != null) (sin(t * 2.4f) + 1f) / 2f else 0f // 1 = jambes tendues
                rr(c, 10f, 72f, 130f, 77f, 2f, DARK)
                rr(c, 12f, 77f, 18f, 86f, 1f, DARK); rr(c, 122f, 77f, 128f, 86f, 1f, DARK)
                fill(0xFF2A2A2A.toInt()); c.drawCircle(140f, 64f, 17f, p)
                fill(STEEL); c.drawCircle(140f, 64f, 5f, p)
                c.save(); c.rotate(t * (if (occ != null) 400f else 30f), 140f, 64f)
                for (i in 0..5) { c.rotate(60f, 140f, 64f); line(c, 140f, 64f, 140f, 50f, 2.4f, 0xFF4A4A4A.toInt()) }
                c.restore()
                rr(c, 110f, 60f, 118f, 72f, 2f, 0xFF2A2A2A.toInt())
                val seatX = 52f + k * 34f
                val hx = (seatX + 16f) + (116f - (seatX + 16f)) * k
                rr(c, seatX - 10f, 64f, seatX + 10f, 70f, 3f, PAD)
                line(c, 132f, 62f, hx, 58f, 1f, 0xFF6B7178.toInt())
                if (occ != null) {
                    pose.t = t; pose.strain = 0f; pose.fitness = occ.fitness
                    val lean = -115f + 50f * k
                    val tr = Math.toRadians(lean.toDouble())
                    val sx = seatX + (cos(tr) * 30f).toFloat(); val sy = 66f + (sin(tr) * 30f).toFloat()
                    MascotPainter.sideBody(c, occ.look, pose, sx, sy, lean + 180f, 0f, 0f, hx, 58f, faceUp = false, armsBehind = false,
                        legTarget = 112f to 66f, kneeBend = 1f)
                }
                rr(c, hx - 2f, 52f, hx + 2f, 64f, 1f, PAD)
            }
            PropType.PEC_DECK -> {
                if (!carried) shadow(c, 10f, 112f, 146f)
                rr(c, 56f, 6f, 64f, 146f, 2f, DARK)
                rr(c, 36f, 6f, 84f, 12f, 2f, DARK)
                rr(c, 40f, 40f, 80f, 100f, 6f, PAD)
                rr(c, 38f, 110f, 82f, 116f, 3f, PAD)
                rr(c, 56f, 116f, 64f, 146f, 1f, DARK)
                rr(c, 40f, 140f, 80f, 146f, 2f, DARK)
                if (occ != null) occupantBox(c, occ, Act.PECDECK, 21f, 34f, t)
                else { rr(c, 18f, 52f, 26f, 76f, 2f, PAD); rr(c, 94f, 52f, 102f, 76f, 2f, PAD) }
            }
            PropType.PULLUP_BAR -> {
                if (!carried) shadow(c, 4f, 116f, 166f)
                rr(c, 6f, 52f, 13f, 166f, 2f, DARK); rr(c, 107f, 52f, 114f, 166f, 2f, DARK)
                rr(c, 0f, 160f, 20f, 166f, 2f, DARK); rr(c, 100f, 160f, 120f, 166f, 2f, DARK)
                rr(c, 2f, 53f, 118f, 57f, 2f, STEEL)
                rr(c, 50f, 52f, 70f, 58f, 1f, YEL)
                if (occ != null) {
                    val acts = listOf(Act.PULLUP, Act.MUSCLEUP, Act.FRONT_LEVER)
                    val act = acts[((t / 6.4f).toInt() + occ.seed) % acts.size]
                    occupantBox(c, occ, act, 21f, 24f, t)
                    label(c, act.label.uppercase(), 60f, 12f)
                }
            }
            PropType.DIP_BARS -> {
                if (!carried) shadow(c, 10f, 110f, 136f)
                for (x in floatArrayOf(28f, 84f)) {
                    rr(c, x, 76f, x + 8f, 136f, 2f, DARK)
                    rr(c, x - 6f, 130f, x + 14f, 136f, 2f, DARK)
                    fill(STEEL); c.drawCircle(x + 4f, 76f, 5f, p); fill(PAD); c.drawCircle(x + 4f, 76f, 2.2f, p)
                }
                if (occ != null) {
                    val acts = listOf(Act.DIPS, Act.DIPS, Act.HANDSTAND)
                    val act = acts[((t / 6f).toInt() + occ.seed) % acts.size]
                    if (act == Act.HANDSTAND) occupantBox(c, occ, Act.HANDSTAND, 21f, 30f, t) else occupantBox(c, occ, Act.DIPS, 21f, 1f, t)
                }
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

    /** Objet qui traîne par terre. */
    fun clutter(c: Canvas, type: com.superessence.squad.model.Clutter, w: Float, h: Float, seed: Int) {
        val sc = minOf(w / type.w, h / type.h)
        c.save(); c.translate((w - type.w * sc) / 2f, (h - type.h * sc) / 2f); c.scale(sc, sc)
        when (type) {
            com.superessence.squad.model.Clutter.DUMBBELL -> {
                shadow(c, 4f, 42f, 19f)
                rr(c, 10f, 9f, 36f, 13f, 2f, STEEL)
                for (x in floatArrayOf(3f, 35f)) { rr(c, x, 3f, x + 8f, 19f, 2f, PAD); rr(c, x + 2f, 2f, x + 6f, 20f, 1f, 0xFF2A2A2A.toInt()) }
                txt.color = YEL; txt.textSize = 5f; c.drawText(listOf("20", "25", "32", "40")[seed % 4], 7f, 13f, txt)
            }
            com.superessence.squad.model.Clutter.PLATE -> {
                shadow(c, 3f, 31f, 31f)
                fill(if (seed % 2 == 0) RED else 0xFF1F3A5F.toInt()); c.drawCircle(17f, 17f, 15f, p)
                fill(0x33000000); c.drawCircle(17f, 17f, 10f, p)
                fill(STEEL); c.drawCircle(17f, 17f, 3f, p)
                txt.color = 0xFFFFFFFF.toInt(); txt.textSize = 6f; c.drawText(listOf("20", "15", "10", "25")[seed % 4], 17f, 9f, txt)
            }
            com.superessence.squad.model.Clutter.KETTLEBELL -> {
                shadow(c, 3f, 27f, 32f)
                s.color = 0xFF2A2A2A.toInt(); s.strokeWidth = 4f; s.style = Paint.Style.STROKE
                r.set(8f, 2f, 22f, 16f); c.drawArc(r, 180f, 180f, false, s)
                fill(if (seed % 2 == 0) 0xFF2A2A2A.toInt() else 0xFFE67E22.toInt()); c.drawCircle(15f, 22f, 11f, p)
                txt.color = YEL; txt.textSize = 6f; c.drawText("${16 + (seed % 3) * 8}", 15f, 25f, txt)
            }
            com.superessence.squad.model.Clutter.SHAKER -> {
                shadow(c, 1f, 17f, 32f)
                rr(c, 2f, 8f, 16f, 32f, 3f, 0xFF151515.toInt())
                rr(c, 2f, 18f, 16f, 25f, 0f, if (seed % 2 == 0) YEL else 0xFFF2F2F2.toInt())
                rr(c, 4f, 2f, 14f, 8f, 2f, 0xFF333333.toInt())
            }
            com.superessence.squad.model.Clutter.TOWEL -> {
                fill(if (seed % 2 == 0) 0xFFF2F2F2.toInt() else 0xFF2F6FB0.toInt())
                path.reset(); path.moveTo(2f, 12f); path.quadTo(12f, 2f, 22f, 8f); path.quadTo(32f, 14f, 38f, 6f); path.lineTo(38f, 14f); path.quadTo(30f, 18f, 20f, 15f); path.quadTo(10f, 12f, 2f, 17f); path.close(); c.drawPath(path, p)
                line(c, 6f, 13f, 34f, 12f, 1f, 0x33000000)
            }
            com.superessence.squad.model.Clutter.BAR -> {
                shadow(c, 4f, 86f, 15f)
                rr(c, 6f, 7f, 84f, 10f, 1.5f, STEEL)
                rr(c, 8f, 2f, 16f, 16f, 2f, RED); rr(c, 74f, 2f, 82f, 16f, 2f, RED)
            }
        }
        c.restore()
    }

    /** Tache de bouffe sur l'écran (forme pseudo-aléatoire selon [seed]). */
    fun stain(c: Canvas, type: com.superessence.squad.model.Stain, w: Float, h: Float, seed: Int, alpha: Float) {
        val rnd = java.util.Random(seed.toLong())
        val col = type.color.toInt()
        val a = ((col ushr 24) * alpha).toInt().coerceIn(0, 255)
        val base = (a shl 24) or (col and 0xFFFFFF)
        val cx = w / 2f; val cy = h / 2f; val rad = minOf(w, h) * 0.3f
        fill(base)
        when (type) {
            com.superessence.squad.model.Stain.CRUMBS -> {
                repeat(14) {
                    val ang = rnd.nextFloat() * 6.28f; val d = rnd.nextFloat() * rad * 1.5f
                    r.set(cx + cos(ang) * d - 2.5f, cy + sin(ang) * d - 2f, cx + cos(ang) * d + 1.5f + rnd.nextFloat() * 3f, cy + sin(ang) * d + 2f)
                    c.drawRoundRect(r, 1f, 1f, fill(if (it % 3 == 0) (a shl 24) or 0x8B5A2B else base))
                }
            }
            com.superessence.squad.model.Stain.GREASE -> {
                // empreinte de doigt grasse
                s.color = (((a * 0.8f).toInt()) shl 24) or 0xB8963E; s.strokeWidth = 1.6f; s.style = Paint.Style.STROKE
                for (i in 0..5) { r.set(cx - rad * 0.6f + i * 1.5f, cy - rad + i * 2.2f, cx + rad * 0.6f - i * 1.5f, cy + rad - i * 1.2f); c.drawOval(r, s) }
                s.style = Paint.Style.STROKE
            }
            else -> {
                // éclaboussure : blob + gouttes
                path.reset()
                val n = 9
                for (i in 0..n) {
                    val ang = i * 6.283f / n
                    val rr = rad * (0.7f + rnd.nextFloat() * 0.5f)
                    val x = cx + cos(ang) * rr; val y = cy + sin(ang) * rr
                    if (i == 0) path.moveTo(x, y) else path.quadTo(cx + cos(ang - 0.35f) * rad * 1.15f, cy + sin(ang - 0.35f) * rad * 1.15f, x, y)
                }
                path.close(); c.drawPath(path, p)
                repeat(6) {
                    val ang = rnd.nextFloat() * 6.28f; val d = rad * (1.2f + rnd.nextFloat() * 0.8f)
                    c.drawCircle(cx + cos(ang) * d, cy + sin(ang) * d, 1.5f + rnd.nextFloat() * 3f, p)
                }
                if (type == com.superessence.squad.model.Stain.KETCHUP || type == com.superessence.squad.model.Stain.BBQ || type == com.superessence.squad.model.Stain.CHOCO) {
                    // coulure
                    r.set(cx - 2.5f, cy, cx + 2.5f, cy + rad * 1.6f); c.drawRoundRect(r, 2.5f, 2.5f, p)
                    c.drawCircle(cx, cy + rad * 1.6f, 3.2f, p)
                }
                fill(((a * 0.35f).toInt() shl 24) or 0xFFFFFF); c.drawCircle(cx - rad * 0.3f, cy - rad * 0.3f, rad * 0.22f, p)
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
