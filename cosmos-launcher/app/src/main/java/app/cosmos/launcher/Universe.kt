package app.cosmos.launcher

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.PorterDuff
import android.graphics.PorterDuffXfermode
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Path
import android.graphics.Shader
import java.util.Random
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.floor
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

fun lerpColor(a: Int, b: Int, t: Float): Int {
    val k = t.coerceIn(0f, 1f)
    return Color.argb(
        (Color.alpha(a) + (Color.alpha(b) - Color.alpha(a)) * k).toInt(),
        (Color.red(a) + (Color.red(b) - Color.red(a)) * k).toInt(),
        (Color.green(a) + (Color.green(b) - Color.green(a)) * k).toInt(),
        (Color.blue(a) + (Color.blue(b) - Color.blue(a)) * k).toInt(),
    )
}

/** Univers animé plein écran : étoiles en parallaxe, nébuleuses, planète à anneaux, météores, vitesse-lumière. */
class Universe {
    var time = 0f
    var warp = 0f
    var warpTarget = 0f
    var camX = 0f
    var camY = 0f
    var camXTarget = 0f
    var camYTarget = 0f
    var tint = 0xFF7C4DFF.toInt()
    var tintTarget = 0xFF7C4DFF.toInt()

    private val rnd = Random(2024)
    private val n = 280
    private val sx = FloatArray(n) { rnd.nextFloat() }
    private val sy = FloatArray(n) { rnd.nextFloat() }
    private val sd = FloatArray(n) { 0.1f + rnd.nextFloat() * 0.9f }
    private val sp = FloatArray(n) { rnd.nextFloat() * 6.2831f }
    private val sv = FloatArray(n) { 0.4f + rnd.nextFloat() * 1.4f }
    private val ss = FloatArray(n) { if (rnd.nextFloat() < 0.07f) 2.4f + rnd.nextFloat() * 1.6f else 0.8f + rnd.nextFloat() * 1.3f }
    private val sc = IntArray(n) {
        when (rnd.nextInt(6)) {
            0 -> 0xFFBFD4FF.toInt()
            1 -> 0xFFFFE9C2.toInt()
            2 -> 0xFFFFC9E6.toInt()
            else -> 0xFFFFFFFF.toInt()
        }
    }

    private class Meteor(var x: Float, var y: Float, val dx: Float, val dy: Float, val speed: Float, val len: Float, val dur: Float) {
        var age = 0f
    }

    private val meteors = ArrayList<Meteor>()
    private var meteorTimer = 2.5f

