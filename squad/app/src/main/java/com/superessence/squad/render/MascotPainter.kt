package com.superessence.squad.render

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import com.superessence.squad.model.Accessory
import com.superessence.squad.model.Act
import com.superessence.squad.model.Beard
import com.superessence.squad.model.HairStyle
import com.superessence.squad.model.Look
import com.superessence.squad.model.Outfit
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/** Pose instantanée d'une mascotte. */
class Pose {
    var act: Act = Act.IDLE
    /** Temps en secondes (pour toutes les oscillations). */
    var t: Float = 0f
    /** -1 regarde à gauche, 1 à droite. */
    var facing: Float = 1f
    /** 0..100 : plus il est entraîné, plus il est large. */
    var fitness: Float = 30f
    var talking: Boolean = false
    var seed: Int = 0
    /** 0 = poing armé, 1 = bras tendu. */
    var punch: Float = 0f
    /** Effet BD affiché (POW!, BAM!…) et son âge en secondes. */
    var fx: String? = null
    var fxAge: Float = 0f
}

/**
 * Dessine une mascotte en vectoriel dans une boîte de 100 x 140 unités, mise à l'échelle
 * dans (w, h). Le même code sert dans l'appli et par-dessus l'écran.
 */
object MascotPainter {
    const val W = 100f
    const val H = 140f

    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val s = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
    private val txt = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD); textAlign = Paint.Align.CENTER }
    private val r = RectF()
    private val path = Path()
    private const val INK = 0xFF141414.toInt()

    private fun shade(c: Int, f: Float): Int {
        val a = Color.alpha(c)
        return Color.argb(a, (Color.red(c) * f).toInt().coerceIn(0, 255), (Color.green(c) * f).toInt().coerceIn(0, 255), (Color.blue(c) * f).toInt().coerceIn(0, 255))
    }

    private fun fill(c: Int) = p.apply { color = c; style = Paint.Style.FILL; alpha = Color.alpha(c) }
    private fun line(c: Int, w: Float) = s.apply { color = c; strokeWidth = w }

    private fun rr(l: Float, t: Float, rgt: Float, b: Float, rad: Float, paint: Paint, c: Canvas) {
        r.set(l, t, rgt, b); c.drawRoundRect(r, rad, rad, paint)
    }

    fun draw(c: Canvas, look: Look, pose: Pose, w: Float, h: Float) {
        val sc = minOf(w / W, h / H)
        c.save()
        c.translate((w - W * sc) / 2f, h - H * sc)
        c.scale(sc, sc)
        drawUnit(c, look, pose)
        c.restore()
    }

    private fun drawUnit(c: Canvas, L: Look, P: Pose) {
        val t = P.t
        val act = P.act
        val sleeping = act == Act.SLEEP
        val dragged = act == Act.DRAGGED
        val running = act == Act.RUN
        val walking = act == Act.WALK || running
        val ko = act == Act.KO
        val sitting = sleeping || ko
        val legHz = if (running) 15f else 9f
        // Respiration : lente en dormant, normale sinon
        val breathHz = if (sleeping) 0.18f else if (act == Act.TRAIN || act == Act.PUSHUP) 0.6f else 0.27f
        val breath = sin(t * 2f * PI.toFloat() * breathHz)
        val bob = when {
            walking -> -abs(sin(t * legHz)) * (if (running) 4f else 2.5f)
            act == Act.VICTORY -> -abs(sin(t * 7f)) * 6f
            act == Act.GUARD || act == Act.PUNCH -> sin(t * 8f) * 1.2f
            dragged -> 0f
            act == Act.TRAIN -> sin(t * 5f) * 0.8f
            else -> breath * 0.6f
        }
        val sitDrop = if (sitting) 20f else 0f
        val broad = (P.fitness - 30f) * 0.09f // gains visibles

        // Ombre au sol
        if (!dragged) {
            fill(0x40000000)
            r.set(24f, 131f, 76f, 139f); c.drawOval(r, p)
        }

        if (act == Act.PUSHUP) { drawPushup(c, L, P, broad); return }

        // Inclinaison du corps (course, coups, impact)
        val lean = when (act) {
            Act.RUN -> 9f * P.facing
            Act.PUNCH -> (4f + 6f * P.punch) * P.facing
            Act.HIT -> -14f * P.facing
            Act.GUARD -> -3f * P.facing
            else -> 0f
        }
        c.save()
        if (lean != 0f) c.rotate(lean, 50f, 134f)
        c.translate(0f, bob + sitDrop)

        // ---------------- Jambes
        if (sitting) {
            fill(L.pantsColor)
            rr(26f, 96f, 50f, 112f, 7f, p, c); rr(50f, 96f, 74f, 112f, 7f, p, c)
            fill(INK); rr(18f, 98f, 30f, 112f, 5f, p, c); rr(70f, 98f, 82f, 112f, 5f, p, c)
        } else {
            for (side in 0..1) {
                val x = if (side == 0) 40f else 60f
                var lift = 0f; var swing = 0f
                if (walking) lift = maxOf(0f, sin(t * legHz + side * PI.toFloat())) * (if (running) 6f else 4f)
                if (act == Act.GUARD || act == Act.PUNCH) swing = if (side == 0) 8f else -8f
                if (dragged) swing = sin(t * 7f + side * 1.7f) * 12f
                c.save()
                c.translate(x, 100f)
                c.rotate(swing)
                fill(L.pantsColor); rr(-7.5f, -2f, 7.5f, 25f - lift, 5f, p, c)
                fill(INK); rr(-8.5f, 22f - lift, 9.5f, 31f - lift, 4f, p, c)
                c.restore()
            }
        }

        // ---------------- Capuche (derrière la tête)
        if (L.outfit == Outfit.HOODIE) {
            fill(shade(L.outfitColor, 0.8f)); rr(26f, 30f, 74f, 70f, 18f, p, c)
        }
        // Cheveux longs (derrière)
        if (L.hair == HairStyle.LONG) {
            fill(L.hairColor); rr(27f, 20f, 73f, 74f, 14f, p, c)
        }

        val armSwing = if (walking) sin(t * legHz) * (if (running) 40f else 22f) else breath * 2f
        val sh = 22f - broad / 2f // épaule gauche (x)
        val shR = 78f + broad / 2f

        // ---------------- Bras arrière (selon l'activité)
        val (la, ra) = armAngles(act, t, armSwing, P)

        // ---------------- Torse
        val chestScale = 1f + breath * (if (sleeping) 0.035f else 0.02f)
        c.save()
        c.scale(1f, chestScale, 50f, 102f)
        drawTorso(c, L, sh, shR)
        c.restore()

        // ---------------- Bras
        drawArm(c, L, sh + 2f, 68f, la, isLeft = true, P = P)
        drawArm(c, L, shR - 2f, 68f, ra, isLeft = false, P = P)

        // ---------------- Cou + tête
        val headTilt = when {
            sleeping -> 14f + sin(t * 0.9f) * 2f
            ko -> 22f + sin(t * 3f) * 6f
            act == Act.HIT -> -8f * P.facing
            act == Act.PHONE -> 6f
            act == Act.TRAIN -> sin(t * 5f) * 2f
            dragged -> sin(t * 4f) * 6f
            else -> sin(t * 0.7f + P.seed) * 2f
        }
        fill(shade(L.skin, 0.9f)); rr(43f, 50f, 57f, 66f, 4f, p, c)
        c.save()
        c.rotate(headTilt, 50f, 58f)
        drawHead(c, L, P)
        c.restore()

        c.restore()

        // ---------------- Étoiles du K.O.
        if (ko) {
            for (i in 0..2) {
                val a = t * 4f + i * 2.094f
                val sx = 50f + cos(a) * 22f
                val sy = 18f + sitDrop + sin(a) * 5f
                star(c, sx, sy, 4.2f, 0xFFFFC928.toInt())
            }
        }
        // ---------------- Effet BD (POW !)
        P.fx?.let { drawFx(c, it, P) }

        // ---------------- Bulles de sommeil
        if (sleeping) {
            txt.color = 0xFFFFFFFF.toInt()
            for (i in 0..2) {
                val ph = ((t * 0.35f + i / 3f) % 1f)
                txt.textSize = 9f + ph * 9f
                txt.alpha = ((1f - ph) * 255).toInt()
                c.drawText("z", 76f + ph * 14f + sin(ph * 6f) * 3f, 40f - ph * 36f, txt)
            }
        }
    }

    /** Angles des bras (degrés, 0 = pendant, positif = vers l'extérieur). */
    private fun armAngles(act: Act, t: Float, swing: Float, P: Pose): Pair<Float, Float> = when (act) {
        Act.WALK, Act.RUN -> 8f + swing to 8f - swing
        Act.PUNCH, Act.GUARD, Act.HIGHFIVE -> {
            val front = when (act) {
                Act.PUNCH -> 45f + 50f * P.punch
                Act.HIGHFIVE -> 150f
                else -> -150f + sin(t * 8f) * 5f
            }
            val back = if (act == Act.HIGHFIVE) 12f else -150f + sin(t * 8f + 1f) * 5f
            if (P.facing >= 0f) back to front else front to back
        }
        Act.HIT -> 55f + sin(t * 20f) * 15f to 55f - sin(t * 20f) * 15f
        Act.KO -> 35f to 35f
        Act.VICTORY -> 165f + sin(t * 7f) * 10f to 165f - sin(t * 7f) * 10f
        Act.FLEX -> 125f + sin(t * 3f) * 4f to 125f + sin(t * 3f) * 4f
        Act.TRAIN -> { val k = (sin(t * 5f) + 1f) / 2f; 150f - 40f * k to 110f + 40f * k }
        Act.DRAGGED -> 160f + sin(t * 5f) * 8f to 160f - sin(t * 5f) * 8f
        Act.WAVE -> 10f to 150f + sin(t * 10f) * 18f
        Act.EAT -> 10f to -130f + sin(t * 6f) * 10f
        Act.COFFEE -> 10f to -120f
        Act.PHONE -> -100f to -110f
        Act.YAWN -> 10f to 150f
        Act.SLEEP -> 25f to 25f
        Act.TALK -> 12f + sin(t * 4f) * 10f to 12f
        else -> 10f + swing to 10f - swing
    }

    private fun drawTorso(c: Canvas, L: Look, sl: Float, sr: Float) {
        val top = 62f; val bot = 103f
        path.reset()
        path.moveTo(sl + 6f, top)
        path.lineTo(sr - 6f, top)
        path.quadTo(sr, top, sr - 1f, top + 8f)
        path.lineTo(67f, bot)
        path.lineTo(33f, bot)
        path.lineTo(sl + 1f, top + 8f)
        path.quadTo(sl, top, sl + 6f, top)
        path.close()
        val body = when (L.outfit) { Outfit.BARE -> L.skin; Outfit.APRON, Outfit.VEST -> 0xFFEDEDED.toInt(); else -> L.outfitColor }
        fill(body); c.drawPath(path, p)
        // ceinture
        fill(shade(L.pantsColor, 0.7f)); rr(33f, 98f, 67f, 104f, 2f, p, c)
        c.save(); c.clipPath(path)
        when (L.outfit) {
            Outfit.TANK -> {
                fill(L.skin); rr(sl, top - 2f, 36f, 80f, 6f, p, c); rr(64f, top - 2f, sr, 80f, 6f, p, c)
                fill(L.skin); r.set(40f, 56f, 60f, 72f); c.drawOval(r, p)
            }
            Outfit.BARE -> {
                line(shade(L.skin, 0.75f), 1.4f)
                c.drawArc(RectF(33f, 66f, 50f, 80f), 20f, 140f, false, s); c.drawArc(RectF(50f, 66f, 67f, 80f), 20f, 140f, false, s)
                c.drawLine(50f, 81f, 50f, 98f, s)
                for (y in floatArrayOf(85f, 91f)) { c.drawLine(43f, y, 48f, y, s); c.drawLine(52f, y, 57f, y, s) }
            }
            Outfit.LEATHER, Outfit.BOMBER -> {
                fill(0xFF2A2A2A.toInt()); r.set(41f, 58f, 59f, 76f); c.drawOval(r, p)
                line(0xFFB8B8B8.toInt(), 1.3f); c.drawLine(50f, 70f, 50f, 103f, s)
                fill(shade(L.outfitColor, 0.75f))
                path.reset(); path.moveTo(42f, 62f); path.lineTo(50f, 78f); path.lineTo(36f, 70f); path.close(); c.drawPath(path, p)
                path.reset(); path.moveTo(58f, 62f); path.lineTo(50f, 78f); path.lineTo(64f, 70f); path.close(); c.drawPath(path, p)
                if (L.outfit == Outfit.BOMBER) { fill(0xFFE9DCC0.toInt()); rr(sl + 4f, top - 3f, sr - 4f, top + 6f, 5f, p, c) }
            }
            Outfit.SUIT -> {
                fill(0xFFF2F2F2.toInt()); path.reset(); path.moveTo(41f, 60f); path.lineTo(59f, 60f); path.lineTo(50f, 90f); path.close(); c.drawPath(path, p)
                fill(0xFF8B1E1E.toInt()); path.reset(); path.moveTo(48f, 64f); path.lineTo(52f, 64f); path.lineTo(53f, 84f); path.lineTo(50f, 88f); path.lineTo(47f, 84f); path.close(); c.drawPath(path, p)
                line(shade(L.outfitColor, 1.8f), 1f); c.drawLine(41f, 60f, 50f, 90f, s); c.drawLine(59f, 60f, 50f, 90f, s)
            }
            Outfit.CAMO -> {
                val blots = floatArrayOf(28f, 70f, 7f, 58f, 75f, 6f, 40f, 88f, 8f, 62f, 94f, 6f, 47f, 68f, 5f, 72f, 84f, 5f)
                for (i in blots.indices step 3) {
                    fill(if (i % 2 == 0) shade(L.outfitColor, 0.6f) else shade(L.outfitColor, 1.35f))
                    r.set(blots[i] - blots[i + 2], blots[i + 1] - blots[i + 2] * 0.6f, blots[i] + blots[i + 2], blots[i + 1] + blots[i + 2] * 0.6f); c.drawOval(r, p)
                }
                line(0xFFB8B8B8.toInt(), 1f); c.drawLine(45f, 62f, 50f, 78f, s); c.drawLine(55f, 62f, 50f, 78f, s)
                fill(0xFFB8B8B8.toInt()); rr(48f, 77f, 52f, 82f, 1f, p, c)
            }
            Outfit.GI -> {
                line(shade(L.outfitColor, 0.55f), 3.5f)
                c.drawLine(40f, 60f, 58f, 92f, s); c.drawLine(60f, 60f, 45f, 82f, s)
                fill(L.skin); path.reset(); path.moveTo(42f, 60f); path.lineTo(58f, 60f); path.lineTo(50f, 74f); path.close(); c.drawPath(path, p)
                fill(INK); rr(33f, 92f, 67f, 99f, 2f, p, c)
            }
            Outfit.FLANNEL -> {
                line(shade(L.outfitColor, 0.55f), 2.2f)
                var x = 20f; while (x < 82f) { c.drawLine(x, top, x, bot, s); x += 9f }
                var y = 66f; while (y < bot) { c.drawLine(sl, y, sr, y, s); y += 9f }
                fill(0xFF2A2A2A.toInt()); r.set(43f, 58f, 57f, 70f); c.drawOval(r, p)
                line(0xFF2A2A2A.toInt(), 1f); c.drawLine(50f, 66f, 50f, 103f, s)
            }
            Outfit.FUR -> {
                fill(L.skin); path.reset(); path.moveTo(40f, 62f); path.lineTo(60f, 62f); path.lineTo(50f, 84f); path.close(); c.drawPath(path, p)
                fill(0xFFC9B79C.toInt())
                for (i in 0..8) { val x = sl + 4f + i * (sr - sl - 8f) / 8f; c.drawCircle(x, top + 2f + (i % 2) * 2f, 5f, p) }
                fill(0xFF6B4A2B.toInt()); rr(33f, 92f, 67f, 99f, 2f, p, c)
                fill(0xFFB8B8B8.toInt()); c.drawCircle(50f, 95.5f, 3f, p)
            }
            Outfit.APRON -> {
                fill(L.outfitColor); rr(35f, 70f, 65f, 108f, 4f, p, c)
                line(L.outfitColor, 2.5f); c.drawLine(37f, 71f, 41f, 60f, s); c.drawLine(63f, 71f, 59f, 60f, s)
                fill(shade(L.outfitColor, 0.75f)); rr(43f, 84f, 57f, 92f, 2f, p, c)
                fill(0x55FFFFFF); c.drawCircle(56f, 76f, 2f, p); c.drawCircle(42f, 98f, 1.6f, p)
            }
            Outfit.HOODIE -> {
                fill(shade(L.outfitColor, 0.75f)); rr(38f, 86f, 62f, 98f, 4f, p, c)
                line(0xFFEDEDED.toInt(), 1.3f); c.drawLine(46f, 64f, 45f, 76f, s); c.drawLine(54f, 64f, 55f, 76f, s)
            }
            Outfit.VEST -> {
                fill(L.outfitColor)
                path.reset(); path.moveTo(sl, top); path.lineTo(45f, top); path.lineTo(47f, bot); path.lineTo(33f, bot); path.close(); c.drawPath(path, p)
                path.reset(); path.moveTo(sr, top); path.lineTo(55f, top); path.lineTo(53f, bot); path.lineTo(67f, bot); path.close(); c.drawPath(path, p)
                fill(0xFFFFC928.toInt()); c.drawCircle(39f, 80f, 2.4f, p)
            }
        }
        c.restore()
        // ceinture visible + boucle
        fill(INK); rr(33f, 99f, 67f, 104f, 2f, p, c)
        fill(0xFFC9A227.toInt()); rr(47f, 99f, 53f, 104f, 1f, p, c)
    }

    private fun sleeveColor(L: Look): Int? = when (L.outfit) {
        Outfit.TANK, Outfit.BARE, Outfit.FUR, Outfit.APRON -> null
        Outfit.VEST -> 0xFFEDEDED.toInt()
        else -> L.outfitColor
    }

    private fun drawArm(c: Canvas, L: Look, x: Float, y: Float, angle: Float, isLeft: Boolean, P: Pose) {
        val a = if (isLeft) angle else -angle
        c.save()
        c.translate(x, y)
        c.rotate(a)
        val sleeve = sleeveColor(L)
        val len = 31f
        val wid = 7f + (P.fitness - 30f) * 0.02f
        fill(L.skin); rr(-wid, -3f, wid, len, wid, p, c)
        if (sleeve != null) { fill(sleeve); rr(-wid - 0.6f, -4f, wid + 0.6f, len - 7f, wid, p, c) }
        else if (L.outfit == Outfit.FUR) { fill(0xFF6B4A2B.toInt()); rr(-wid - 0.5f, 16f, wid + 0.5f, 24f, 3f, p, c) }
        // biceps gonflé quand il pose
        if (P.act == Act.FLEX && sleeve == null) { fill(shade(L.skin, 1.04f)); c.drawCircle(if (isLeft) -3f else 3f, 11f, wid + 2.5f, p) }
        if (L.tattoos && sleeve == null) {
            line(0xCC1F3A5F.toInt(), 1.3f)
            c.drawLine(-4f, 4f, 4f, 10f, s); c.drawLine(4f, 4f, -4f, 10f, s); c.drawCircle(0f, 15f, 2.5f, s)
        }
        // main
        val hand = if (P.act == Act.TRAIN || P.act == Act.DRAGGED) L.skin else L.skin
        val fighting = P.act == Act.PUNCH || P.act == Act.GUARD
        fill(shade(hand, 0.96f)); c.drawCircle(0f, len, if (fighting) 7.8f else 6.5f, p)
        if (fighting) { line(shade(hand, 0.7f), 0.9f); c.drawLine(-4f, len + 1f, 4f, len + 1f, s) }
        // objets tenus
        when {
            P.act == Act.TRAIN -> {
                fill(0xFF2A2A2A.toInt()); rr(-11f, len - 1.5f, 11f, len + 1.5f, 1f, p, c)
                fill(INK); rr(-14f, len - 6f, -9f, len + 6f, 2f, p, c); rr(9f, len - 6f, 14f, len + 6f, 2f, p, c)
            }
            P.act == Act.EAT && !isLeft -> {
                c.save(); c.translate(0f, len + 2f); c.rotate(30f)
                fill(0xFFF2EAD8.toInt()); rr(-1.8f, 0f, 1.8f, 10f, 1.8f, p, c); c.drawCircle(-2f, 10f, 2.3f, p); c.drawCircle(2f, 10f, 2.3f, p)
                fill(0xFF9B5523.toInt()); r.set(-6f, -13f, 6f, 2f); c.drawOval(r, p)
                fill(0x55FFFFFF); r.set(-3f, -11f, 0f, -5f); c.drawOval(r, p)
                c.restore()
            }
            P.act == Act.COFFEE && !isLeft -> {
                fill(0xFFF2F2F2.toInt()); rr(-6f, len - 9f, 6f, len + 3f, 2f, p, c)
                fill(0xFF8B1E1E.toInt()); rr(-6f, len - 5f, 6f, len - 2f, 0f, p, c)
                line(0x99FFFFFF.toInt(), 1.2f)
                val st = P.t * 3f
                c.drawLine(-2f, len - 12f - (st % 3f), -1f, len - 18f - (st % 3f), s); c.drawLine(2f, len - 12f - ((st + 1.5f) % 3f), 3f, len - 18f - ((st + 1.5f) % 3f), s)
            }
            P.act == Act.PHONE && !isLeft -> {
                fill(INK); rr(-5f, len - 5f, 5f, len + 12f, 2f, p, c)
                fill(0xFF3A7BD5.toInt()); rr(-3.8f, len - 3.6f, 3.8f, len + 10.4f, 1f, p, c)
            }
        }
        c.restore()
    }

    private fun drawHead(c: Canvas, L: Look, P: Pose) {
        val act = P.act
        val sleeping = act == Act.SLEEP
        val look = P.facing * 1.6f
        // oreilles
        fill(shade(L.skin, 0.92f)); c.drawCircle(30f, 39f, 5f, p); c.drawCircle(70f, 39f, 5f, p)
        // visage : crâne arrondi, mâchoire carrée
        fill(L.skin)
        path.reset()
        path.moveTo(31f, 30f)
        path.cubicTo(31f, 13f, 69f, 13f, 69f, 30f)
        path.lineTo(69f, 46f)
        path.quadTo(69f, 57f, 58f, 58f)
        path.lineTo(42f, 58f)
        path.quadTo(31f, 57f, 31f, 46f)
        path.close()
        c.drawPath(path, p)
        // ombre de mâchoire
        fill(0x14000000); rr(33f, 50f, 67f, 58f, 7f, p, c)

        // ----- cheveux
        drawHair(c, L)

        // ----- yeux
        val blink = !sleeping && ((P.t + P.seed * 0.37f) % 4.2f) < 0.13f
        val shades = L.accessory == Accessory.SHADES || L.accessory == Accessory.AVIATORS
        if (!shades) {
            if (act == Act.KO) {
                line(INK, 1.7f)
                for (ex in floatArrayOf(42.5f, 57.5f)) { c.drawLine(ex - 2.6f, 35.8f, ex + 2.6f, 41f, s); c.drawLine(ex + 2.6f, 35.8f, ex - 2.6f, 41f, s) }
            } else if (act == Act.HIT) {
                line(INK, 1.8f)
                c.drawLine(40f, 36f, 45f, 38.5f, s); c.drawLine(45f, 38.5f, 40f, 41f, s)
                c.drawLine(60f, 36f, 55f, 38.5f, s); c.drawLine(55f, 38.5f, 60f, 41f, s)
            } else if (sleeping || blink) {
                line(INK, 1.6f)
                c.drawArc(RectF(39f, 36f, 46f, 41f), 10f, 160f, false, s); c.drawArc(RectF(54f, 36f, 61f, 41f), 10f, 160f, false, s)
            } else if (act == Act.DRAGGED) {
                fill(0xFFFFFFFF.toInt()); c.drawCircle(42.5f, 38f, 3.6f, p); c.drawCircle(57.5f, 38f, 3.6f, p)
                fill(INK); c.drawCircle(42.5f, 38f, 1.8f, p); c.drawCircle(57.5f, 38f, 1.8f, p)
            } else {
                val dy = if (act == Act.PHONE) 1.2f else 0f
                fill(INK); rr(40.5f + look, 35.5f + dy, 44.5f + look, 41f + dy, 2f, p, c); rr(55.5f + look, 35.5f + dy, 59.5f + look, 41f + dy, 2f, p, c)
                fill(0xCCFFFFFF.toInt()); c.drawCircle(42f + look, 37f + dy, 0.8f, p); c.drawCircle(57f + look, 37f + dy, 0.8f, p)
            }
        }
        // sourcils épais
        val browY = if (act == Act.DRAGGED) -2.5f else 0f
        val angry = act == Act.PUNCH || act == Act.GUARD
        fill(shade(L.hairColor, 0.8f))
        c.save(); c.rotate(if (act == Act.DRAGGED) -8f else if (angry) 18f else 8f, 42f, 32f); rr(36.5f, 30.5f + browY, 47f, 33.5f + browY, 1.5f, p, c); c.restore()
        c.save(); c.rotate(if (act == Act.DRAGGED) 8f else if (angry) -18f else -8f, 58f, 32f); rr(53f, 30.5f + browY, 63.5f, 33.5f + browY, 1.5f, p, c); c.restore()
        // nez
        line(shade(L.skin, 0.72f), 1.5f); c.drawLine(50f, 39f, 48.5f, 45f, s); c.drawLine(48.5f, 45f, 51f, 45.5f, s)
        // cicatrice
        if (L.scar) { line(0xFFB5524A.toInt(), 1.2f); c.drawLine(58f, 30f, 62f, 42f, s); c.drawLine(58.5f, 34f, 61.5f, 33f, s); c.drawLine(59.8f, 38f, 62.6f, 37f, s) }

        // ----- barbe
        drawBeard(c, L)

        // ----- bouche
        val talkOpen = P.talking && ((P.t * 9f).toInt() % 2 == 0)
        when {
            act == Act.YAWN -> { fill(0xFF5A1A1A.toInt()); r.set(45f, 47f, 55f, 56f); c.drawOval(r, p) }
            act == Act.DRAGGED || act == Act.HIT -> { fill(0xFF5A1A1A.toInt()); r.set(47f, 48f, 53f, 54f); c.drawOval(r, p) }
            act == Act.KO -> {
                line(INK, 1.4f); path.reset(); path.moveTo(45f, 51f); path.quadTo(47.5f, 49f, 50f, 51f); path.quadTo(52.5f, 53f, 55f, 51f); c.drawPath(path, s)
                fill(0xFFE57373.toInt()); rr(51f, 51f, 55f, 56f, 2f, p, c)
            }
            act == Act.PUNCH || act == Act.GUARD -> {
                fill(0xFFFFFFFF.toInt()); rr(45f, 48.5f, 55f, 52f, 1.2f, p, c)
                line(INK, 0.8f); c.drawLine(45f, 50.2f, 55f, 50.2f, s); c.drawLine(48.3f, 48.5f, 48.3f, 52f, s); c.drawLine(51.7f, 48.5f, 51.7f, 52f, s)
            }
            act == Act.EAT -> { val o = abs(sin(P.t * 8f)) * 3f; fill(0xFF5A1A1A.toInt()); rr(46f, 49f, 54f, 50.5f + o, 2f, p, c) }
            talkOpen -> { fill(0xFF5A1A1A.toInt()); rr(45f, 48.5f, 55f, 53f, 2.5f, p, c) }
            act == Act.FLEX || act == Act.TRAIN || act == Act.WAVE || act == Act.VICTORY || act == Act.HIGHFIVE || act == Act.RUN -> {
                fill(0xFFFFFFFF.toInt()); path.reset(); path.moveTo(44f, 48.5f); path.lineTo(56f, 48.5f); path.quadTo(55f, 54f, 50f, 54f); path.quadTo(45f, 54f, 44f, 48.5f); c.drawPath(path, p)
                line(INK, 1f); c.drawPath(path, s)
            }
            sleeping -> { line(INK, 1.4f); c.drawLine(47f, 51f, 53f, 51f, s) }
            else -> { line(INK, 1.6f); path.reset(); path.moveTo(45f, 50f); path.quadTo(50f, 51.5f, 55.5f, 48.5f); c.drawPath(path, s) }
        }

        // ----- accessoires
        drawAccessory(c, L, P)
    }

    private fun drawHair(c: Canvas, L: Look) {
        val hc = L.hairColor
        when (L.hair) {
            HairStyle.BALD -> { fill(0x30FFFFFF); r.set(38f, 18f, 50f, 24f); c.drawOval(r, p) }
            HairStyle.BUZZ -> {
                fill((hc and 0x00FFFFFF) or 0x88000000.toInt())
                path.reset(); path.moveTo(31f, 31f); path.cubicTo(31f, 12.5f, 69f, 12.5f, 69f, 31f); path.quadTo(50f, 24f, 31f, 31f); c.drawPath(path, p)
            }
            HairStyle.CREW -> {
                fill(hc); path.reset(); path.moveTo(30.5f, 33f); path.cubicTo(29f, 10f, 71f, 10f, 69.5f, 33f); path.lineTo(67f, 27f); path.quadTo(50f, 22f, 33f, 27f); path.close(); c.drawPath(path, p)
            }
            HairStyle.MOHAWK -> {
                fill((hc and 0x00FFFFFF) or 0x55000000); path.reset(); path.moveTo(31f, 31f); path.cubicTo(31f, 13f, 69f, 13f, 69f, 31f); path.quadTo(50f, 25f, 31f, 31f); c.drawPath(path, p)
                fill(hc); path.reset(); path.moveTo(44f, 22f); path.lineTo(47f, 4f); path.lineTo(51f, 12f); path.lineTo(54f, 3f); path.lineTo(56f, 22f); path.close(); c.drawPath(path, p)
            }
            HairStyle.SLICK -> {
                fill(hc); path.reset(); path.moveTo(30.5f, 34f); path.cubicTo(28f, 9f, 72f, 9f, 69.5f, 34f); path.quadTo(66f, 22f, 44f, 25f); path.quadTo(35f, 26f, 30.5f, 34f); c.drawPath(path, p)
                line(0x40FFFFFF, 1.2f); c.drawLine(40f, 17f, 60f, 15f, s); c.drawLine(38f, 21f, 62f, 19f, s)
            }
            HairStyle.LONG -> {
                fill(hc); path.reset(); path.moveTo(30f, 40f); path.cubicTo(27f, 9f, 73f, 9f, 70f, 40f); path.quadTo(64f, 24f, 50f, 24f); path.quadTo(36f, 24f, 30f, 40f); c.drawPath(path, p)
            }
            HairStyle.BUN -> {
                fill(hc); path.reset(); path.moveTo(30.5f, 33f); path.cubicTo(29f, 11f, 71f, 11f, 69.5f, 33f); path.quadTo(50f, 22f, 30.5f, 33f); c.drawPath(path, p)
                c.drawCircle(50f, 11f, 6.5f, p); fill(0xFF8B1E1E.toInt()); rr(46f, 15f, 54f, 18f, 1f, p, c)
            }
            HairStyle.CURLY -> {
                fill(hc)
                val pts = floatArrayOf(33f, 28f, 38f, 20f, 45f, 16f, 52f, 15f, 59f, 17f, 65f, 21f, 68f, 28f)
                for (i in pts.indices step 2) c.drawCircle(pts[i], pts[i + 1], 6f, p)
            }
        }
    }

    private fun drawBeard(c: Canvas, L: Look) {
        val hc = L.hairColor
        when (L.beard) {
            Beard.NONE -> {}
            Beard.STUBBLE -> {
                fill((hc and 0x00FFFFFF) or 0x45000000)
                path.reset(); path.moveTo(31f, 42f); path.lineTo(31f, 46f); path.quadTo(31f, 57f, 42f, 58f); path.lineTo(58f, 58f); path.quadTo(69f, 57f, 69f, 46f); path.lineTo(69f, 42f); path.quadTo(60f, 47f, 50f, 46f); path.quadTo(40f, 47f, 31f, 42f); c.drawPath(path, p)
            }
            Beard.FULL, Beard.VIKING -> {
                fill(hc)
                path.reset(); path.moveTo(30.5f, 38f); path.lineTo(31f, 48f); path.quadTo(34f, 63f, 50f, 64f); path.quadTo(66f, 63f, 69f, 48f); path.lineTo(69.5f, 38f); path.quadTo(66f, 45f, 58f, 46f); path.quadTo(50f, 44f, 42f, 46f); path.quadTo(34f, 45f, 30.5f, 38f); c.drawPath(path, p)
                if (L.beard == Beard.VIKING) {
                    for (x in floatArrayOf(44f, 56f)) {
                        for (i in 0..2) c.drawCircle(x, 64f + i * 5f, 3.2f, p)
                        fill(0xFFC9A227.toInt()); rr(x - 3f, 78f, x + 3f, 81f, 1f, p, c); fill(hc)
                    }
                }
                fill(shade(hc, 0.85f)); path.reset(); path.moveTo(43f, 47.5f); path.quadTo(50f, 44.5f, 57f, 47.5f); path.quadTo(50f, 47f, 43f, 47.5f); c.drawPath(path, p)
            }
            Beard.GOATEE -> {
                fill(hc); rr(46f, 52f, 54f, 61f, 3f, p, c)
                path.reset(); path.moveTo(43f, 48.5f); path.quadTo(50f, 45f, 57f, 48.5f); path.lineTo(56f, 50f); path.quadTo(50f, 48f, 44f, 50f); path.close(); c.drawPath(path, p)
            }
            Beard.MUSTACHE -> {
                fill(hc); path.reset(); path.moveTo(42f, 49.5f); path.quadTo(50f, 43.5f, 58f, 49.5f); path.quadTo(54f, 48f, 50f, 49f); path.quadTo(46f, 48f, 42f, 49.5f); c.drawPath(path, p)
            }
            Beard.CHEVRON -> {
                fill(hc); path.reset(); path.moveTo(41f, 53f); path.quadTo(41f, 44f, 50f, 45f); path.quadTo(59f, 44f, 59f, 53f); path.quadTo(55f, 49f, 50f, 50f); path.quadTo(45f, 49f, 41f, 53f); c.drawPath(path, p)
            }
        }
    }

    private fun drawAccessory(c: Canvas, L: Look, P: Pose) {
        when (L.accessory) {
            Accessory.NONE -> {}
            Accessory.SHADES -> {
                fill(INK); rr(36f, 34f, 48f, 41.5f, 2.5f, p, c); rr(52f, 34f, 64f, 41.5f, 2.5f, p, c); rr(46f, 35f, 54f, 37f, 1f, p, c)
                line(INK, 1.6f); c.drawLine(36f, 36f, 31f, 36f, s); c.drawLine(64f, 36f, 69f, 36f, s)
                fill(0x55FFFFFF); rr(38f, 35.5f, 42f, 37f, 1f, p, c); rr(54f, 35.5f, 58f, 37f, 1f, p, c)
            }
            Accessory.AVIATORS -> {
                fill(0xEE2C3E50.toInt()); r.set(36f, 33.5f, 48.5f, 43f); c.drawOval(r, p); r.set(51.5f, 33.5f, 64f, 43f); c.drawOval(r, p)
                line(0xFFC9A227.toInt(), 1.2f); r.set(36f, 33.5f, 48.5f, 43f); c.drawOval(r, s); r.set(51.5f, 33.5f, 64f, 43f); c.drawOval(r, s)
                c.drawLine(48.5f, 35.5f, 51.5f, 35.5f, s); c.drawLine(36f, 36f, 31f, 36f, s); c.drawLine(64f, 36f, 69f, 36f, s)
                fill(0x66FFFFFF); r.set(38f, 35f, 42f, 38f); c.drawOval(r, p); r.set(53.5f, 35f, 57.5f, 38f); c.drawOval(r, p)
            }
            Accessory.HEADBAND -> {
                fill(0xFFC0392B.toInt()); rr(30f, 24f, 70f, 29.5f, 2f, p, c)
                path.reset(); path.moveTo(69f, 26f); path.lineTo(77f, 22f + sin(P.t * 6f) * 2f); path.lineTo(76f, 30f + sin(P.t * 6f) * 2f); path.close(); c.drawPath(path, p)
            }
            Accessory.CAP -> {
                fill(0xFF151515.toInt()); path.reset(); path.moveTo(30f, 28f); path.cubicTo(29f, 8f, 71f, 8f, 70f, 28f); path.close(); c.drawPath(path, p)
                rr(42f, 24f, 82f * 0.5f + 40f, 29f, 3f, p, c)
                fill(0xFFFFC928.toInt()); c.drawCircle(50f, 19f, 3f, p)
            }
            Accessory.HORNS -> {
                fill(0xFFF2EAD8.toInt())
                path.reset(); path.moveTo(32f, 22f); path.quadTo(20f, 18f, 22f, 4f); path.quadTo(27f, 14f, 36f, 16f); path.close(); c.drawPath(path, p)
                path.reset(); path.moveTo(68f, 22f); path.quadTo(80f, 18f, 78f, 4f); path.quadTo(73f, 14f, 64f, 16f); path.close(); c.drawPath(path, p)
                fill(0xFF8A8F96.toInt()); path.reset(); path.moveTo(30f, 27f); path.cubicTo(29f, 7f, 71f, 7f, 70f, 27f); path.close(); c.drawPath(path, p)
                fill(0xFF5F646B.toInt()); rr(29f, 24f, 71f, 28.5f, 2f, p, c); rr(48f, 10f, 52f, 26f, 1f, p, c)
                fill(0xFFC9C9C9.toInt()); for (x in floatArrayOf(34f, 42f, 58f, 66f)) c.drawCircle(x, 26.3f, 1f, p)
            }
            Accessory.BEANIE -> {
                fill(0xFF2F4F4F.toInt()); path.reset(); path.moveTo(30f, 28f); path.cubicTo(29f, 5f, 71f, 5f, 70f, 28f); path.close(); c.drawPath(path, p)
                fill(0xFF243D3D.toInt()); rr(29f, 22f, 71f, 30f, 3f, p, c)
                line(0x33FFFFFF, 1f); var x = 33f; while (x < 70f) { c.drawLine(x, 23f, x, 29f, s); x += 4f }
            }
            Accessory.BANDANA -> {
                fill(0xFF8B1E1E.toInt()); path.reset(); path.moveTo(30.5f, 29f); path.cubicTo(29f, 10f, 71f, 10f, 69.5f, 29f); path.quadTo(50f, 25f, 30.5f, 29f); c.drawPath(path, p)
                path.reset(); path.moveTo(69f, 26f); path.lineTo(77f, 30f); path.lineTo(72f, 33f); path.close(); c.drawPath(path, p)
                fill(0xFFF2F2F2.toInt()); for (pt in arrayOf(40f to 20f, 50f to 17f, 60f to 20f, 45f to 25f, 55f to 25f)) c.drawCircle(pt.first, pt.second, 1f, p)
            }
            Accessory.BERET -> {
                fill(0xFF6B1E2E.toInt()); r.set(27f, 13f, 66f, 27f); c.drawOval(r, p)
                fill(0xFF4A1420.toInt()); rr(31f, 23f, 67f, 27f, 2f, p, c)
                fill(0xFFC9A227.toInt()); c.drawCircle(37f, 23f, 2.4f, p)
            }
            Accessory.HEADPHONES -> {
                line(0xFF2A2A2A.toInt(), 3f); path.reset(); path.moveTo(30f, 38f); path.cubicTo(26f, 6f, 74f, 6f, 70f, 38f); c.drawPath(path, s)
                fill(0xFF151515.toInt()); rr(25f, 33f, 33f, 46f, 3f, p, c); rr(67f, 33f, 75f, 46f, 3f, p, c)
                fill(0xFFFFC928.toInt()); rr(26f, 37f, 28f, 42f, 1f, p, c); rr(72f, 37f, 74f, 42f, 1f, p, c)
            }
            Accessory.COWBOY -> {
                fill(0xFF6B4226.toInt()); r.set(16f, 20f, 84f, 30f); c.drawOval(r, p)
                path.reset(); path.moveTo(34f, 25f); path.cubicTo(33f, 4f, 45f, 9f, 50f, 9f); path.cubicTo(55f, 9f, 67f, 4f, 66f, 25f); path.close(); c.drawPath(path, p)
                fill(0xFF3B2418.toInt()); rr(34f, 20f, 66f, 24f, 1f, p, c)
            }
            Accessory.TOQUE -> {
                fill(0xFFF7F7F7.toInt()); c.drawCircle(40f, 12f, 8f, p); c.drawCircle(50f, 9f, 9f, p); c.drawCircle(60f, 12f, 8f, p)
                rr(33f, 12f, 67f, 28f, 3f, p, c)
                line(0xFFDADADA.toInt(), 1f); c.drawLine(33f, 23f, 67f, 23f, s)
            }
        }
    }

    private fun star(c: Canvas, x: Float, y: Float, rad: Float, color: Int, points: Int = 5, inner: Float = 0.45f) {
        path.reset()
        for (i in 0 until points * 2) {
            val a = (i * PI / points - PI / 2).toFloat()
            val rr = if (i % 2 == 0) rad else rad * inner
            val px = x + cos(a) * rr; val py = y + sin(a) * rr
            if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
        }
        path.close()
        fill(color); c.drawPath(path, p)
    }

    /** Explosion BD façon « POW ! » côté adversaire. */
    private fun drawFx(c: Canvas, text: String, P: Pose) {
        val k = (P.fxAge / 0.5f).coerceIn(0f, 1f)
        val scale = 0.6f + 0.5f * kotlin.math.min(1f, P.fxAge / 0.12f)
        val alpha = ((1f - k) * 255).toInt().coerceIn(0, 255)
        if (alpha <= 0) return
        val cx = 50f + P.facing * 22f
        val cy = 24f - k * 6f
        c.save(); c.scale(scale, scale, cx, cy)
        star(c, cx, cy, 21f, (alpha shl 24) or 0xFFC928, points = 9, inner = 0.68f)
        star(c, cx, cy, 15f, (alpha shl 24) or 0xE53935, points = 9, inner = 0.7f)
        txt.color = 0xFFFFFFFF.toInt(); txt.alpha = alpha; txt.textSize = 9.5f
        c.drawText(text, cx, cy + 3.3f, txt)
        c.restore()
    }

    /** Pompes : vue allongée, rendu dédié. */
    private fun drawPushup(c: Canvas, L: Look, P: Pose, broad: Float) {
        val k = (sin(P.t * 4f) + 1f) / 2f // 0 bas, 1 haut
        val bodyY = 112f - k * 10f
        c.save()
        // corps incliné
        c.rotate(-8f + k * -4f, 50f, bodyY)
        fill(L.pantsColor); rr(52f, bodyY - 6f, 92f, bodyY + 5f, 5f, p, c)
        fill(INK); rr(88f, bodyY - 5f, 96f, bodyY + 8f, 3f, p, c)
        val body = when (L.outfit) { Outfit.BARE -> L.skin; Outfit.APRON, Outfit.VEST -> 0xFFEDEDED.toInt(); else -> L.outfitColor }
        fill(body); rr(20f, bodyY - 9f - broad * 0.3f, 56f, bodyY + 6f, 7f, p, c)
        c.restore()
        // bras
        val sleeve = sleeveColor(L)
        fill(sleeve ?: L.skin); rr(24f, bodyY - 2f, 33f, 132f, 4.5f, p, c)
        fill(L.skin); c.drawCircle(28.5f, 131f, 4.5f, p)
        // tête de profil
        fill(L.skin); c.drawCircle(14f, bodyY - 8f, 12f, p)
        val hc = L.hairColor
        if (L.hair != HairStyle.BALD) { fill(hc); r.set(3f, bodyY - 22f, 26f, bodyY - 9f); c.drawArc(r, 180f, 180f, true, p) }
        if (L.beard == Beard.FULL || L.beard == Beard.VIKING) { fill(hc); r.set(2f, bodyY - 6f, 16f, bodyY + 5f); c.drawOval(r, p) }
        fill(INK); c.drawCircle(8f, bodyY - 10f, 1.6f, p)
        // sueur
        if (k < 0.3f) { fill(0xCC7FC8FF.toInt()); c.drawCircle(24f, bodyY - 20f, 1.8f, p) }
        txt.color = 0xFFFFC928.toInt(); txt.textSize = 10f; txt.alpha = 255
        c.drawText(((P.t * 4f / (2f * PI.toFloat())).toInt() % 30 + 1).toString(), 60f, 70f, txt)
    }
}
