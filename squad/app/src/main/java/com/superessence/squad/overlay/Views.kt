package com.superessence.squad.overlay

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import com.superessence.squad.model.Look
import com.superessence.squad.render.MascotPainter
import com.superessence.squad.render.Pose
import kotlin.math.hypot

/** Vue d'une mascotte posée sur l'écran. */
@SuppressLint("ViewConstructor")
class SpriteView(ctx: Context, var look: Look, val pose: Pose) : View(ctx) {
    interface Listener {
        fun onTap(v: SpriteView)
        fun onDoubleTap(v: SpriteView)
        fun onLongPress(v: SpriteView)
        fun onDragStart(v: SpriteView)
        fun onDrag(v: SpriteView, rawX: Float, rawY: Float)
        fun onDrop(v: SpriteView, rawX: Float, rawY: Float)
    }

    var listener: Listener? = null
    private val slop = ViewConfiguration.get(ctx).scaledTouchSlop * 1.5f
    private var downX = 0f; private var downY = 0f; private var downT = 0L
    private var dragging = false
    private var lastTap = 0L
    private val longPress = Runnable { if (!dragging) { longFired = true; listener?.onLongPress(this) } }
    private var longFired = false
    var dimmed = false

    override fun onDraw(canvas: Canvas) {
        if (dimmed) canvas.saveLayerAlpha(0f, 0f, width.toFloat(), height.toFloat(), 150)
        MascotPainter.draw(canvas, look, pose, width.toFloat(), height.toFloat())
        if (dimmed) canvas.restore()
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                downX = e.rawX; downY = e.rawY; downT = System.currentTimeMillis(); dragging = false; longFired = false
                postDelayed(longPress, 650)
            }
            MotionEvent.ACTION_MOVE -> {
                if (!dragging && hypot(e.rawX - downX, e.rawY - downY) > slop) {
                    dragging = true; removeCallbacks(longPress); listener?.onDragStart(this)
                }
                if (dragging) listener?.onDrag(this, e.rawX, e.rawY)
            }
            MotionEvent.ACTION_UP -> {
                removeCallbacks(longPress)
                if (dragging) listener?.onDrop(this, e.rawX, e.rawY)
                else if (!longFired) {
                    val now = System.currentTimeMillis()
                    if (now - lastTap < 350) { lastTap = 0; listener?.onDoubleTap(this) } else { lastTap = now; listener?.onTap(this) }
                }
                dragging = false
            }
            MotionEvent.ACTION_CANCEL -> {
                removeCallbacks(longPress)
                if (dragging) listener?.onDrop(this, e.rawX, e.rawY)
                dragging = false
            }
        }
        return true
    }
}

/** Bulle de dialogue (non tactile). */
@SuppressLint("ViewConstructor")
class BubbleView(ctx: Context, private val density: Float) : View(ctx) {
    private val tp = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF111111.toInt(); textSize = 14f * density; typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
    }
    private val namePaint = TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF8A6A00.toInt(); textSize = 10.5f * density; typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD); letterSpacing = 0.08f
    }
    private val bg = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFFFFFFF.toInt() }
    private val border = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFFFC928.toInt(); style = Paint.Style.STROKE; strokeWidth = 2.5f * density }
    private var layout: StaticLayout? = null
    private var name = ""
    private val r = RectF()
    private val path = Path()
    private val pad = 10f * density
    private val tail = 9f * density
    /** Position horizontale de la pointe (0..1). */
    var tailAt = 0.5f

    fun set(name: String, text: String, maxW: Int): Pair<Int, Int> {
        this.name = name.uppercase()
        val inner = (maxW - 2 * pad).toInt().coerceAtLeast(80)
        val want = StaticLayout.getDesiredWidth(text, tp).toInt().coerceAtMost(inner)
        val nameW = namePaint.measureText(this.name).toInt()
        val w = maxOf(want, nameW)
        layout = StaticLayout.Builder.obtain(text, 0, text.length, tp, w).setAlignment(Layout.Alignment.ALIGN_NORMAL).build()
        val h = (layout!!.height + namePaint.textSize + 4 * density + 2 * pad + tail).toInt()
        invalidate()
        return (w + 2 * pad).toInt() to h
    }

    override fun onDraw(c: Canvas) {
        val bodyH = height - tail
        r.set(border.strokeWidth, border.strokeWidth, width - border.strokeWidth, bodyH)
        path.reset()
        path.addRoundRect(r, 14f * density, 14f * density, Path.Direction.CW)
        val tx = (width * tailAt).coerceIn(20f * density, width - 20f * density)
        path.moveTo(tx - 8 * density, bodyH - 1); path.lineTo(tx, height.toFloat() - 1); path.lineTo(tx + 8 * density, bodyH - 1); path.close()
        c.drawPath(path, bg); c.drawPath(path, border)
        c.drawText(name, pad, pad + namePaint.textSize * 0.85f, namePaint)
        c.save(); c.translate(pad, pad + namePaint.textSize + 4 * density); layout?.draw(c); c.restore()
    }
}

/** Zone « ranger » qui apparaît en bas pendant qu'on porte une mascotte. */
class TrashView(ctx: Context, private val density: Float) : View(ctx) {
    var hot = false
        set(v) { if (field != v) { field = v; invalidate() } }
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER; typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD) }

    override fun onDraw(c: Canvas) {
        val cx = width / 2f; val cy = height * 0.42f
        val rad = (if (hot) 36f else 30f) * density
        p.color = if (hot) 0xFFE53935.toInt() else 0xE6111111.toInt(); p.style = Paint.Style.FILL
        c.drawCircle(cx, cy, rad, p)
        p.color = 0xFFFFC928.toInt(); p.style = Paint.Style.STROKE; p.strokeWidth = 3f * density
        c.drawCircle(cx, cy, rad, p)
        p.color = 0xFFFFFFFF.toInt(); p.strokeWidth = 4f * density; p.strokeCap = Paint.Cap.ROUND
        val k = 10f * density
        c.drawLine(cx - k, cy - k, cx + k, cy + k, p); c.drawLine(cx + k, cy - k, cx - k, cy + k, p)
        tp.color = 0xFFFFFFFF.toInt(); tp.textSize = 12f * density
        tp.setShadowLayer(4f * density, 0f, 0f, 0xFF000000.toInt())
        c.drawText(if (hot) "Lâche pour ranger" else "Ranger ici", cx, height - 6f * density, tp)
    }
}
