package app.cosmos.launcher

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Shader
import android.graphics.Typeface
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import java.util.Calendar
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin

/** Tout le menu est dessiné ici, dans un repère 1920x1080 mis à l'échelle de l'écran. */
class LauncherView(ctx: Context) : View(ctx) {

    companion object {
        const val DW = 1920f
        const val DH = 1080f
        const val BW = 300f
        const val BH = 170f
        const val GAP = 36f
        const val MARGIN = 110f
        const val ROW_PITCH = 296f
        const val ROW_Y = 600f
        private val DAYS = arrayOf("dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi")
        private val MONTHS = arrayOf("janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre")
    }

    val universe = Universe()
    var onLaunch: ((Tile) -> Unit)? = null

    private var rows: List<Row> = emptyList()
    private var apps: List<Tile> = emptyList()

    // ---- état de navigation ----
    private var focusRow = 0
    private var focusCol = IntArray(0)
    private var rowScroll = 0f
    private var focusPos = FloatArray(0)
    private var colScroll = FloatArray(0)
    private var rowActive = FloatArray(0)
    private var allOpen = false
    private var allT = 0f
    private var gIdx = 0
    private var gfx = 0f
    private var gfy = 0f
    private var gScroll = 0f
    private val gridCols = 6

    // ---- temps / animations ----
    private var time = 0f
    private var intro = 0f
    private var titleT = 1f
    private var idle = 0f
    private var idleMix = 0f
    private var launching: Tile? = null
    private var launchT = 0f
    private var launched = false
    private var launchX = 0f
    private var launchY = 0f
    private var bump = 0f
    private var lastNs = 0L
    private var clockAcc = 10f
    private var timeStr = "00:00"
    private var dateStr = ""
    private var greet = ""
    private var secFrac = 0f
    private val cal = Calendar.getInstance()

    // ---- particules ----
    private val pn = 320
    private val ppx = FloatArray(pn); private val ppy = FloatArray(pn)
    private val pvx = FloatArray(pn); private val pvy = FloatArray(pn)
    private val plife = FloatArray(pn); private val pmax = FloatArray(pn)
    private val psize = FloatArray(pn); private val pcol = IntArray(pn)
    private var pHead = 0
    private val rings = FloatArray(4 * 4) // x, y, t, color
    private var ringHead = 0

    // ---- pinceaux ----
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
    private val textP = Paint(Paint.ANTI_ALIAS_FLAG)
    private val face = Typeface.create("sans-serif-medium", Typeface.NORMAL)
    private val faceLight = Typeface.create("sans-serif-light", Typeface.NORMAL)
    private val faceThin = Typeface.create("sans-serif-thin", Typeface.NORMAL)
    private val faceBold = Typeface.create("sans-serif", Typeface.BOLD)
    private val cardPath = Path().apply { addRoundRect(0f, 0f, BW, BH, 26f, 26f, Path.Direction.CW) }
    private val glossShader = LinearGradient(0f, 0f, BW * 0.7f, BH, 0x40FFFFFF, 0x00FFFFFF, Shader.TileMode.CLAMP)
    private val sweepShader = LinearGradient(0f, 0f, 170f, 0f, intArrayOf(0x00FFFFFF, 0x55FFFFFF, 0x00FFFFFF), floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)
    private val shadeShader = LinearGradient(0f, BH * 0.45f, 0f, BH, 0x00000000, 0x99000000.toInt(), Shader.TileMode.CLAMP)
    private val mtx = Matrix()
    private val src = FloatArray(8)
    private val dst = FloatArray(8)

    // zones tactiles
    private class Hit(val tile: Tile, val row: Int, val col: Int)
    private val hitRects = ArrayList<FloatArray>()
    private val hitTiles = ArrayList<Hit>()

    init {
        isFocusable = true
        isFocusableInTouchMode = true
        setBackgroundColor(Color.BLACK)
        universe.warp = 1f // arrivée en vitesse-lumière
    }

    // ------------------------------------------------------------------ données

