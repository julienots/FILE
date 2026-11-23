package com.superessence.squad.overlay

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ServiceInfo
import android.content.res.Configuration
import android.graphics.PixelFormat
import android.os.Build
import android.os.IBinder
import android.provider.Settings as AndroidSettings
import android.view.Choreographer
import android.view.Gravity
import android.view.HapticFeedbackConstants
import android.view.WindowManager
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.superessence.squad.R
import com.superessence.squad.engine.Life
import com.superessence.squad.engine.Phase
import com.superessence.squad.engine.Squad
import com.superessence.squad.engine.SquadState
import com.superessence.squad.model.Act
import com.superessence.squad.model.Mascot
import com.superessence.squad.model.Say
import com.superessence.squad.render.Pose
import com.superessence.squad.ui.MainActivity
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlin.math.hypot
import kotlin.random.Random

/**
 * Service au premier plan qui garde les mascottes vivantes sur l'écran,
 * même quand l'application est fermée.
 */
class OverlayService : Service(), SpriteView.Listener, Choreographer.FrameCallback {

    companion object {
        const val ACTION_STOP = "com.superessence.squad.STOP"
        const val ACTION_RECALL = "com.superessence.squad.RECALL"
        private const val CHANNEL = "squad_live"
        private const val NOTIF_ID = 42

        fun canRun(ctx: Context) = AndroidSettings.canDrawOverlays(ctx)

        fun start(ctx: Context) {
            if (!canRun(ctx)) return
            ContextCompat.startForegroundService(ctx, Intent(ctx, OverlayService::class.java))
        }

        fun stop(ctx: Context) {
            ctx.startService(Intent(ctx, OverlayService::class.java).setAction(ACTION_STOP))
        }
    }

    private inner class Sprite(val id: String, val view: SpriteView, val lp: WindowManager.LayoutParams) {
        val brain = Brain()
        var x = 0f; var y = 0f
        var tx = 0f; var ty = 0f
        var dragged = false
        var bubble: BubbleView? = null
        var bubbleLp: WindowManager.LayoutParams? = null
        var bubbleLeft = 0f
        var phase: Phase? = null
        var lastSaved = 0L
    }

    private lateinit var wm: WindowManager
    private val scope = MainScope()
    private val sprites = LinkedHashMap<String, Sprite>()
    private var trash: TrashView? = null
    private var trashLp: WindowManager.LayoutParams? = null
    private var running = false
    private var screenOn = true
    private var lastFrame = 0L
    private var t = 0f
    private var tickAcc = 0f
    private var socialAcc = 0f
    private var screenW = 0; private var screenH = 0
    private val density by lazy { resources.displayMetrics.density }
    private val rnd = Random.Default

