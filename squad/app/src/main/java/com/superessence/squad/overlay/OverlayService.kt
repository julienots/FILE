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

        const val ACTION_FIGHT = "com.superessence.squad.FIGHT"
        const val EXTRA_A = "a"
        const val EXTRA_B = "b"

        /** Lance une baston entre deux mascottes à l'écran. */
        fun fight(ctx: Context, a: String, b: String) {
            if (!canRun(ctx)) return
            ContextCompat.startForegroundService(ctx, Intent(ctx, OverlayService::class.java).setAction(ACTION_FIGHT).putExtra(EXTRA_A, a).putExtra(EXTRA_B, b))
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
        // rencontres
        var enc: Encounter? = null
        var encAct: Act? = null
        var encMove = false
        var encSpeed = 1f
        var hp = 100f
        var punchT = 0f
        val cx get() = x + lp.width / 2f
        val cy get() = y + lp.height / 2f
    }

    private enum class Kind { FIGHT, TALK, HIGHFIVE, CHASE, TRAIN }

    private inner class Encounter(val kind: Kind, val a: Sprite, val b: Sprite) {
        var stage = 0
        var t = 0f
        var beat = 0f
        var n = 0
        var winner: Sprite? = null
        var loser: Sprite? = null
    }

    private val encounters = mutableListOf<Encounter>()
    private var nextEncounter = 10f

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
            ACTION_FIGHT -> {
                val a = intent.getStringExtra(EXTRA_A); val b = intent.getStringExtra(EXTRA_B)
                if (a != null && b != null && a != b) {
                    Squad.edit { s -> s.copy(mascots = s.mascots.map { if (it.id == a || it.id == b) it.copy(onScreen = true) else it }) }
                    requestFight(a, b, 0)
                }
            }
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
        sp.enc?.let { endEncounter(it) }
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
                if (sp.enc == null) {
                    val line = sp.brain.step(dt, m, phase, st.settings.chatter, arrived)
                    if (line != null) say(sp, m, line)
                    if (sp.brain.wantsTarget) {
                        sp.brain.wantsTarget = false
                        sp.tx = rnd.nextFloat() * (screenW - sp.lp.width)
                        sp.ty = (sp.y + (rnd.nextFloat() - 0.5f) * screenH * 0.6f).coerceIn(0f, (screenH - sp.lp.height).toFloat())
                    }
                }
                val moving = if (sp.enc != null) sp.encMove else sp.brain.act == Act.WALK
                if (moving) {
                    val dx = sp.tx - sp.x; val dy = sp.ty - sp.y
                    val d = hypot(dx, dy)
                    if (d > 1f) {
                        val v = speed * (0.75f + 0.3f * m.archetype.energy) * (if (m.energy < 25) 0.6f else 1f) * (if (sp.enc != null) sp.encSpeed else 1f)
                        val k = minOf(1f, v * dt / d)
                        sp.x += dx * k; sp.y += dy * k
                        if (kotlin.math.abs(dx) > 2f) sp.view.pose.facing = if (dx > 0) 1f else -1f
                    }
                }
            }
            val pose = sp.view.pose
            pose.act = when {
                sp.dragged -> Act.DRAGGED
                sp.encAct != null -> sp.encAct!!
                else -> sp.brain.act
            }
            pose.t = t
            pose.talking = sp.bubble != null
            sp.punchT -= dt
            pose.punch = if (sp.punchT > 0f) 1f else 0.1f
            if (pose.fx != null) { pose.fxAge += dt; if (pose.fxAge > 0.55f) pose.fx = null }
            sp.view.dimmed = pose.act == Act.SLEEP
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
        encounters(dt, phase, st.settings)
    }

    // ------------------------------------------------------------------ rencontres entre mascottes

    private fun power(m: Mascot) = m.archetype.skill * (0.6f + m.fitness / 100f * 0.6f + m.level * 0.03f) * (0.7f + m.energy / 100f * 0.3f)

    private fun free(sp: Sprite) = !sp.dragged && sp.enc == null && sp.brain.act != Act.SLEEP

    /** De temps en temps, deux gars vont interagir. */
    private fun encounters(dt: Float, phase: Phase, set: com.superessence.squad.model.Settings) {
        for (e in encounters.toList()) stepEncounter(e, dt)
        if (set.social <= 0f || phase == Phase.SLEEP || phase.isMeal) return
        nextEncounter -= dt * set.social
        if (nextEncounter > 0f) return
        nextEncounter = 9f + rnd.nextFloat() * 18f
        val pool = sprites.values.filter { free(it) }
        if (pool.size < 2) return
        val a = pool.random()
        val b = pool.filter { it !== a }.minByOrNull { hypot(it.x - a.x, it.y - a.y) * (0.6f + rnd.nextFloat()) } ?: return
        val ma = Squad.get(a.id) ?: return
        val mb = Squad.get(b.id) ?: return
        val rel = ma.relWith(b.id)
        val tired = ma.energy < 20f || mb.energy < 20f
        val w = linkedMapOf(
            Kind.FIGHT to if (set.fights && !tired) (ma.archetype.aggro + mb.archetype.aggro) * 2.2f * (1f + maxOf(0, -rel) / 40f) * (if (ma.mood < 35f) 1.5f else 1f) else 0f,
            Kind.TALK to (ma.archetype.chatty + mb.archetype.chatty) * 1.4f,
            Kind.HIGHFIVE to if (tired) 0.3f else 0.9f + maxOf(0, rel) / 25f,
            Kind.CHASE to if (tired) 0f else 0.5f * (ma.archetype.energy + mb.archetype.energy),
            Kind.TRAIN to if (tired) 0f else if (phase == Phase.WORKOUT) 6f else 0.5f,
        )
        var r = rnd.nextFloat() * w.values.sum()
        val kind = w.entries.firstOrNull { r -= it.value; r < 0f }?.key ?: Kind.TALK
        startEncounter(kind, a, b)
    }

    private fun requestFight(a: String, b: String, tries: Int) {
        val sa = sprites[a]; val sb = sprites[b]
        if (sa == null || sb == null) {
            if (tries < 10) android.os.Handler(mainLooper).postDelayed({ requestFight(a, b, tries + 1) }, 300)
            return
        }
        sa.enc?.let { endEncounter(it) }; sb.enc?.let { endEncounter(it) }
        startEncounter(Kind.FIGHT, sa, sb)
    }

    private fun startEncounter(kind: Kind, a: Sprite, b: Sprite) {
        val e = Encounter(kind, a, b)
        encounters += e
        for (sp in listOf(a, b)) { sp.enc = e; sp.hp = 100f; sp.brain.clearForce(); sp.encSpeed = 1f }
        val ma = Squad.get(a.id) ?: return endEncounter(e)
        when (kind) {
            Kind.FIGHT -> { meet(e, 0.62f, if (ma.archetype.aggro > 0.6f) 1.6f else 1.2f); say(a, ma, Say.TAUNT) }
            Kind.TALK -> meet(e, 1.0f, 1f)
            Kind.HIGHFIVE -> meet(e, 0.78f, 1.2f)
            Kind.TRAIN -> meet(e, 1.05f, 1f)
            Kind.CHASE -> {
                e.stage = 1
                a.encAct = Act.RUN; a.encMove = true; a.encSpeed = 1.75f
                b.encAct = Act.RUN; b.encMove = true; b.encSpeed = 1.4f
                fleeTarget(b, a)
                say(a, ma, Say.CHASE)
            }
        }
    }

    /** Les deux marchent l'un vers l'autre jusqu'à se faire face. */
    private fun meet(e: Encounter, gapK: Float, speed: Float) {
        val a = e.a; val b = e.b
        val w = a.lp.width
        val gap = w * gapK
        val left = if (a.x <= b.x) a else b
        val right = if (left === a) b else a
        val maxX = (screenW - w).toFloat()
        val mx = ((a.x + b.x) / 2f).coerceIn(gap / 2f, maxOf(gap / 2f, maxX - gap / 2f))
        val my = ((a.y + b.y) / 2f).coerceIn(0f, (screenH - a.lp.height).toFloat())
        left.tx = (mx - gap / 2f).coerceIn(0f, maxX); right.tx = (mx + gap / 2f).coerceIn(0f, maxX)
        left.ty = my; right.ty = my
        for (sp in listOf(a, b)) { sp.encMove = true; sp.encAct = if (speed > 1.3f) Act.RUN else Act.WALK; sp.encSpeed = speed }
        e.stage = 0
    }

    private fun fleeTarget(runner: Sprite, chaser: Sprite) {
        val maxX = (screenW - runner.lp.width).toFloat()
        val maxY = (screenH - runner.lp.height).toFloat()
        runner.tx = if (chaser.x < screenW / 2f) maxX * (0.7f + rnd.nextFloat() * 0.3f) else maxX * rnd.nextFloat() * 0.3f
        runner.ty = maxY * (0.15f + rnd.nextFloat() * 0.75f)
    }

    private fun faceEachOther(e: Encounter) {
        e.a.view.pose.facing = if (e.b.x >= e.a.x) 1f else -1f
        e.b.view.pose.facing = -e.a.view.pose.facing
    }

    private fun endEncounter(e: Encounter) {
        encounters.remove(e)
        for (sp in listOf(e.a, e.b)) {
            if (sp.enc !== e) continue
            sp.enc = null; sp.encAct = null; sp.encMove = false; sp.encSpeed = 1f; sp.hp = 100f
            sp.tx = sp.x; sp.ty = sp.y
            if (!sp.dragged) sp.brain.force(Act.IDLE, 1.5f)
        }
    }

    private fun stepEncounter(e: Encounter, dt: Float) {
        val a = e.a; val b = e.b
        if (a.enc !== e || b.enc !== e) { endEncounter(e); return }
        val ma = Squad.get(a.id); val mb = Squad.get(b.id)
        if (ma == null || mb == null) { endEncounter(e); return }
        e.t += dt
        if (e.stage == 0) {
            var both = true
            for (sp in listOf(a, b)) {
                if (hypot(sp.tx - sp.x, sp.ty - sp.y) < 4f) { sp.encMove = false; sp.encAct = Act.IDLE } else both = false
            }
            if (e.t > 10f) { endEncounter(e); return }
            if (!both) return
            faceEachOther(e)
            e.stage = 1; e.t = 0f; e.beat = 0f
            when (e.kind) {
                Kind.FIGHT -> { a.encAct = Act.GUARD; b.encAct = Act.GUARD; say(b, mb, Say.TAUNT) }
                Kind.TRAIN -> { a.encAct = Act.PUSHUP; b.encAct = Act.PUSHUP; say(a, ma, Say.TRAIN) }
                Kind.HIGHFIVE -> { a.encAct = Act.HIGHFIVE; b.encAct = Act.HIGHFIVE }
                else -> {}
            }
            return
        }
        when (e.kind) {
            Kind.FIGHT -> stepFight(e, ma, mb, dt)
            Kind.TALK -> {
                e.beat -= dt
                if (e.beat <= 0f) {
                    if (e.n >= 4) { Squad.bond(a.id, b.id, 4); endEncounter(e); return }
                    val sp = if (e.n % 2 == 0) a else b
                    val other = if (sp === a) b else a
                    val what = when (e.n) { 0 -> Say.FRIEND; 2 -> Say.RANDOM; else -> Say.REPLY }
                    say(sp, if (sp === a) ma else mb, what)
                    sp.encAct = Act.TALK; other.encAct = Act.IDLE
                    e.n++; e.beat = 2.7f
                }
            }
            Kind.HIGHFIVE -> {
                if (e.n == 0 && e.t > 0.35f) {
                    e.n = 1
                    a.view.pose.fx = "CLAP!"; a.view.pose.fxAge = 0f
                    say(a, ma, Say.HIGHFIVE)
                }
                if (e.t > 1.2f) { a.encAct = Act.VICTORY; b.encAct = Act.VICTORY }
                if (e.t > 2.4f) { Squad.bond(a.id, b.id, 6); endEncounter(e) }
            }
            Kind.TRAIN -> {
                if (e.t > 6f) { a.encAct = Act.FLEX; b.encAct = Act.FLEX }
                if (e.t > 6.1f && e.n == 0) { e.n = 1; say(b, mb, Say.REPLY) }
                if (e.t > 8.2f) { Squad.bond(a.id, b.id, 4, "Séance à deux avec ${mb.name}", "Séance à deux avec ${ma.name}"); endEncounter(e) }
            }
            Kind.CHASE -> {
                if (e.stage == 1) {
                    a.tx = b.x; a.ty = b.y
                    if (hypot(b.tx - b.x, b.ty - b.y) < 6f) fleeTarget(b, a)
                    if (e.n == 0 && e.t > 1.2f) { e.n = 1; say(b, mb, Say.FLEE) }
                    if (hypot(a.x - b.x, a.y - b.y) < a.lp.width * 0.5f) {
                        e.stage = 2; e.t = 0f
                        a.encMove = false; b.encMove = false
                        faceEachOther(e)
                        a.encAct = Act.VICTORY; b.encAct = Act.HIT
                        b.view.pose.fx = "TAG!"; b.view.pose.fxAge = 0f
                        say(a, ma, Say.CAUGHT)
                    } else if (e.t > 9f) {
                        a.encMove = false; b.encMove = false
                        a.encAct = Act.YAWN; b.encAct = Act.VICTORY
                        say(b, mb, Say.FLEE)
                        e.stage = 2; e.t = 0f
                    }
                } else if (e.t > 2f) { Squad.bond(a.id, b.id, 3); endEncounter(e) }
            }
        }
    }

    private val hits = listOf("POW!", "BAM!", "PAF!", "BOUM!", "CLAC!", "VLAN!", "SBAM!")

    private fun stepFight(e: Encounter, ma: Mascot, mb: Mascot, dt: Float) {
        val a = e.a; val b = e.b
        when (e.stage) {
            1 -> if (e.t > 1.1f) { e.stage = 2; e.t = 0f; e.beat = 0f }
            2 -> {
                e.beat -= dt
                if (e.beat < 0.18f) for (sp in listOf(a, b)) if (sp.encAct == Act.PUNCH || sp.encAct == Act.HIT) sp.encAct = Act.GUARD
                if (e.beat <= 0f) {
                    e.beat = 0.42f + rnd.nextFloat() * 0.15f
                    faceEachOther(e)
                    val pa = power(ma); val pb = power(mb)
                    val att = if (rnd.nextFloat() < pa / (pa + pb)) a else b
                    val def = if (att === a) b else a
                    val pAtt = if (att === a) pa else pb
                    val pDef = if (att === a) pb else pa
                    att.encAct = Act.PUNCH; att.punchT = 0.2f
                    val dir = if (def.x >= att.x) 1f else -1f
                    val maxX = (screenW - def.lp.width).toFloat()
                    if (rnd.nextFloat() < 0.22f * (pDef / pAtt)) {
                        def.encAct = Act.GUARD
                        def.x = (def.x + dir * 3 * density).coerceIn(0f, maxX)
                    } else {
                        def.hp -= (8f + rnd.nextFloat() * 10f) * (pAtt / pDef).coerceIn(0.6f, 1.6f)
                        def.encAct = Act.HIT
                        def.view.pose.fx = hits.random(); def.view.pose.fxAge = 0f
                        def.x = (def.x + dir * 6 * density).coerceIn(0f, maxX)
                        att.x = (att.x + dir * 4 * density).coerceIn(0f, maxX)
                        if (def.bubble == null && rnd.nextFloat() < 0.18f) say(def, if (def === a) ma else mb, Say.OUCH)
                    }
                    if (a.hp <= 0f || b.hp <= 0f || e.t > 10f) {
                        val win = if (a.hp >= b.hp) a else b
                        val lose = if (win === a) b else a
                        e.winner = win; e.loser = lose
                        e.stage = 3; e.t = 0f
                        win.encAct = Act.VICTORY; lose.encAct = Act.KO
                        lose.view.pose.fx = "K.O."; lose.view.pose.fxAge = 0f
                        say(win, if (win === a) ma else mb, Say.WIN)
                        Squad.recordFight(win.id, lose.id)
                        if (Squad.state.value.settings.vibrate) lose.view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
                    }
                }
            }
            3 -> {
                val win = e.winner ?: return endEncounter(e)
                val lose = e.loser ?: return endEncounter(e)
                if (e.t > 2.6f) win.encAct = Act.FLEX
                if (e.t > 3.0f && e.n == 0) { e.n = 1; say(lose, if (lose === a) ma else mb, Say.LOSE) }
                if (e.t > 5.5f) endEncounter(e)
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
        sp.enc?.let { e ->
            val other = if (e.a === sp) e.b else e.a
            endEncounter(e)
            Squad.get(other.id)?.let { om -> say(other, om, Say.REPLY) }
        }
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
        saveOne(sp)
        // Lâché sur un autre gars ? Ça part en baston.
        val victim = sprites.values.filter { it !== sp && !it.dragged && it.enc == null }
            .minByOrNull { hypot(it.cx - sp.cx, it.cy - sp.cy) }
            ?.takeIf { hypot(it.cx - sp.cx, it.cy - sp.cy) < sp.lp.width * 0.75f }
        if (victim != null) {
            val vm = Squad.get(victim.id)
            if (vm != null) {
                if (Squad.state.value.settings.fights) {
                    say(victim, vm, Say.BUMPED)
                    startEncounter(Kind.FIGHT, victim, sp)
                } else startEncounter(Kind.HIGHFIVE, victim, sp)
                return
            }
        }
        if (sp.brain.act != Act.SLEEP) { sp.brain.force(Act.IDLE, 2f); say(sp, m, Say.DROP) }
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