    fun setContent(r: List<Row>, all: List<Tile>) {
        val sameShape = r.size == rows.size
        rows = r
        apps = all
        if (!sameShape || focusCol.size != r.size) {
            focusCol = IntArray(r.size)
            focusPos = FloatArray(r.size)
            colScroll = FloatArray(r.size)
            rowActive = FloatArray(r.size).also { if (it.isNotEmpty()) it[0] = 1f }
            focusRow = 0
        } else {
            for (i in r.indices) focusCol[i] = focusCol[i].coerceIn(0, r[i].tiles.size - 1)
        }
        gIdx = gIdx.coerceIn(0, max(0, apps.size - 1))
        titleT = 0f
        updateTint()
        postInvalidateOnAnimation()
    }

    fun resetAfterLaunch() {
        launching = null; launchT = 0f; launched = false
        universe.warpTarget = 0f
        idle = 0f
        postInvalidateOnAnimation()
    }

    fun goHome() {
        allOpen = false
        focusRow = 0
        titleT = 0f
        wake()
        updateTint()
    }

    private fun currentTile(): Tile? =
        if (allOpen) apps.getOrNull(gIdx) else rows.getOrNull(focusRow)?.tiles?.getOrNull(focusCol[focusRow])

    private fun updateTint() {
        currentTile()?.let { universe.tintTarget = it.color1 or (0xFF shl 24) }
        val rc = focusCol.getOrNull(focusRow) ?: 0
        universe.camXTarget = ((if (allOpen) (gIdx % gridCols) else rc) - 2f) * 0.32f
        universe.camYTarget = if (allOpen) 1.2f else (focusRow - 1f) * 0.6f
    }

    // ------------------------------------------------------------------ entrées