    private val bokeh = 26
    private val bx = FloatArray(bokeh) { rnd.nextFloat() }
    private val by = FloatArray(bokeh) { rnd.nextFloat() }
    private val br = FloatArray(bokeh) { 16f + rnd.nextFloat() * 70f }
    private val bs = FloatArray(bokeh) { 0.004f + rnd.nextFloat() * 0.012f }
    private val bp = FloatArray(bokeh) { rnd.nextFloat() * 6.28f }

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND }
    private val glow = Paint(Paint.ANTI_ALIAS_FLAG)
    private val bmpPaint = Paint(Paint.FILTER_BITMAP_FLAG)
    private val addPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { xfermode = PorterDuffXfermode(PorterDuff.Mode.ADD) }
    private val ringPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }

    private var w = 0f
    private var h = 0f
    private var skyShader: LinearGradient? = null
    private var vignetteShader: RadialGradient? = null
    private var nebulaA: Bitmap? = null
    private var nebulaB: Bitmap? = null
    private var tintShader: RadialGradient? = null
    private var tintShaderColor = 0
    private var planetShader: RadialGradient? = null
    private var haloShader: RadialGradient? = null
    private var iceShader: RadialGradient? = null
    private var iceHalo: RadialGradient? = null
    private var moonShader: RadialGradient? = null
    private val planetPath = Path()
    private val ringOval = RectF()
    private val tmpRect = RectF()

    fun update(dt: Float) {
        time += dt
        warp += (warpTarget - warp) * (1f - exp(-2.6f * dt))
        camX += (camXTarget - camX) * (1f - exp(-2.2f * dt))
        camY += (camYTarget - camY) * (1f - exp(-2.2f * dt))
        tint = lerpColor(tint, tintTarget, 1f - exp(-3f * dt))

        meteorTimer -= dt
        if (meteorTimer <= 0f && w > 0f) {
            meteorTimer = 3f + rnd.nextFloat() * 5f
            val ang = (200f + rnd.nextFloat() * 25f) * PI.toFloat() / 180f
            meteors += Meteor(
                w * (0.35f + rnd.nextFloat() * 0.7f), -h * 0.05f + rnd.nextFloat() * h * 0.4f,
                cos(ang), -sin(ang), w * (0.9f + rnd.nextFloat() * 0.5f), w * (0.12f + rnd.nextFloat() * 0.1f), 0.9f,
            )
        }
        val it = meteors.iterator()
        while (it.hasNext()) {
            val m = it.next()
            m.age += dt
            m.x += m.dx * m.speed * dt
            m.y += m.dy * m.speed * dt
            if (m.age > m.dur) it.remove()
        }
    }

    fun layout(width: Float, height: Float) {
        if (width == w && height == h) return
        w = width; h = height
        skyShader = LinearGradient(
            0f, 0f, 0f, h, intArrayOf(0xFF04050E.toInt(), 0xFF0A0A1F.toInt(), 0xFF150A2B.toInt()), floatArrayOf(0f, 0.55f, 1f), Shader.TileMode.CLAMP,
        )
        vignetteShader = RadialGradient(
            w / 2f, h / 2f, hypot(w, h) * 0.62f,
            intArrayOf(0x00000000, 0x00000000, 0xA0000000.toInt()), floatArrayOf(0f, 0.55f, 1f), Shader.TileMode.CLAMP,
        )
        if (nebulaA == null) {
            nebulaA = makeNebula(1, intArrayOf(0xFF2B1F8F.toInt(), 0xFF1E6BFF.toInt(), 0xFF8E44FF.toInt()), 0.62f)
            nebulaB = makeNebula(7, intArrayOf(0xFFE0359B.toInt(), 0xFFFF7A3D.toInt(), 0xFF1DE9B6.toInt()), 0.42f)
        }
        val r = h * 0.22f
        planetShader = RadialGradient(
            -r * 0.35f, -r * 0.4f, r * 1.7f,
            intArrayOf(0xFFE9DDFF.toInt(), 0xFF9D7BFF.toInt(), 0xFF4B2FA8.toInt(), 0xFF140C3A.toInt()), floatArrayOf(0f, 0.32f, 0.68f, 1f), Shader.TileMode.CLAMP,
        )
        haloShader = RadialGradient(
            0f, 0f, r * 1.7f,
            intArrayOf(0x00000000, 0x00000000, 0x5578A8FF.toInt(), 0x00000000), floatArrayOf(0f, 0.58f, 0.64f, 1f), Shader.TileMode.CLAMP,
        )
        val ri = h * 0.045f
        iceShader = RadialGradient(
            -ri * 0.3f, -ri * 0.3f, ri * 1.6f,
            intArrayOf(0xFFFFFFFF.toInt(), 0xFF8EE6FF.toInt(), 0xFF1D5C8A.toInt()), floatArrayOf(0f, 0.45f, 1f), Shader.TileMode.CLAMP,
        )
        iceHalo = RadialGradient(0f, 0f, ri * 4f, intArrayOf(0x4478E0FF.toInt(), 0x00000000), floatArrayOf(0.2f, 1f), Shader.TileMode.CLAMP)
        val rm = r * 0.11f
        moonShader = RadialGradient(
            -rm * 0.3f, -rm * 0.3f, rm * 1.7f,
            intArrayOf(0xFFFFFFFF.toInt(), 0xFFB9B9C8.toInt(), 0xFF4A4A5E.toInt()), floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP,
        )
        tintShaderColor = 0
    }

    // ---- bruit pour les nébuleuses ----
    private fun hash(ix: Int, iy: Int, seed: Int): Float {
        var x = ix * 374761393 + iy * 668265263 + seed * 1442695041.toInt()
        x = (x xor (x ushr 13)) * 1274126177
        x = x xor (x ushr 16)
        return (x and 0xFFFFFF) / 16777215f
    }

    private fun vnoise(x: Float, y: Float, seed: Int): Float {
        val xi = floor(x).toInt(); val yi = floor(y).toInt()
        val xf = x - xi; val yf = y - yi
        val u = xf * xf * (3f - 2f * xf); val v = yf * yf * (3f - 2f * yf)
        val a = hash(xi, yi, seed); val b = hash(xi + 1, yi, seed)
        val c = hash(xi, yi + 1, seed); val d = hash(xi + 1, yi + 1, seed)
        return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v
    }

    private fun fbm(x: Float, y: Float, seed: Int): Float {
        var s = 0f; var amp = 0.5f; var f = 1f
        for (i in 0 until 5) { s += amp * vnoise(x * f, y * f, seed + i); f *= 2.03f; amp *= 0.5f }
        return s
    }

    private fun smooth(e0: Float, e1: Float, x: Float): Float {
        val t = ((x - e0) / (e1 - e0)).coerceIn(0f, 1f)
        return t * t * (3f - 2f * t)
    }

    private fun makeNebula(seed: Int, cols: IntArray, strength: Float): Bitmap {
        val bw = 288; val bh = 162
        val px = IntArray(bw * bh)
        for (y in 0 until bh) for (x in 0 until bw) {
            val u = x / bw.toFloat(); val v = y / bh.toFloat()
            val n1 = fbm(u * 3.4f, v * 2.1f, seed)
            val n2 = fbm(u * 2.2f + 9f, v * 1.6f + 3f, seed + 11)
            val density = smooth(0.40f, 0.86f, n1)
            val mask = 0.35f + 0.65f * smooth(0.15f, 0.8f, fbm(u * 1.3f + 5f, v * 1.1f, seed + 23))
            val a = (density * density * mask * strength).coerceIn(0f, 1f)
            val c = if (n2 < 0.5f) lerpColor(cols[0], cols[1], n2 * 2f) else lerpColor(cols[1], cols[2], (n2 - 0.5f) * 2f)
            px[y * bw + x] = Color.argb((a * 255).toInt(), Color.red(c), Color.green(c), Color.blue(c))
        }
        return Bitmap.createBitmap(px, bw, bh, Bitmap.Config.ARGB_8888)
    }

    fun draw(c: Canvas, width: Float, height: Float) {
        layout(width, height)
        // ciel
        fill.shader = skyShader; fill.alpha = 255
        c.drawRect(0f, 0f, w, h, fill)
        fill.shader = null

        // nébuleuses (parallaxe lente)
        val na = nebulaA; val nb = nebulaB
        if (na != null) {
            val dw = w * 1.4f; val dh = h * 1.4f
            val ox = -(dw - w) / 2f + sin(time * 0.045f) * w * 0.05f - camX * 26f
            val oy = -(dh - h) / 2f + cos(time * 0.037f) * h * 0.04f - camY * 16f
            tmpRect.set(ox, oy, ox + dw, oy + dh)
            bmpPaint.alpha = 255
            c.drawBitmap(na, null, tmpRect, bmpPaint)
        }
        if (nb != null) {
            c.save()
            c.rotate(sin(time * 0.02f) * 3f, w / 2f, h / 2f)
            val dw = w * 1.5f; val dh = h * 1.5f
            val ox = -(dw - w) / 2f - sin(time * 0.05f) * w * 0.06f - camX * 44f
            val oy = -(dh - h) / 2f + sin(time * 0.04f) * h * 0.05f - camY * 24f
            tmpRect.set(ox, oy, ox + dw, oy + dh)
            bmpPaint.alpha = 190
            c.drawBitmap(nb, null, tmpRect, bmpPaint)
            c.restore()
        }

        // lumière teintée selon l'élément sélectionné
        if (tintShader == null || tintShaderColor != tint) {
            tintShader = RadialGradient(
                0f, 0f, w * 0.75f,
                intArrayOf(Color.argb(120, Color.red(tint), Color.green(tint), Color.blue(tint)), 0x00000000), floatArrayOf(0f, 1f), Shader.TileMode.CLAMP,
            )
            tintShaderColor = tint
        }
        c.save()
        c.translate(w * 0.32f - camX * 60f, h * 0.62f - camY * 30f)
        addPaint.shader = tintShader
        c.drawRect(-w, -h, w, h, addPaint)
        addPaint.shader = null
        c.restore()

        drawStars(c)
        drawIcePlanet(c)
        drawPlanet(c)
        drawBokeh(c)
        drawMeteors(c)

        fill.shader = vignetteShader; fill.alpha = 255
        c.drawRect(0f, 0f, w, h, fill)
        fill.shader = null
    }

    private fun drawStars(c: Canvas) {
        val cx = w / 2f; val cy = h / 2f
        val warping = warp > 0.02f
        for (i in 0 until n) {
            val d = sd[i]
            var x = (sx[i] * w - time * sv[i] * (3f + d * 12f) - camX * d * 70f) % w
            if (x < 0f) x += w
            var y = (sy[i] * h + sin(time * 0.12f + sp[i]) * 7f * d - camY * d * 36f) % h
            if (y < 0f) y += h
            val tw = 0.6f + 0.4f * sin(time * sv[i] * 2.2f + sp[i])
            val a = (tw * (0.35f + 0.65f * d)).coerceIn(0f, 1f)
            val size = ss[i] * (0.55f + d * 0.75f)
            if (warping) {
                val vx = x - cx; val vy = y - cy
                val k = 1f + warp * (0.2f + d * 2.2f)
                line.color = sc[i]; line.alpha = (a * 255).toInt(); line.strokeWidth = size
                c.drawLine(x, y, cx + vx * k, cy + vy * k, line)
            } else {
                fill.color = sc[i]; fill.alpha = (a * 255).toInt()
                c.drawCircle(x, y, size, fill)
                if (ss[i] > 2.3f) {
                    fill.alpha = (a * 38).toInt()
                    c.drawCircle(x, y, size * 4.2f, fill)
                    line.color = sc[i]; line.alpha = (a * 130).toInt(); line.strokeWidth = 1.2f
                    val l = size * (4f + 2f * tw)
                    c.drawLine(x - l, y, x + l, y, line)
                    c.drawLine(x, y - l, x, y + l, line)
                }
            }
        }
    }

    private fun drawIcePlanet(c: Canvas) {
        val r = h * 0.045f
        val cx = w * 0.56f - camX * 18f
        val cy = h * 0.13f + sin(time * 0.15f) * 5f - camY * 10f
        c.save(); c.translate(cx, cy)
        glow.shader = iceHalo; glow.alpha = 255
        c.drawCircle(0f, 0f, r * 4f, glow)
        glow.shader = iceShader
        c.drawCircle(0f, 0f, r, glow)
        glow.shader = null
        c.restore()
    }

    private fun drawPlanet(c: Canvas) {
        val r = h * 0.22f
        val cx = w * 0.89f - camX * 24f
        val cy = h * 0.88f + sin(time * 0.1f) * 6f - camY * 12f
        val tilt = -17f
        c.save()
        c.translate(cx, cy)
        // halo atmosphérique
        glow.shader = haloShader; glow.alpha = 255
        c.drawCircle(0f, 0f, r * 1.7f, glow)
        glow.shader = null

        val moonAng = time * 0.22f
        val ringA = r * 1.95f; val ringB = r * 0.42f
        val mx = cos(moonAng) * ringA * 1.12f
        val my = sin(moonAng) * ringB * 1.12f
        val moonBehind = sin(moonAng) < 0f

        // anneaux arrière
        c.save(); c.rotate(tilt)
        drawRings(c, ringA, ringB, back = true)
        if (moonBehind) drawMoon(c, mx, my)
        c.restore()

        // corps de la planète
        planetPath.reset(); planetPath.addCircle(0f, 0f, r, Path.Direction.CW)
        glow.shader = planetShader
        c.drawCircle(0f, 0f, r, glow)
        glow.shader = null
        c.save(); c.clipPath(planetPath)
        for (k in 0 until 6) {
            val by = -r + (k + 0.5f) * (2f * r / 6f) + sin(time * 0.3f + k) * 3f
            fill.color = if (k % 2 == 0) 0x18FFFFFF else 0x22000000
            c.drawRect(-r, by - r * 0.06f, r, by + r * 0.06f, fill)
        }
        // ombre de la nuit sur le côté bas-droit
        fill.color = 0x40000020
        c.drawCircle(r * 0.55f, r * 0.55f, r * 1.15f, fill)
        c.restore()

        // anneaux avant
        c.save(); c.rotate(tilt)
        drawRings(c, ringA, ringB, back = false)
        if (!moonBehind) drawMoon(c, mx, my)
        c.restore()
        c.restore()
    }

    private fun drawRings(c: Canvas, a: Float, b: Float, back: Boolean) {
        val bands = intArrayOf(0x66C8B4FF, 0x88FFD9A8.toInt(), 0x55B0A0FF, 0x77FFE9C8)
        val ws = floatArrayOf(7f, 12f, 5f, 9f)
        for (k in bands.indices) {
            val m = 0.72f + k * 0.1f
            ringOval.set(-a * m, -b * m, a * m, b * m)
            ringPaint.color = bands[k]; ringPaint.strokeWidth = ws[k]
            c.drawArc(ringOval, if (back) 180f else 0f, 180f, false, ringPaint)
        }
    }

    private fun drawMoon(c: Canvas, x: Float, y: Float) {
        c.save(); c.translate(x, y)
        glow.shader = moonShader; glow.alpha = 255
        c.drawCircle(0f, 0f, h * 0.22f * 0.11f, glow)
        glow.shader = null
        c.restore()
    }

    private fun drawBokeh(c: Canvas) {
        for (i in 0 until bokeh) {
            val y = ((by[i] - time * bs[i]) % 1f).let { if (it < 0f) it + 1f else it }
            val x = bx[i] + sin(time * 0.1f + bp[i]) * 0.02f
            val a = 0.035f + 0.03f * sin(time * 0.4f + bp[i])
            fill.color = lerpColor(0xFFFFFFFF.toInt(), tint, 0.55f)
            fill.alpha = (a * 255).toInt().coerceIn(0, 255)
            c.drawCircle(x * w - camX * 30f, y * h, br[i], fill)
        }
    }

    private fun drawMeteors(c: Canvas) {
        for (m in meteors) {
            val env = min(1f, m.age / 0.12f) * (1f - smooth(0.55f, 1f, m.age / m.dur))
            val seg = 14
            var px = m.x; var py = m.y
            for (k in 1..seg) {
                val t = k / seg.toFloat()
                val nx = m.x - m.dx * m.len * t
                val ny = m.y - m.dy * m.len * t
                val f = (1f - t)
                line.color = lerpColor(0xFFFFFFFF.toInt(), 0xFF8EB8FF.toInt(), t)
                line.alpha = (f * f * env * 255).toInt().coerceIn(0, 255)
                line.strokeWidth = 0.8f + 3.2f * f
                c.drawLine(px, py, nx, ny, line)
                px = nx; py = ny
            }
            fill.color = 0xFFFFFFFF.toInt(); fill.alpha = (env * 255).toInt().coerceIn(0, 255)
            c.drawCircle(m.x, m.y, 3.4f, fill)
            fill.alpha = (env * 60).toInt().coerceIn(0, 255)
            c.drawCircle(m.x, m.y, 12f, fill)
        }
    }
}