    private val screenReceiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context, i: Intent) {
            screenOn = i.action != Intent.ACTION_SCREEN_OFF
            if (screenOn) { Squad.tick(); startLoop() } else stopLoop()
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        Squad.init(this)
        wm = getSystemService(WINDOW_SERVICE) as WindowManager
        measureScreen()
        createChannel()
        val type = if (Build.VERSION.SDK_INT >= 34) ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE else 0
        if (Build.VERSION.SDK_INT >= 29) startForeground(NOTIF_ID, buildNotif(Squad.state.value), type)
        else startForeground(NOTIF_ID, buildNotif(Squad.state.value))
        registerReceiver(screenReceiver, IntentFilter().apply { addAction(Intent.ACTION_SCREEN_OFF); addAction(Intent.ACTION_SCREEN_ON) })
        scope.launch { Squad.state.collectLatest { sync(it) } }
        startLoop()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                Squad.edit { it.copy(settings = it.settings.copy(overlayOn = false)) }
                stopSelf()
                return START_NOT_STICKY
            }
            ACTION_RECALL -> Squad.edit { s -> s.copy(mascots = s.mascots.map { it.copy(onScreen = true) }) }
        }
        if (!canRun(this)) { stopSelf(); return START_NOT_STICKY }
        return START_STICKY
    }

    override fun onDestroy() {
        stopLoop()
        runCatching { unregisterReceiver(screenReceiver) }
        savePositions()
        sprites.values.toList().forEach { removeSprite(it) }
        hideTrash()
        scope.cancel()
        super.onDestroy()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        measureScreen()
        sprites.values.forEach { clamp(it); it.tx = it.x; it.ty = it.y }
    }

    private fun measureScreen() {
        if (Build.VERSION.SDK_INT >= 30) {
            val b = wm.currentWindowMetrics.bounds
            screenW = b.width(); screenH = b.height()
        } else {
            val dm = resources.displayMetrics
            screenW = dm.widthPixels; screenH = dm.heightPixels
        }
    }

    // ------------------------------------------------------------------ fenêtres

    private fun overlayType() = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY

    private fun spriteSize(): Pair<Int, Int> {
        val k = Squad.state.value.settings.size
        return (78 * density * k).toInt() to (110 * density * k).toInt()
    }

    private fun sync(s: SquadState) {
        val phase = Life.phaseNow(s.schedule)
        val visible = s.mascots.filter { it.onScreen && (s.settings.showSleeping || phase != Phase.SLEEP) }
        val ids = visible.map { it.id }.toSet()
        sprites.values.filter { it.id !in ids }.forEach { removeSprite(it) }
        val (w, h) = spriteSize()
        for (m in visible) {
            val sp = sprites[m.id]
            if (sp == null) addSprite(m, w, h)
            else {
                if (sp.view.look != m.look) { sp.view.look = m.look; sp.view.invalidate() }
                sp.view.pose.fitness = m.fitness
                if (sp.lp.width != w) { sp.lp.width = w; sp.lp.height = h; clamp(sp); runCatching { wm.updateViewLayout(sp.view, sp.lp) } }
            }
        }
        updateNotif(s)
    }

    private fun addSprite(m: Mascot, w: Int, h: Int) {
        val pose = Pose().apply { seed = m.id.hashCode() and 0xff; fitness = m.fitness }
        val v = SpriteView(this, m.look, pose)
        v.listener = this
        v.tag = m.id
        val lp = WindowManager.LayoutParams(
            w, h, overlayType(),
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply { gravity = Gravity.TOP or Gravity.START }
        val sp = Sprite(m.id, v, lp)
        if (m.x in 0f..1f && m.y in 0f..1f) { sp.x = m.x * (screenW - w); sp.y = m.y * (screenH - h) }
        else { sp.x = rnd.nextFloat() * (screenW - w); sp.y = screenH * (0.35f + rnd.nextFloat() * 0.45f) }
        clamp(sp)
        sp.tx = sp.x; sp.ty = sp.y
        lp.x = sp.x.toInt(); lp.y = sp.y.toInt()
        try { wm.addView(v, lp) } catch (e: Exception) { return }
        sprites[m.id] = sp
        sp.brain.force(Act.WAVE, 2.5f)
        if (rnd.nextFloat() < 0.6f) say(sp, m, Say.TAP)
    }

    private fun removeSprite(sp: Sprite) {
        hideBubble(sp)
        runCatching { wm.removeView(sp.view) }
        sprites.remove(sp.id)
    }

    private fun clamp(sp: Sprite) {
        sp.x = sp.x.coerceIn(0f, (screenW - sp.lp.width).toFloat().coerceAtLeast(0f))
        sp.y = sp.y.coerceIn(0f, (screenH - sp.lp.height).toFloat().coerceAtLeast(0f))
    }

    // ------------------------------------------------------------------ boucle d'animation

    private fun startLoop() {
        if (running || !screenOn) return
        running = true; lastFrame = 0L
        Choreographer.getInstance().postFrameCallback(this)
    }

    private fun stopLoop() {
        running = false
        Choreographer.getInstance().removeFrameCallback(this)
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (!running) return
        if (lastFrame == 0L) lastFrame = frameTimeNanos
        val dt = ((frameTimeNanos - lastFrame) / 1e9f)
        // ~30 i/s en éveil, ~12 i/s quand tout le monde dort
        val allAsleep = sprites.values.isNotEmpty() && sprites.values.all { it.brain.act == Act.SLEEP && !it.dragged }
        val minDt = if (allAsleep) 0.08f else 0.032f
        if (dt >= minDt) {
            lastFrame = frameTimeNanos
            step(dt.coerceAtMost(0.25f))
        }
        Choreographer.getInstance().postFrameCallback(this)
    }

    private fun step(dt: Float) {
        t += dt
        tickAcc += dt
        if (tickAcc > 60f) { tickAcc = 0f; Squad.tick(); savePositions() }
        val st = Squad.state.value
        val phase = Life.phaseNow(st.schedule)
        val speed = 46f * density * st.settings.speed
        for (sp in sprites.values.toList()) {
            val m = Squad.get(sp.id) ?: continue
            // changement de phase : réplique adaptée
            if (sp.phase != phase) {
                val prev = sp.phase
                sp.phase = phase
                if (prev != null) when {
                    phase == Phase.WAKING -> say(sp, m, Say.WAKE)
                    phase.isMeal -> say(sp, m, Say.EAT)
                    phase == Phase.WORKOUT -> say(sp, m, Say.TRAIN)
                    phase == Phase.WIND_DOWN || phase == Phase.SLEEP -> say(sp, m, Say.SLEEPY)
                }
            }
            val arrived = hypot(sp.tx - sp.x, sp.ty - sp.y) < 4f
            if (!sp.dragged) {
                val line = sp.brain.step(dt, m, phase, st.settings.chatter, arrived)
                if (line != null) say(sp, m, line)
                if (sp.brain.wantsTarget) {
                    sp.brain.wantsTarget = false
                    sp.tx = rnd.nextFloat() * (screenW - sp.lp.width)
                    sp.ty = (sp.y + (rnd.nextFloat() - 0.5f) * screenH * 0.6f).coerceIn(0f, (screenH - sp.lp.height).toFloat())
                }
                if (sp.brain.act == Act.WALK) {
                    val dx = sp.tx - sp.x; val dy = sp.ty - sp.y
                    val d = hypot(dx, dy)
                    if (d > 1f) {
                        val v = speed * (0.75f + 0.3f * m.archetype.energy) * (if (m.energy < 25) 0.6f else 1f)
                        val k = minOf(1f, v * dt / d)
                        sp.x += dx * k; sp.y += dy * k
                        if (kotlin.math.abs(dx) > 2f) sp.view.pose.facing = if (dx > 0) 1f else -1f
                    }
                }
            }
            sp.view.pose.act = if (sp.dragged) Act.DRAGGED else sp.brain.act
            sp.view.pose.t = t
            sp.view.pose.talking = sp.bubble != null
            sp.view.dimmed = sp.brain.act == Act.SLEEP && !sp.dragged
            sp.view.invalidate()
            val nx = sp.x.toInt(); val ny = sp.y.toInt()
            if (nx != sp.lp.x || ny != sp.lp.y) {
                sp.lp.x = nx; sp.lp.y = ny
                runCatching { wm.updateViewLayout(sp.view, sp.lp) }
            }
            // bulle
            if (sp.bubble != null) {
                sp.bubbleLeft -= dt
                if (sp.bubbleLeft <= 0f) hideBubble(sp) else placeBubble(sp)
            }
        }
        social(dt)
    }

    /** Quand deux mascottes se croisent, elles se parlent. */
    private fun social(dt: Float) {
        socialAcc += dt
        if (socialAcc < 1.5f || sprites.size < 2) return
        socialAcc = 0f
        val list = sprites.values.filter { !it.dragged && it.brain.act != Act.SLEEP && it.bubble == null && !it.brain.isForced }
        for (i in list.indices) for (j in i + 1 until list.size) {
            val a = list[i]; val b = list[j]
            if (hypot(a.x - b.x, a.y - b.y) < 130 * density && rnd.nextFloat() < 0.08f) {
                val m = Squad.get(a.id) ?: return
                a.brain.force(Act.WAVE, 3f); b.brain.force(Act.TALK, 3f)
                a.view.pose.facing = if (b.x > a.x) 1f else -1f
                b.view.pose.facing = -a.view.pose.facing
                say(a, m, Say.FRIEND)
                return
            }
        }
    }

    // ------------------------------------------------------------------ paroles

    private fun say(sp: Sprite, m: Mascot, what: Say) {
        val lines = m.archetype.lines(what)
        if (lines.isEmpty()) return
        showBubble(sp, m.name, lines.random())
    }

    private fun showBubble(sp: Sprite, name: String, text: String) {
        val maxW = (220 * density).toInt().coerceAtMost(screenW - (16 * density).toInt())
        val b = sp.bubble ?: BubbleView(this, density)
        val (bw, bh) = b.set(name, text, maxW)
        val lp = sp.bubbleLp ?: WindowManager.LayoutParams(
            bw, bh, overlayType(),
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            // Android 12+ : une fenêtre non tactile doit rester ≤ 80 % opaque pour ne pas bloquer les touches
            alpha = 0.8f
        }
        lp.width = bw; lp.height = bh
        sp.bubbleLeft = 3.2f + text.length * 0.045f
        if (sp.bubble == null) {
            sp.bubble = b; sp.bubbleLp = lp
            placeBubble(sp, add = true)
        } else placeBubble(sp)
    }

    private fun placeBubble(sp: Sprite, add: Boolean = false) {
        val b = sp.bubble ?: return
        val lp = sp.bubbleLp ?: return
        val cx = sp.x + sp.lp.width / 2f
        val bx = (cx - lp.width / 2f).coerceIn(0f, (screenW - lp.width).toFloat().coerceAtLeast(0f))
        val by = (sp.y - lp.height + sp.lp.height * 0.12f).coerceAtLeast(0f)
        b.tailAt = ((cx - bx) / lp.width).coerceIn(0.1f, 0.9f)
        val nx = bx.toInt(); val ny = by.toInt()
        if (add) {
            lp.x = nx; lp.y = ny
            runCatching { wm.addView(b, lp) }.onFailure { sp.bubble = null; sp.bubbleLp = null }
        } else if (nx != lp.x || ny != lp.y || b.width != lp.width) {
            lp.x = nx; lp.y = ny
            runCatching { wm.updateViewLayout(b, lp) }
            b.invalidate()
        }
    }

    private fun hideBubble(sp: Sprite) {
        sp.bubble?.let { runCatching { wm.removeView(it) } }
        sp.bubble = null; sp.bubbleLp = null
    }

    // ------------------------------------------------------------------ interactions

    private fun spriteOf(v: SpriteView) = sprites[v.tag as String]

    private fun buzz(v: SpriteView) {
        if (Squad.state.value.settings.vibrate) v.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
    }

    override fun onTap(v: SpriteView) {
        val sp = spriteOf(v) ?: return
        val m = Squad.get(sp.id) ?: return
        buzz(v)
        if (sp.brain.act == Act.SLEEP) { say(sp, m, Say.NIGHT); return }
        when {
            m.satiety < 25f -> say(sp, m, Say.HUNGRY)
            m.energy < 20f -> { sp.brain.force(Act.YAWN, 2.5f); say(sp, m, Say.SLEEPY) }
            else -> { sp.brain.force(Act.TALK, 3f); say(sp, m, Say.TAP) }
        }
    }

    override fun onDoubleTap(v: SpriteView) {
        val sp = spriteOf(v) ?: return
        val m = Squad.get(sp.id) ?: return
        buzz(v)
        Squad.pet(sp.id)
        if (sp.brain.act != Act.SLEEP) sp.brain.force(Act.FLEX, 3f)
        say(sp, m, Say.PET)
    }

    override fun onLongPress(v: SpriteView) {
        val sp = spriteOf(v) ?: return
        buzz(v)
        startActivity(
            Intent(this, MainActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)
                .putExtra(MainActivity.EXTRA_MASCOT, sp.id),
        )
    }

    override fun onDragStart(v: SpriteView) {
        val sp = spriteOf(v) ?: return
        val m = Squad.get(sp.id) ?: return
        sp.dragged = true
        sp.brain.clearForce()
        buzz(v)
        showTrash()
        say(sp, m, if (sp.brain.act == Act.SLEEP) Say.NIGHT else Say.DRAG)
    }

    override fun onDrag(v: SpriteView, rawX: Float, rawY: Float) {
        val sp = spriteOf(v) ?: return
        sp.x = rawX - sp.lp.width / 2f
        sp.y = rawY - sp.lp.height * 0.2f
        clamp(sp)
        sp.tx = sp.x; sp.ty = sp.y
        trash?.hot = overTrash(rawX, rawY)
    }

    override fun onDrop(v: SpriteView, rawX: Float, rawY: Float) {
        val sp = spriteOf(v) ?: return
        sp.dragged = false
        val removeIt = overTrash(rawX, rawY)
        hideTrash()
        if (removeIt) {
            buzz(v)
            Squad.editMascot(sp.id) { it.copy(onScreen = false).withEvent("Rangé dans l'appli") }
            return
        }
        val m = Squad.get(sp.id) ?: return
        sp.tx = sp.x; sp.ty = sp.y
        if (sp.brain.act != Act.SLEEP) { sp.brain.force(Act.IDLE, 2f); say(sp, m, Say.DROP) }
        saveOne(sp)
    }

    private fun overTrash(rawX: Float, rawY: Float): Boolean {
        val lp = trashLp ?: return false
        val cx = lp.x + lp.width / 2f
        val cy = lp.y + lp.height * 0.42f
        return hypot(rawX - cx, rawY - cy) < 70 * density
    }

    private fun showTrash() {
        if (trash != null) return
        val size = (120 * density).toInt()
        val v = TrashView(this, density)
        val lp = WindowManager.LayoutParams(
            size, size, overlayType(),
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = (screenW - size) / 2; y = screenH - size - (40 * density).toInt()
            alpha = 0.8f
        }
        runCatching { wm.addView(v, lp); trash = v; trashLp = lp }
    }

    private fun hideTrash() {
        trash?.let { runCatching { wm.removeView(it) } }
        trash = null; trashLp = null
    }

    // ------------------------------------------------------------------ sauvegarde des positions

    private fun saveOne(sp: Sprite) {
        val fx = if (screenW > sp.lp.width) sp.x / (screenW - sp.lp.width) else 0.5f
        val fy = if (screenH > sp.lp.height) sp.y / (screenH - sp.lp.height) else 0.5f
        Squad.editMascot(sp.id) { it.copy(x = fx, y = fy) }
    }

    private fun savePositions() = sprites.values.toList().forEach { saveOne(it) }

    // ------------------------------------------------------------------ notification

    private fun createChannel() {
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(NotificationChannel(CHANNEL, "Squad à l'écran", NotificationManager.IMPORTANCE_LOW).apply {
            description = "Garde tes mascottes vivantes par-dessus les autres applis"
            setShowBadge(false)
        })
    }

    private var lastNotifText = ""

    private fun buildNotif(s: SquadState): Notification {
        val n = s.mascots.count { it.onScreen }
        val phase = Life.phaseNow(s.schedule)
        val text = when {
            s.mascots.isEmpty() -> "Ta squad est vide"
            n == 0 -> "Tout le monde est rangé"
            else -> "$n sur l'écran · ${phase.label.lowercase()}"
        }
        lastNotifText = text
        val flags = PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), flags)
        val stop = PendingIntent.getService(this, 1, Intent(this, OverlayService::class.java).setAction(ACTION_STOP), flags)
        val recall = PendingIntent.getService(this, 2, Intent(this, OverlayService::class.java).setAction(ACTION_RECALL), flags)
        return NotificationCompat.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_stat)
            .setContentTitle("SQUAD. est en vie")
            .setContentText(text)
            .setContentIntent(open)
            .setOngoing(true)
            .setSilent(true)
            .addAction(0, "Tout rappeler", recall)
            .addAction(0, "Arrêter", stop)
            .build()
    }

    private fun updateNotif(s: SquadState) {
        val before = lastNotifText
        val n = buildNotif(s)
        if (lastNotifText != before) getSystemService(NotificationManager::class.java).notify(NOTIF_ID, n)
    }
}

/** Relance la squad après un redémarrage du téléphone. */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        Squad.init(context)
        if (Squad.state.value.settings.overlayOn) runCatching { OverlayService.start(context) }
    }
}