    private fun wake() { idle = 0f }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val wasIdle = idleMix > 0.3f
        wake()
        if (launching != null) return true
        if (wasIdle && (keyCode in KeyEvent.KEYCODE_DPAD_UP..KeyEvent.KEYCODE_DPAD_CENTER)) return true
        return when (keyCode) {
            KeyEvent.KEYCODE_DPAD_LEFT -> { move(-1, 0); true }
            KeyEvent.KEYCODE_DPAD_RIGHT -> { move(1, 0); true }
            KeyEvent.KEYCODE_DPAD_UP -> { move(0, -1); true }
            KeyEvent.KEYCODE_DPAD_DOWN -> { move(0, 1); true }
            KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER, KeyEvent.KEYCODE_BUTTON_A -> {
                if (event.repeatCount == 0) activate(); true
            }
            KeyEvent.KEYCODE_MENU, KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_GUIDE -> { toggleAll(); true }
            KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_ESCAPE -> {
                if (allOpen) { toggleAll() } else bump = -1f
                true
            }
            else -> super.onKeyDown(keyCode, event)
        }
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        wake()
        if (e.actionMasked != MotionEvent.ACTION_UP) return true
        for (i in hitRects.indices.reversed()) {
            val r = hitRects[i]
            if (e.x in r[0]..r[2] && e.y in r[1]..r[3]) {
                val h = hitTiles[i]
                if (allOpen) gIdx = apps.indexOf(h.tile).coerceAtLeast(0)
                else { focusRow = h.row; focusCol[h.row] = h.col }
                updateTint(); titleT = 0f
                activate()
                return true
            }
        }
        return true
    }

    private fun move(dx: Int, dy: Int) {
        if (allOpen) {
            val n = apps.size
            if (n == 0) return
            var ni = gIdx
            if (dx != 0) ni = (gIdx + dx).coerceIn(0, n - 1)
            if (dy != 0) ni = (gIdx + dy * gridCols).let { if (it > n - 1 && dy > 0 && gIdx / gridCols < (n - 1) / gridCols) n - 1 else it }.coerceIn(0, n - 1)
            if (ni == gIdx) { bump = if (dx != 0) dx.toFloat() else 0f; return }
            gIdx = ni
        } else {
            if (rows.isEmpty()) return
            if (dy != 0) {
                val nr = focusRow + dy
                if (nr !in rows.indices) { bump = 0f; return }
                focusRow = nr
            } else {
                val nc = focusCol[focusRow] + dx
                if (nc !in rows[focusRow].tiles.indices) { bump = dx.toFloat(); return }
                focusCol[focusRow] = nc
            }
        }
        titleT = 0f
        updateTint()
        sparkle()
    }

    private fun toggleAll() {
        allOpen = !allOpen
        if (allOpen && apps.isNotEmpty()) { gIdx = gIdx.coerceIn(0, apps.size - 1) }
        titleT = 0f
        updateTint()
        addRing(DW / 2f, DH / 2f, 0xFFFFFFFF.toInt())
    }

    private fun activate() {
        val t = currentTile() ?: return
        if (t.system == SystemAction.ALL_APPS) { toggleAll(); return }
        val (x, y) = focusCenter()
        launchX = x; launchY = y
        launching = t; launchT = 0f; launched = false
        burst(x, y, 90, t.color1)
        addRing(x, y, t.color1)
    }

    private fun focusCenter(): Pair<Float, Float> {
        if (allOpen) {
            val r = gIdx / gridCols; val c = gIdx % gridCols
            return gridX(c) + gridW() / 2f to gridY(r) + gridH() / 2f
        }
        val cx = tileX(focusRow, focusCol[focusRow].toFloat()) + BW / 2f
        return cx to rowY(focusRow) + BH / 2f
    }

    // ------------------------------------------------------------------ particules

    private fun burst(x: Float, y: Float, count: Int, color: Int) {
        for (i in 0 until count) {
            val a = (Math.random() * 6.2831).toFloat()
            val sp = 120f + (Math.random() * 520f).toFloat()
            spawn(x, y, cos(a) * sp, sin(a) * sp, 0.7f + (Math.random() * 0.7f).toFloat(), 2f + (Math.random() * 4f).toFloat(), if (i % 3 == 0) Color.WHITE else color)
        }
    }

    private fun sparkle() {
        val (x, y) = focusCenter()
        val color = currentTile()?.color1 ?: Color.WHITE
        for (i in 0 until 14) {
            val a = (Math.random() * 6.2831).toFloat()
            val sp = 40f + (Math.random() * 180f).toFloat()
            spawn(
                x + (Math.random().toFloat() - 0.5f) * BW, y + (Math.random().toFloat() - 0.5f) * BH,
                cos(a) * sp, sin(a) * sp - 40f, 0.5f + (Math.random() * 0.6f).toFloat(), 1.5f + (Math.random() * 3f).toFloat(), if (i % 2 == 0) Color.WHITE else color,
            )
        }
    }

    private fun spawn(x: Float, y: Float, vx: Float, vy: Float, life: Float, size: Float, color: Int) {
        val i = pHead; pHead = (pHead + 1) % pn
        ppx[i] = x; ppy[i] = y; pvx[i] = vx; pvy[i] = vy
        plife[i] = life; pmax[i] = life; psize[i] = size; pcol[i] = color
    }

    private fun addRing(x: Float, y: Float, color: Int) {
        val b = ringHead * 4; ringHead = (ringHead + 1) % 4
        rings[b] = x; rings[b + 1] = y; rings[b + 2] = 0.0001f; rings[b + 3] = Float.fromBits(color)
    }

    // ------------------------------------------------------------------ mise à jour

    private fun ease(a: Float, b: Float, k: Float, dt: Float) = a + (b - a) * (1f - exp(-k * dt))

    fun update(dt: Float) {
        time += dt
        intro += dt
        titleT = min(1f, titleT + dt / 0.5f)
        idle += dt
        idleMix = ease(idleMix, if (idle > 45f && launching == null && !allOpen) 1f else 0f, 2.2f, dt)
        bump = ease(bump, 0f, 9f, dt)
        rowScroll = ease(rowScroll, focusRow.toFloat(), 8f, dt)
        for (i in rows.indices) {
            focusPos[i] = ease(focusPos[i], focusCol[i].toFloat(), 11f, dt)
            colScroll[i] = ease(colScroll[i], max(0, focusCol[i] - 3).toFloat(), 7f, dt)
            rowActive[i] = ease(rowActive[i], if (i == focusRow) 1f else 0f, 9f, dt)
        }
        allT = ease(allT, if (allOpen) 1f else 0f, 8f, dt)
        val gr = (gIdx / gridCols).toFloat(); val gc = (gIdx % gridCols).toFloat()
        gfx = ease(gfx, gc, 14f, dt); gfy = ease(gfy, gr, 14f, dt)
        gScroll = ease(gScroll, max(0f, gr - 1f), 8f, dt)

        if (intro > 0.15f && launching == null) universe.warpTarget = if (idleMix > 0.5f) 0.22f else 0f
        if (launching != null) {
            launchT += dt / 0.75f
            universe.warpTarget = 1f
            if (launchT >= 1f && !launched) { launched = true; launching?.let { onLaunch?.invoke(it) } }
        }
        universe.update(dt)

        for (i in 0 until pn) {
            if (plife[i] <= 0f) continue
            plife[i] -= dt
            ppx[i] += pvx[i] * dt; ppy[i] += pvy[i] * dt
            pvx[i] *= (1f - 1.6f * dt); pvy[i] = pvy[i] * (1f - 1.6f * dt) + 70f * dt
        }
        for (k in 0 until 4) if (rings[k * 4 + 2] > 0f) {
            rings[k * 4 + 2] += dt / 0.9f
            if (rings[k * 4 + 2] >= 1f) rings[k * 4 + 2] = 0f
        }

        clockAcc += dt
        if (clockAcc > 0.25f) { clockAcc = 0f; refreshClock() }
    }

    private fun refreshClock() {
        cal.timeInMillis = System.currentTimeMillis()
        val h = cal.get(Calendar.HOUR_OF_DAY); val m = cal.get(Calendar.MINUTE)
        timeStr = "%02d:%02d".format(h, m)
        dateStr = "${DAYS[cal.get(Calendar.DAY_OF_WEEK) - 1]} ${cal.get(Calendar.DAY_OF_MONTH)} ${MONTHS[cal.get(Calendar.MONTH)]}"
        greet = when (h) { in 5..11 -> "Bonjour"; in 12..17 -> "Bon après-midi"; in 18..22 -> "Bonsoir"; else -> "Bonne nuit" }
        secFrac = (cal.get(Calendar.SECOND) + cal.get(Calendar.MILLISECOND) / 1000f) / 60f
    }

    // ------------------------------------------------------------------ géométrie

    private fun rowY(i: Int) = ROW_Y + (i - rowScroll) * ROW_PITCH
    private fun tileX(row: Int, j: Float) = MARGIN + (j - colScroll[row]) * (BW + GAP)
    private fun gridW() = 256f
    private fun gridH() = 144f
    private fun gridX(c: Int) = (DW - (gridCols * 256f + (gridCols - 1) * 38f)) / 2f + c * (256f + 38f)
    private fun gridY(r: Int) = 340f + (r - gScroll) * 216f

    // ------------------------------------------------------------------ dessin

    override fun onSizeChanged(w: Int, h: Int, ow: Int, oh: Int) {
        universe.layout(w.toFloat(), h.toFloat())
    }

    override fun onDraw(canvas: Canvas) {
        val now = System.nanoTime()
        val dt = if (lastNs == 0L) 0.016f else ((now - lastNs) / 1e9f).coerceIn(0f, 0.05f)
        lastNs = now
        update(dt)
        render(canvas)
        postInvalidateOnAnimation()
    }

    /** Dessine l'image courante sans avancer le temps (utilisé aussi par les tests de rendu). */
    fun render(canvas: Canvas) {
        val w = width.toFloat(); val h = height.toFloat()
        universe.draw(canvas, w, h)
        val s = min(w / DW, h / DH)
        canvas.save()
        canvas.translate((w - DW * s) / 2f, (h - DH * s) / 2f)
        canvas.scale(s, s)
        hitRects.clear(); hitTiles.clear()
        drawHeader(canvas)
        val homeA = (1f - allT) * (1f - idleMix) * (1f - smooth(launchT * 1.3f - 0.2f))
        if (homeA > 0.01f) drawHome(canvas, homeA)
        if (allT > 0.01f) drawAll(canvas, allT * (1f - smooth(launchT * 1.3f - 0.2f)))
        drawEffects(canvas)
        drawLaunchTile(canvas)
        canvas.restore()
        // mémorise la transformation pour les zones tactiles
        val ox = (w - DW * s) / 2f; val oy = (h - DH * s) / 2f
        for (r in hitRects) { r[0] = r[0] * s + ox; r[1] = r[1] * s + oy; r[2] = r[2] * s + ox; r[3] = r[3] * s + oy }
    }

    private fun smooth(x: Float): Float { val t = x.coerceIn(0f, 1f); return t * t * (3f - 2f * t) }
    private fun outCubic(x: Float): Float { val t = 1f - x.coerceIn(0f, 1f); return 1f - t * t * t }

    private fun text(c: Canvas, s: String, x: Float, y: Float, size: Float, color: Int, alpha: Float, tf: Typeface = face, align: Paint.Align = Paint.Align.LEFT, spacing: Float = 0f) {
        if (alpha <= 0.01f) return
        textP.typeface = tf; textP.textSize = size; textP.textAlign = align; textP.letterSpacing = spacing
        textP.color = color
        textP.alpha = (alpha * 255).roundToInt().coerceIn(0, 255)
        c.drawText(s, x, y, textP)
    }

    private fun drawHeader(c: Canvas) {
        val a = outCubic(intro / 0.9f) * (1f - smooth(launchT * 1.3f - 0.2f))
        // logo : anneau + point en orbite
        val lx = MARGIN + 30f; val ly = 96f
        val tint = universe.tint or (0xFF shl 24)
        stroke.color = Color.WHITE; stroke.alpha = (a * 210).toInt(); stroke.strokeWidth = 4f
        c.drawCircle(lx, ly, 24f, stroke)
        stroke.color = tint; stroke.alpha = (a * 140).toInt(); stroke.strokeWidth = 3f
        c.save(); c.rotate(-24f, lx, ly)
        c.drawOval(lx - 42f, ly - 11f, lx + 42f, ly + 11f, stroke)
        c.restore()
        val oa = time * 1.6f
        fill.color = tint; fill.alpha = (a * 255).toInt()
        c.drawCircle(lx + cos(oa) * 36f, ly + sin(oa) * 36f * 0.0f + sin(oa) * 7f, 5f, fill)
        fill.color = Color.WHITE; fill.alpha = (a * 255).toInt()
        c.drawCircle(lx, ly, 9f, fill)
        text(c, "COSMOS", lx + 70f, ly + 15f, 44f, Color.WHITE, a, face, spacing = 0.32f)

        // horloge
        val cx = DW - MARGIN
        val clockA = a * (1f - 0.1f * idleMix)
        val idleShift = idleMix
        val ex = cx - idleShift * (DW / 2f - MARGIN - 60f)
        val ey = 150f + idleShift * 330f
        val big = 112f + idleShift * 90f
        text(c, timeStr, ex, ey, big, Color.WHITE, clockA, faceThin, Paint.Align.RIGHT)
        text(c, "$greet  ·  $dateStr", ex, ey + 46f + idleShift * 30f, 30f + idleShift * 8f, 0xFFD6D9FF.toInt(), clockA * 0.82f, faceLight, Paint.Align.RIGHT, 0.04f)
        // barre des secondes
        val bw = 300f + idleShift * 160f
        fill.color = Color.WHITE; fill.alpha = (clockA * 38).toInt()
        c.drawRoundRect(ex - bw, ey + 70f + idleShift * 40f, ex, ey + 74f + idleShift * 40f, 2f, 2f, fill)
        fill.color = tint; fill.alpha = (clockA * 255).toInt()
        c.drawRoundRect(ex - bw, ey + 70f + idleShift * 40f, ex - bw + bw * secFrac, ey + 74f + idleShift * 40f, 2f, 2f, fill)
    }

    private fun drawHome(c: Canvas, alpha: Float) {
        val row = rows.getOrNull(focusRow) ?: return
        val ft = row.tiles[focusCol[focusRow]]
        val e = outCubic(titleT)
        val accent = lerpColor(ft.color1 or (0xFF shl 24), Color.WHITE, 0.45f)
        val ia = outCubic((intro - 0.3f) / 0.8f)

        // gros titre de l'élément sélectionné
        text(c, row.title, MARGIN, 292f, 26f, accent, alpha * e * ia, face, spacing = 0.34f)
        val tx = MARGIN - (1f - e) * 70f
        // halo coloré du titre
        text(c, ft.label, tx + 3f, 392f + 3f, 104f, accent, alpha * e * 0.35f * ia, faceBold)
        text(c, ft.label, tx, 392f, 104f, Color.WHITE, alpha * e * ia, faceBold)
        val sub = if (ft.available) ft.subtitle else "Pas encore installée — OK pour l'installer"
        text(c, sub, MARGIN - (1f - e) * 40f, 446f, 36f, 0xFFE3E6FF.toInt(), alpha * e * 0.78f * ia, faceLight, spacing = 0.03f)

        // rangées
        for (i in rows.indices) {
            val y = rowY(i)
            if (y > DH + 60f) continue
            val rel = i - rowScroll
            val above = if (rel < 0f) (1f + rel * 1.35f).coerceIn(0f, 1f) else 1f
            if (above <= 0.01f) continue
            val act = rowActive[i]
            val entry = outCubic((intro - 0.45f - i * 0.16f) / 0.8f)
            val ra = alpha * above * entry
            text(c, rows[i].title, MARGIN, y - 30f, 25f, Color.WHITE, ra * (0.5f + 0.5f * act), face, spacing = 0.3f)
            val nearest = focusPos[i].roundToInt().coerceIn(0, rows[i].tiles.size - 1)
            for (pass in 0..1) {
                for (j in rows[i].tiles.indices) {
                    if ((j == nearest) != (pass == 1)) continue
                    drawRowTile(c, i, j, y, ra, act, entry)
                }
            }
        }
    }

    private fun drawRowTile(c: Canvas, i: Int, j: Int, y: Float, ra: Float, act: Float, entry: Float) {
        val t = rows[i].tiles[j]
        val d = j - focusPos[i]
        val f = max(0f, 1f - abs(d)) * act
        val push = d.coerceIn(-1f, 1f) * 26f * act
        val tEntry = outCubic((intro - 0.55f - i * 0.16f - j * 0.075f) / 0.7f)
        var cx = tileX(i, j.toFloat()) + BW / 2f + push + BW * (1f - (0.9f + 0.1f * act)) / 2f
        if (i == focusRow) cx += 0f + bump * 16f
        val cy = y + BH / 2f + (1f - tEntry) * 80f
        val sc = (0.9f + 0.1f * act) * (0.92f + 0.08f * tEntry) + 0.17f * f
        val tilt = if (f > 0.01f) ((focusCol[i] - focusPos[i]) * 0.9f).coerceIn(-1f, 1f) + sin(time * 1.3f) * 0.1f else 0f
        val a = ra * tEntry * (0.62f + 0.38f * act)
        if (cx + BW < -100f || cx > DW + 100f) return
        drawTile(c, t, cx, cy, sc, f, a, tilt * f, act > 0.5f, BW, BH)
        val x0 = cx - BW * sc / 2f; val y0 = cy - BH * sc / 2f
        hitRects.add(floatArrayOf(x0, y0, x0 + BW * sc, y0 + BH * sc)); hitTiles.add(Hit(t, i, j))
    }

    private fun drawAll(c: Canvas, alpha: Float) {
        val e = smooth(alpha)
        text(c, "TOUTES LES APPS", MARGIN + 6f, 214f, 26f, universe.tint or (0xFF shl 24), alpha, face, spacing = 0.34f)
        text(c, "Bibliothèque", MARGIN, 286f, 68f, Color.WHITE, alpha, faceBold)
        if (apps.isEmpty()) {
            text(c, "Chargement des applications…", DW / 2f, 560f, 40f, Color.WHITE, alpha * 0.7f, faceLight, Paint.Align.CENTER)
            return
        }
        val scale = 256f / BW
        for (idx in apps.indices) {
            val r = idx / gridCols; val col = idx % gridCols
            val y = gridY(r)
            if (y > DH + 40f || y < 250f) continue
            val fade = ((y - 262f) / 70f).coerceIn(0f, 1f)
            val dist = hypot(col - gfx, r - gfy)
            val f = max(0f, 1f - dist)
            val cx = gridX(col) + gridW() / 2f
            val cy = y + gridH() / 2f + (1f - e) * 70f
            val sc = scale * (0.96f + 0.04f * e) + 0.15f * f
            drawTile(c, apps[idx], cx, cy, sc, f, alpha * fade, if (idx == gIdx) sin(time * 1.3f) * 0.1f else 0f, true, BW, BH)
            val x0 = cx - gridW() / 2f; val y0 = cy - gridH() / 2f
            hitRects.add(floatArrayOf(x0, y0, x0 + gridW(), y0 + gridH())); hitTiles.add(Hit(apps[idx], -1, idx))
        }
    }

    /** Dessine une tuile dans son repère local 300x170. */
    private fun drawTile(c: Canvas, t: Tile, cx: Float, cy: Float, sc: Float, f: Float, alpha: Float, tilt: Float, label: Boolean, w: Float, h: Float) {
        if (alpha <= 0.01f) return
        val layer = alpha < 0.98f
        if (layer) c.saveLayerAlpha(cx - w * sc, cy - h * sc, cx + w * sc, cy + h * sc + 70f, (alpha * 255).toInt())
        c.save()
        c.translate(cx, cy)
        c.scale(sc, sc)
        if (abs(tilt) > 0.002f) {
            val k = tilt * 0.09f * h
            val hw = w / 2f; val hh = h / 2f
            src[0] = -hw; src[1] = -hh; src[2] = hw; src[3] = -hh; src[4] = hw; src[5] = hh; src[6] = -hw; src[7] = hh
            if (tilt > 0f) {
                dst[0] = -hw; dst[1] = -hh; dst[2] = hw - k * 0.5f; dst[3] = -hh + k; dst[4] = hw - k * 0.5f; dst[5] = hh - k; dst[6] = -hw; dst[7] = hh
            } else {
                dst[0] = -hw - k * 0.5f; dst[1] = -hh - k; dst[2] = hw; dst[3] = -hh; dst[4] = hw; dst[5] = hh; dst[6] = -hw - k * 0.5f; dst[7] = hh + k
            }
            mtx.setPolyToPoly(src, 0, dst, 0, 4)
            c.concat(mtx)
        }
        c.translate(-w / 2f, -h / 2f)

        val pulse = 0.65f + 0.35f * sin(time * 3.2f)
        // ombre portée
        for (k in 1..4) {
            fill.color = Color.BLACK
            fill.alpha = ((0.14f - k * 0.025f) * (0.7f + f) * 255).toInt().coerceIn(0, 255)
            val g = k * 8f
            c.drawRoundRect(-g, -g + 18f + 10f * f, w + g, h + g + 18f + 10f * f, 28f + g, 28f + g, fill)
        }
        // halo coloré du focus
        if (f > 0.02f) for (k in 1..6) {
            fill.color = t.color1
            fill.alpha = (f * pulse * (0.26f - k * 0.036f) * 255).toInt().coerceIn(0, 255)
            val g = k * 9f
            c.drawRoundRect(-g, -g, w + g, h + g, 26f + g, 26f + g, fill)
        }

        c.save()
        c.clipPath(cardPath)
        val s1 = t.shader ?: LinearGradient(0f, 0f, w, h, t.color1, t.color2, Shader.TileMode.CLAMP).also { t.shader = it }
        fill.shader = s1; fill.alpha = 255
        c.drawRect(0f, 0f, w, h, fill)
        fill.shader = null
        val b = t.banner
        if (b != null) {
            b.setBounds(0, 0, w.toInt(), h.toInt()); b.alpha = 255; b.draw(c)
        } else {
            // décor : cercles lumineux + icône ou lettre
            fill.color = Color.WHITE
            fill.alpha = 24; c.drawCircle(w * 0.88f, h * 0.05f, 110f, fill)
            fill.alpha = 16; c.drawCircle(w * 0.1f, h * 1.05f, 90f, fill)
            val ic = t.icon
            if (ic != null) {
                val s = 104
                ic.setBounds(((w - s) / 2f).toInt(), ((h - s) / 2f).toInt(), ((w + s) / 2f).toInt(), ((h + s) / 2f).toInt())
                ic.draw(c)
            } else {
                text(c, t.glyph, w / 2f, h / 2f + 34f, if (t.glyph.length > 2) 76f else 108f, Color.WHITE, 0.96f, faceBold, Paint.Align.CENTER)
            }
        }
        // dégradé bas + reflet
        fill.shader = shadeShader; fill.alpha = 255; c.drawRect(0f, 0f, w, h, fill)
        fill.shader = glossShader; c.drawRect(0f, 0f, w, h, fill)
        fill.shader = null
        if (!t.available) {
            fill.color = 0xB0070812.toInt(); c.drawRect(0f, 0f, w, h, fill)
            text(c, "INSTALLER", w / 2f, h - 22f, 24f, Color.WHITE, 0.9f, face, Paint.Align.CENTER, 0.28f)
        }
        // balayage lumineux sur la tuile sélectionnée
        if (f > 0.25f) {
            val ph = (time * 0.62f) % 2.4f
            if (ph < 1f) {
                c.save()
                c.translate(-220f + ph * (w + 440f), 0f)
                c.skew(-0.28f, 0f)
                fill.shader = sweepShader; fill.alpha = (f * 255).toInt()
                c.drawRect(0f, -20f, 170f, h + 20f, fill)
                fill.shader = null
                c.restore()
            }
        }
        c.restore()

        // liseré
        stroke.color = Color.WHITE
        stroke.alpha = (40 + 215 * f).toInt().coerceIn(0, 255)
        stroke.strokeWidth = 2f + 3f * f
        c.drawRoundRect(1f, 1f, w - 1f, h - 1f, 26f, 26f, stroke)

        if (label && f < 0.5f) {
            val sl = t.shortLabel ?: shorten(t.label, 280f).also { t.shortLabel = it }
            text(c, sl, w / 2f, h + 44f, 27f, Color.WHITE, 0.62f * (1f - f * 2f) + 0.0f, face, Paint.Align.CENTER, 0.02f)
        }
        c.restore()
        if (layer) c.restore()
    }

    private fun shorten(s: String, maxW: Float): String {
        textP.typeface = face; textP.textSize = 27f; textP.letterSpacing = 0.02f
        if (textP.measureText(s) <= maxW) return s
        var n = s.length
        while (n > 1 && textP.measureText(s, 0, n) + textP.measureText("…") > maxW) n--
        return s.substring(0, n).trimEnd() + "…"
    }

    private fun drawEffects(c: Canvas) {
        for (i in 0 until pn) {
            val l = plife[i]
            if (l <= 0f) continue
            val k = l / pmax[i]
            fill.color = pcol[i]
            fill.alpha = (k * 235).toInt().coerceIn(0, 255)
            c.drawCircle(ppx[i], ppy[i], psize[i] * (0.4f + k), fill)
        }
        for (k in 0 until 4) {
            val t = rings[k * 4 + 2]
            if (t <= 0f) continue
            val col = rings[k * 4 + 3].toBits()
            stroke.color = col
            stroke.alpha = ((1f - t) * 200).toInt().coerceIn(0, 255)
            stroke.strokeWidth = 6f * (1f - t) + 1f
            c.drawCircle(rings[k * 4], rings[k * 4 + 1], outCubic(t) * 900f, stroke)
        }
    }

    /** Pendant le lancement : la tuile choisie grossit et s'évanouit dans la vitesse-lumière. */
    private fun drawLaunchTile(c: Canvas) {
        val t = launching ?: return
        val k = launchT.coerceIn(0f, 1f)
        val a = 1f - smooth((k - 0.25f) / 0.6f)
        val sc = 1.17f + outCubic(k) * 2.6f
        fill.color = Color.WHITE
        fill.alpha = (smooth((k - 0.55f) / 0.45f) * 255).toInt()
        c.drawRect(-DW, -DH, DW * 2f, DH * 2f, fill)
        drawTile(c, t, lerp(launchX, DW / 2f, k), lerp(launchY, DH / 2f, k), sc, 1f, a, 0f, false, BW, BH)
    }

    private fun lerp(a: Float, b: Float, t: Float) = a + (b - a) * t
}
