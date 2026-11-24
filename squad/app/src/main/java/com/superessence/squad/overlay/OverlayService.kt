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
import com.superessence.squad.model.PropType
import com.superessence.squad.model.Say
import com.superessence.squad.render.Pose
import com.superessence.squad.ui.MainActivity
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlin.math.hypot
import kotlin.math.pow
import kotlin.math.sin
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
        const val ACTION_GYM = "com.superessence.squad.GYM"
        const val ACTION_PARTY = "com.superessence.squad.PARTY"
        const val ACTION_STREET = "com.superessence.squad.STREET"
        const val ACTION_CLEAN = "com.superessence.squad.CLEAN"
        const val ACTION_PICK = "com.superessence.squad.PICK"

        /** Affiche le viseur pour choisir où le Cliqueur doit cliquer. */
        fun pickPoint(ctx: Context, id: String) {
            if (!canRun(ctx)) return
            ContextCompat.startForegroundService(ctx, Intent(ctx, OverlayService::class.java).setAction(ACTION_PICK).putExtra(EXTRA_A, id))
        }

        /** Lance tout de suite : séance muscu (« gym »), street workout (« street ») ou fête (« party »). */
        fun happening(ctx: Context, kind: String) {
            if (!canRun(ctx)) return
            val a = when (kind) { "party" -> ACTION_PARTY; "street" -> ACTION_STREET; else -> ACTION_GYM }
            ContextCompat.startForegroundService(ctx, Intent(ctx, OverlayService::class.java).setAction(a))
        }

        fun clean(ctx: Context) {
            if (!canRun(ctx)) return
            ContextCompat.startForegroundService(ctx, Intent(ctx, OverlayService::class.java).setAction(ACTION_CLEAN))
        }
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
        var parked = false
        var stainT = 5f
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
        registerReceiver(batteryReceiver, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        scope.launch { Squad.state.collectLatest { sync(it) } }
        scope.launch {
            Squad.pings.collect { (k, id) ->
                val sp = sprites[id] ?: return@collect
                if (k == "task") {
                    work.remove(id); sp.encAct = null; sp.encMove = false
                    Squad.get(id)?.let { if (it.task.kind != com.superessence.squad.model.TaskKind.NONE) say(sp, it, Say.TASK_START) }
                    return@collect
                }
                if (sp.enc != null || inEvent(sp) || sp === cleaner || sp.dragged) return@collect
                sp.brain.force(if (k == "eat") Act.EAT else Act.TRAIN, 7f)
                sp.stainT = 1.5f
            }
        }
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
            ACTION_GYM, ACTION_PARTY, ACTION_STREET -> {
                Squad.edit { s -> s.copy(mascots = s.mascots.map { it.copy(onScreen = true) }) }
                requestHappening(when (intent.action) { ACTION_PARTY -> EvKind.PARTY; ACTION_STREET -> EvKind.STREET; else -> EvKind.GYM }, 0)
            }
            ACTION_CLEAN -> android.os.Handler(mainLooper).postDelayed({ startClean(manual = true) }, 300)
            ACTION_PICK -> intent.getStringExtra(EXTRA_A)?.let { id -> android.os.Handler(mainLooper).postDelayed({ showPicker(id) }, 500) }
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
        happening?.let { e -> e.stations.forEach { removeStation(it) }; e.crew.forEach { unpark(it) } }
        stains.toList().forEach { removeStain(it) }
        mess.toList().forEach { removeClutter(it) }
        runCatching { unregisterReceiver(screenReceiver) }
        runCatching { unregisterReceiver(batteryReceiver) }
        hidePicker()
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
        happening?.let { e -> e.stations.forEach { if (it.occupant === sp) it.occupant = null; if (it.reserved === sp) it.reserved = null } }
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
                val working = isWorking(sp, m, phase)
                val busy = sp.enc != null || inEvent(sp) || sp === cleaner || working
                if (working) workStep(sp, m, phase, dt)
                else ws(sp).let { w -> if (w.active) { w.active = false; w.tapRun?.let { setTouchable(sp, true) }; w.tapRun = null; if (sp.enc == null && !inEvent(sp) && sp !== cleaner) { sp.encAct = null; sp.encMove = false } } }
                if (!busy) {
                    val line = sp.brain.step(dt, m, phase, st.settings.chatter, arrived)
                    if (line != null) say(sp, m, line)
                    if (sp.brain.wantsTarget) {
                        sp.brain.wantsTarget = false
                        sp.tx = rnd.nextFloat() * (screenW - sp.lp.width)
                        sp.ty = (sp.y + (rnd.nextFloat() - 0.5f) * screenH * 0.6f).coerceIn(0f, (screenH - sp.lp.height).toFloat())
                    }
                }
                val moving = if (busy) sp.encMove else sp.brain.act == Act.WALK
                if (moving) {
                    val dx = sp.tx - sp.x; val dy = sp.ty - sp.y
                    val d = hypot(dx, dy)
                    if (d > 1f) {
                        val v = speed * (0.75f + 0.3f * m.archetype.energy) * (if (m.energy < 25) 0.6f else 1f) * (if (busy) sp.encSpeed else 1f)
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
            pose.sign = if (pose.act == Act.SIGN) signFor(sp, m) else null
            // ils mangent salement
            if (pose.act == Act.EAT && st.settings.mess && !sp.parked && !sp.dragged) {
                sp.stainT -= dt
                if (sp.stainT <= 0f) { sp.stainT = 6f + rnd.nextFloat() * 9f; spawnStain(sp, m) }
            }
            if (pose.act == Act.CHEERS && st.settings.mess && rnd.nextFloat() < dt * 0.02f) spawnStain(sp, m, com.superessence.squad.model.Stain.SODA)
            if (pose.act == Act.SLEEP && sp.bubble == null && rnd.nextFloat() < dt * 0.012f * st.settings.chatter) say(sp, m, Say.SNORE)
            if (sp.parked) {
                if (sp.bubble != null) { sp.bubbleLeft -= dt; if (sp.bubbleLeft <= 0f) hideBubble(sp) else placeBubble(sp) }
                continue
            }
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
        happenings(dt, phase, st.settings)
        cleaning(dt)
        guardZones(dt)
        workChecks(dt)
        for (c in mess) if (c.view.spin != c.spinTo) { c.view.spin += (c.spinTo - c.view.spin) * minOf(1f, dt * 8f); c.view.invalidate() }
        encounters(dt, phase, st.settings)
    }

    // ------------------------------------------------------------------ séances de muscu et fêtes

    private enum class EvKind { GYM, PARTY, STREET }
    private enum class Job { NONE, FETCH, CARRY, GO_USE, CHALKING, USE, SPOT, SPOT_PR, FREESTYLE, HANG, GO_PACK, CARRY_BACK, DONE }

    /** Tentative de record. */
    private inner class Pr(val lifter: Sprite, val spotter: Sprite?, val lift: String, val kg: Float, val ok: Boolean) {
        var t = 0f
        var step = 0
        var done = false
    }

    private inner class Station(val type: PropType) {
        var x = 0f; var y = 0f
        var view: PropView? = null
        var lp: WindowManager.LayoutParams? = null
        var occupant: Sprite? = null
        var reserved: Sprite? = null
        var pr: Pr? = null
        var prLabel: String? = null
        var placed = false
        var removed = false
        val w get() = (type.w * density * Squad.state.value.settings.size).toInt()
        val h get() = (type.h * density * Squad.state.value.settings.size).toInt()
    }

    private inner class Happening(val kind: EvKind, val crew: MutableList<Sprite>, val stations: MutableList<Station>) {
        var stage = 0
        var t = 0f
        val job = HashMap<Sprite, Job>()
        val target = HashMap<Sprite, Station>()
        val timer = HashMap<Sprite, Float>()
        val queue = HashMap<Sprite, ArrayDeque<Station>>()
        val used = HashMap<Sprite, MutableSet<PropType>>()
        val freeAct = HashMap<Sprite, Act>()
        var duration = 100f
    }

    private var happening: Happening? = null
    private var happeningCheck = 20f
    private var lastGymDay = -1
    private var lastPartyDay = -1

    private fun today() = java.util.Calendar.getInstance().get(java.util.Calendar.DAY_OF_YEAR)

    private fun inEvent(sp: Sprite) = happening?.crew?.contains(sp) == true

    /** Vérifie de temps en temps s'il est l'heure d'une séance ou d'une fête. */
    private fun happenings(dt: Float, phase: Phase, set: com.superessence.squad.model.Settings) {
        happening?.let { stepHappening(it, dt, phase); return }
        happeningCheck -= dt
        if (happeningCheck > 0f) return
        happeningCheck = 30f
        if (cleaner == null && stains.size + mess.size >= 10 && phase != Phase.SLEEP && rnd.nextFloat() < 0.3f) startClean(manual = false)
        if (phase == Phase.SLEEP || phase.isMeal) return
        val cal = java.util.Calendar.getInstance()
        val day = cal.get(java.util.Calendar.DAY_OF_YEAR)
        val dow = cal.get(java.util.Calendar.DAY_OF_WEEK)
        val minute = cal.get(java.util.Calendar.HOUR_OF_DAY) * 60 + cal.get(java.util.Calendar.MINUTE)
        val sch = Squad.state.value.schedule
        val sporty = gymCrew()
        if (sporty.size >= 2 && (set.gym || set.street)) {
            val due = (phase == Phase.WORKOUT && lastGymDay != day) || (phase == Phase.FREE && rnd.nextFloat() < 0.025f)
            if (due) {
                val street = set.street && (!set.gym || rnd.nextFloat() < 0.35f)
                if (street) startStreet(sporty) else startGym(sporty)
                return
            }
        }
        if (set.parties) {
            val crew = sprites.values.filter { freeForEvent(it) }
            val weekendNight = (dow == java.util.Calendar.FRIDAY || dow == java.util.Calendar.SATURDAY) &&
                minute >= sch.dinner + 40 && minute < sch.bed - 40
            val evening = minute >= 18 * 60
            if (crew.size >= 2 && phase == Phase.FREE && ((weekendNight && lastPartyDay != day) || (evening && rnd.nextFloat() < 0.008f))) {
                startParty(crew)
            }
        }
    }

    private fun freeForEvent(sp: Sprite) = !sp.dragged && sp.enc == null && sp.brain.act != Act.SLEEP && !sp.parked && sp !== cleaner && !postedTask(sp) &&
        Squad.get(sp.id)?.task?.kind != com.superessence.squad.model.TaskKind.CLEANER

    private fun gymCrew(): List<Sprite> {
        val free = sprites.values.filter { freeForEvent(it) }
        return free.filter { s -> Squad.get(s.id)?.let { com.superessence.squad.model.Events.sporty(it) } == true }
    }

    private fun requestHappening(kind: EvKind, tries: Int) {
        if (happening != null) return
        val free = sprites.values.filter { freeForEvent(it) || it.enc != null }
        val need = 2
        if (free.size < need) {
            if (tries < 12) android.os.Handler(mainLooper).postDelayed({ requestHappening(kind, tries + 1) }, 350)
            return
        }
        free.forEach { sp -> sp.enc?.let { endEncounter(it) } }
        val sporty = gymCrew().let { if (it.size < 2) free else it }
        when (kind) {
            EvKind.GYM -> startGym(sporty)
            EvKind.STREET -> startStreet(sporty)
            EvKind.PARTY -> startParty(free)
        }
    }

    private fun stationPos(st: Station, i: Int, n: Int) {
        val cols = maxOf(1, minOf(n, (screenW / (st.w + 12 * density)).toInt()))
        val row = i / cols; val col = i % cols
        val inRow = minOf(cols, n - row * cols)
        val slot = screenW / inRow.toFloat()
        st.x = (slot * col + slot / 2f - st.w / 2f).coerceIn(0f, (screenW - st.w).toFloat())
        st.y = (screenH * 0.36f + row * 165 * density * Squad.state.value.settings.size).coerceIn(0f, (screenH - st.h).toFloat())
    }

    private fun startGym(people: List<Sprite>) {
        lastGymDay = today()
        val crew = people.shuffled().take(5).toMutableList()
        val pool = PropType.entries.filter { it.gym && !it.street }.shuffled()
        // le banc ou le rack presque toujours là, pour les PR
        val must = listOf(PropType.BENCH, PropType.RACK).shuffled().take(if (rnd.nextFloat() < 0.5f) 2 else 1)
        val types = (must + pool.filter { it !in must } + listOf(PropType.PULLUP_BAR)).take((crew.size + 1).coerceIn(3, 5)).shuffled()
        val stations = types.map { Station(it) }.toMutableList()
        stations.forEachIndexed { i, st -> stationPos(st, i, stations.size) }
        val e = Happening(EvKind.GYM, crew, stations).apply { duration = 95f + rnd.nextFloat() * 30f }
        happening = e
        crew.forEach { e.queue[it] = ArrayDeque(); e.used[it] = mutableSetOf(); it.brain.clearForce(); it.encSpeed = 1.15f }
        stations.forEachIndexed { i, st -> e.queue[crew[i % crew.size]]!!.addLast(st) }
        crew.forEach { nextCarry(e, it) }
        Squad.get(crew[0].id)?.let { say(crew[0], it, Say.GYM_START) }
    }

    private fun startStreet(people: List<Sprite>) {
        lastGymDay = today()
        val crew = people.shuffled().take(5).toMutableList()
        val types = listOf(PropType.PULLUP_BAR, PropType.DIP_BARS) + (if (crew.size >= 4) listOf(PropType.PULLUP_BAR) else emptyList())
        val stations = types.map { Station(it) }.toMutableList()
        stations.forEachIndexed { i, st -> stationPos(st, i, stations.size) }
        val e = Happening(EvKind.STREET, crew, stations).apply { duration = 90f + rnd.nextFloat() * 30f }
        happening = e
        crew.forEach { e.queue[it] = ArrayDeque(); e.used[it] = mutableSetOf(); it.brain.clearForce(); it.encSpeed = 1.15f }
        stations.forEachIndexed { i, st -> e.queue[crew[i % crew.size]]!!.addLast(st) }
        crew.forEach { nextCarry(e, it) }
        Squad.get(crew[0].id)?.let { say(crew[0], it, Say.STREET_START) }
    }

    private fun startParty(people: List<Sprite>) {
        lastPartyDay = today()
        val crew = people.take(8).toMutableList()
        val speaker = Station(PropType.SPEAKER).apply { x = screenW * 0.06f; y = screenH * 0.5f }
        val table = Station(PropType.TABLE).apply { x = screenW * 0.55f; y = screenH * 0.58f }
        val disco = Station(PropType.DISCO).apply { x = screenW / 2f - w / 2f; y = 30 * density }
        listOf(speaker, table).forEach { it.x = it.x.coerceIn(0f, (screenW - it.w).toFloat()); it.y = it.y.coerceIn(0f, (screenH - it.h).toFloat()) }
        val e = Happening(EvKind.PARTY, crew, mutableListOf(speaker, table, disco)).apply { duration = 100f + rnd.nextFloat() * 40f }
        happening = e
        crew.forEach { e.queue[it] = ArrayDeque(); e.used[it] = mutableSetOf(); it.brain.clearForce(); it.encSpeed = 1.15f }
        e.queue[crew[0]]!!.addLast(speaker)
        e.queue[crew[1 % crew.size]]!!.addLast(table)
        placeStation(e, disco) // la boule descend du plafond
        crew.forEach { nextCarry(e, it) }
        Squad.get(crew[0].id)?.let { say(crew[0], it, Say.PARTY) }
    }

    private fun standPoint(sp: Sprite, st: Station): Pair<Float, Float> {
        val x = (st.x + st.w / 2f - sp.lp.width / 2f).coerceIn(0f, (screenW - sp.lp.width).toFloat())
        val y = (st.y + st.h - sp.lp.height).coerceIn(0f, (screenH - sp.lp.height).toFloat())
        return x to y
    }

    private fun goTo(sp: Sprite, x: Float, y: Float, act: Act = Act.WALK) {
        sp.tx = x.coerceIn(0f, (screenW - sp.lp.width).toFloat()); sp.ty = y.coerceIn(0f, (screenH - sp.lp.height).toFloat())
        sp.encMove = true; sp.encAct = act
    }

    private fun arrived(sp: Sprite) = hypot(sp.tx - sp.x, sp.ty - sp.y) < 4f

    /** Va chercher le prochain matériel sur le bord de l'écran. */
    private fun nextCarry(e: Happening, sp: Sprite) {
        val st = e.queue[sp]?.removeFirstOrNull()
        if (st == null) { e.job[sp] = Job.NONE; sp.encMove = false; sp.encAct = Act.IDLE; return }
        e.target[sp] = st
        e.job[sp] = Job.FETCH
        val edgeX = if (st.x + st.w / 2f < screenW / 2f) 0f else (screenW - sp.lp.width).toFloat()
        goTo(sp, edgeX, standPoint(sp, st).second)
    }

    private fun placeStation(e: Happening, st: Station) {
        if (st.placed) return
        val v = PropView(this, st.type)
        v.listener = propListener
        val lp = WindowManager.LayoutParams(
            st.w, st.h, overlayType(),
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply { gravity = Gravity.TOP or Gravity.START; x = st.x.toInt(); y = st.y.toInt() }
        if (st.type == PropType.DISCO) { v.appear = 1f; lp.y = -st.h }
        runCatching { wm.addView(v, lp); st.view = v; st.lp = lp; st.placed = true }
        raiseSprites()
    }

    private fun removeStation(st: Station) {
        st.view?.let { runCatching { wm.removeView(it) } }
        st.view = null; st.lp = null; st.removed = true
    }

    /** Remet les mascottes au premier plan, devant le matériel. */
    private fun raiseSprites() {
        for (sp in sprites.values) {
            if (sp.parked || sp.dragged) continue
            runCatching { wm.removeView(sp.view); wm.addView(sp.view, sp.lp) }
        }
    }

    private fun park(sp: Sprite) {
        if (sp.parked) return
        runCatching { wm.removeView(sp.view) }
        sp.parked = true
    }

    private fun unpark(sp: Sprite) {
        if (!sp.parked) return
        sp.parked = false
        runCatching { wm.addView(sp.view, sp.lp) }
    }

    private val propListener = object : PropView.Listener {
        override fun onPropDrag(v: PropView, dx: Float, dy: Float) {
            val st = happening?.stations?.firstOrNull { it.view === v } ?: return
            st.x = (st.x + dx).coerceIn(0f, (screenW - st.w).toFloat())
            st.y = (st.y + dy).coerceIn(if (st.type == PropType.DISCO) 0f else 0f, (screenH - st.h).toFloat())
            st.lp?.let { it.x = st.x.toInt(); it.y = st.y.toInt(); runCatching { wm.updateViewLayout(v, it) } }
            st.occupant?.let { o -> val (x, y) = standPoint(o, st); o.x = x; o.y = y; o.tx = x; o.ty = y }
        }

        override fun onPropTap(v: PropView) {
            val e = happening ?: return
            val st = e.stations.firstOrNull { it.view === v } ?: return
            if (Squad.state.value.settings.vibrate) v.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
            st.occupant?.let { o -> Squad.get(o.id)?.let { say(o, it, Say.TRAIN) }; return }
            if (st.type == PropType.SPEAKER || st.type == PropType.DISCO) {
                // on monte le son : tout le monde danse
                e.crew.filter { e.job[it] == Job.HANG }.forEach { it.encAct = Act.DANCE; e.timer[it] = 6f }
                e.crew.firstOrNull()?.let { sp -> Squad.get(sp.id)?.let { say(sp, it, Say.DANCE) } }
            }
        }
    }

    private fun endHappening(e: Happening, reward: Boolean) {
        e.stations.forEach { removeStation(it) }
        for (sp in e.crew) {
            unpark(sp)
            sp.view.pose.carry = null
            sp.encAct = null; sp.encMove = false; sp.encSpeed = 1f
            sp.tx = sp.x; sp.ty = sp.y
            if (!sp.dragged) sp.brain.force(if (e.kind != EvKind.PARTY) Act.FLEX else Act.YAWN, 2.5f)
        }
        if (reward) {
            val ids = e.crew.map { it.id }
            if (e.kind != EvKind.PARTY) Squad.recordGym(ids, e.stations.map { it.type.label.substringAfter(' ') }.distinct())
            else Squad.recordParty(ids)
        }
        happening = null
        happeningCheck = 120f
    }

    private fun stepHappening(e: Happening, dt: Float, phase: Phase) {
        e.t += dt
        // gars disparus (rangés, supprimés)
        e.crew.removeAll { sprites[it.id] !== it }
        if (e.crew.isEmpty() || phase == Phase.SLEEP) { endHappening(e, false); return }
        // animation du matériel
        for (st in e.stations) {
            val v = st.view ?: continue
            v.t = t
            if (v.appear < 1f) v.appear = (v.appear + dt * 3f).coerceAtMost(1f)
            val lp = st.lp ?: continue
            if (st.type == PropType.DISCO) {
                val goal = if (e.stage == 2) -st.h.toFloat() else st.y
                val ny = lp.y + (goal - lp.y) * minOf(1f, dt * 1.5f)
                if (ny.toInt() != lp.y) { lp.y = ny.toInt(); runCatching { wm.updateViewLayout(v, lp) } }
            }
            v.occupant = st.occupant?.takeIf { it.parked }?.let { o ->
                Squad.get(o.id)?.let { m ->
                    val pr = st.pr
                    if (pr != null && !pr.done) {
                        val tt = pr.t
                        val g = when {
                            tt < 1.2f -> 1f - tt / 1.2f
                            pr.ok -> ((tt - 1.2f) / 4.3f).coerceIn(0f, 1f).pow(1.6f)
                            tt < 5.5f -> minOf(0.42f, (tt - 1.2f) / 4.3f * 0.6f)
                            else -> (0.42f + (tt - 5.5f) / 1.5f * 0.58f).coerceAtMost(1f)
                        }
                        com.superessence.squad.render.Occupant(m.look, m.fitness, o.view.pose.seed, g, if (tt < 1.2f) 0.3f else 1f, "${pr.lift.uppercase()} ${Squad.fmtKg(pr.kg)}")
                    } else com.superessence.squad.render.Occupant(m.look, m.fitness, o.view.pose.seed, label = st.prLabel)
                }
            }
            v.invalidate()
        }
        // fin de la séance : on range
        if (e.stage == 1 && e.t > e.duration) {
            e.stage = 2
            Squad.get(e.crew[0].id)?.let { say(e.crew[0], it, if (e.kind == EvKind.PARTY) Say.PARTY_END else Say.GYM_END) }
            e.stations.forEach { st -> st.pr?.done = true; st.occupant?.let { o -> leaveStation(e, o) }; st.reserved = null }
            val movable = e.stations.filter { it.type != PropType.DISCO && it.placed && !it.removed }
            e.crew.forEach { e.queue[it]!!.clear() }
            movable.forEachIndexed { i, st -> e.queue[e.crew[i % e.crew.size]]!!.addLast(st) }
            e.crew.forEach { packNext(e, it) }
        }
        for (sp in e.crew.toList()) {
            if (sp.dragged) continue
            val m = Squad.get(sp.id) ?: continue
            when (e.job[sp] ?: Job.NONE) {
                Job.FETCH -> if (arrived(sp)) {
                    val st = e.target[sp]!!
                    e.job[sp] = Job.CARRY
                    sp.view.pose.carry = st.type
                    val (x, y) = standPoint(sp, st)
                    goTo(sp, x, y, Act.CARRY)
                    if (rnd.nextFloat() < 0.5f) say(sp, m, Say.CARRY)
                }
                Job.CARRY -> if (arrived(sp)) {
                    val st = e.target[sp]!!
                    sp.view.pose.carry = null
                    placeStation(e, st)
                    nextCarry(e, sp)
                }
                Job.NONE -> {
                    if (e.stage == 0) {
                        sp.encMove = false; sp.encAct = Act.IDLE
                        if (e.stations.all { it.placed }) {
                            e.stage = 1; e.t = 0f
                            e.crew.forEach { e.job[it] = Job.HANG; e.timer[it] = 0f }
                        }
                    }
                }
                Job.HANG -> {
                    e.timer[sp] = (e.timer[sp] ?: 0f) - dt
                    if (e.stage != 1) continue
                    if (e.kind == EvKind.PARTY) partyPick(e, sp, m) else gymPick(e, sp, m)
                }
                Job.GO_USE -> if (arrived(sp)) {
                    val st = e.target[sp]!!
                    if (st.occupant != null) { st.reserved = null; e.job[sp] = Job.HANG; e.timer[sp] = 0f; continue }
                    st.occupant = sp; st.reserved = null
                    e.used[sp]!!.add(st.type)
                    // tentative de PR avec un pareur ?
                    val spotter = if (e.kind == EvKind.GYM && st.type.pr != null && (st.type == PropType.BENCH || st.type == PropType.RACK) && rnd.nextFloat() < 0.5f)
                        e.crew.firstOrNull { it !== sp && !it.dragged && !it.parked && (e.job[it] == Job.HANG || e.job[it] == Job.SPOT) } else null
                    if (spotter != null) {
                        val lift = st.type.pr!!
                        val ok = rnd.nextFloat() < 0.55f + m.mood / 400f + m.energy / 500f
                        st.pr = Pr(sp, spotter, lift, Squad.prTarget(m, lift), ok)
                        e.job[spotter] = Job.SPOT_PR; e.target[spotter] = st
                        val (x, y) = spotPoint(spotter, st)
                        goTo(spotter, x, y)
                        say(sp, m, Say.PR_TRY)
                    }
                    sp.encMove = false; sp.encAct = Act.CHALK
                    e.job[sp] = Job.CHALKING
                    e.timer[sp] = if (st.pr != null || rnd.nextFloat() < 0.35f) 1.6f else 0f
                }
                Job.CHALKING -> {
                    e.timer[sp] = (e.timer[sp] ?: 0f) - dt
                    if ((e.timer[sp] ?: 0f) <= 0f) {
                        val st = e.target[sp]!!
                        if (st.occupant !== sp) { e.job[sp] = Job.HANG; e.timer[sp] = 0f; continue }
                        val (x, y) = standPoint(sp, st)
                        sp.x = x; sp.y = y; sp.tx = x; sp.ty = y
                        park(sp)
                        e.job[sp] = Job.USE; e.timer[sp] = if (st.pr != null) 13f else 14f + rnd.nextFloat() * 8f
                        if (st.pr == null && rnd.nextFloat() < 0.4f) say(sp, m, if (e.kind == EvKind.STREET) Say.STREET else Say.TRAIN)
                    }
                }
                Job.USE -> {
                    val st = e.stations.firstOrNull { it.occupant === sp }
                    st?.pr?.let { stepPr(e, st, it, dt) }
                    e.timer[sp] = (e.timer[sp] ?: 0f) - dt
                    if ((e.timer[sp] ?: 0f) <= 0f && (st?.pr == null || st.pr!!.done)) {
                        leaveStation(e, sp)
                        val r = rnd.nextFloat()
                        sp.encAct = when { r < 0.4f -> Act.DRINK; r < 0.65f -> Act.FLEX; else -> Act.STRETCH }
                        e.timer[sp] = 2.6f
                        if (sp.encAct == Act.DRINK && rnd.nextFloat() < 0.35f) say(sp, m, Say.SHAKER)
                        if (sp.encAct == Act.DRINK && rnd.nextFloat() < 0.3f) addClutter(com.superessence.squad.model.Clutter.SHAKER, sp.cx + (rnd.nextFloat() - 0.5f) * 40 * density, sp.y + sp.lp.height - 30 * density)
                    }
                }
                Job.SPOT_PR -> {
                    val st = e.target[sp]
                    if (st == null || st.pr == null || st.pr!!.spotter !== sp || st.pr!!.done) { e.job[sp] = Job.HANG; e.timer[sp] = 1f; continue }
                    if (arrived(sp)) { sp.encMove = false; sp.encAct = Act.SPOTTING; sp.view.pose.facing = if (st.x + st.w / 2f > sp.cx) 1f else -1f }
                }
                Job.FREESTYLE -> {
                    e.timer[sp] = (e.timer[sp] ?: 0f) - dt
                    if (arrived(sp) && sp.encMove) { sp.encMove = false; sp.encAct = e.freeAct[sp] ?: Act.HANDSTAND }
                    if ((e.timer[sp] ?: 0f) <= 0f) { e.job[sp] = Job.HANG; e.timer[sp] = 0f; sp.encAct = Act.IDLE }
                }
                Job.SPOT -> {
                    e.timer[sp] = (e.timer[sp] ?: 0f) - dt
                    if (arrived(sp)) { sp.encMove = false; sp.encAct = if ((t * 0.7f).toInt() % 2 == 0) Act.TALK else Act.WAVE }
                    if ((e.timer[sp] ?: 0f) <= 0f) { e.job[sp] = Job.HANG; e.timer[sp] = 0f }
                }
                Job.GO_PACK -> if (arrived(sp)) {
                    val st = e.target[sp]!!
                    removeStation(st)
                    sp.view.pose.carry = st.type
                    e.job[sp] = Job.CARRY_BACK
                    val edgeX = if (st.x + st.w / 2f < screenW / 2f) 0f else (screenW - sp.lp.width).toFloat()
                    goTo(sp, edgeX, sp.y, Act.CARRY)
                }
                Job.CARRY_BACK -> if (arrived(sp)) { sp.view.pose.carry = null; packNext(e, sp) }
                Job.DONE -> {}
            }
        }
        if (e.stage == 2 && e.crew.all { e.job[it] == Job.DONE } && e.stations.none { it.type == PropType.DISCO && (it.lp?.y ?: -9999) > -it.h + 4 }) {
            Squad.get(e.crew[0].id)?.let { say(e.crew[0], it, if (e.kind == EvKind.PARTY) Say.PARTY_END else Say.GYM_END) }
            endHappening(e, true)
        }
        if (e.t > 400f) endHappening(e, false) // garde-fou
    }

    private fun packNext(e: Happening, sp: Sprite) {
        val st = e.queue[sp]?.removeFirstOrNull()
        if (st == null) { e.job[sp] = Job.DONE; sp.encMove = false; sp.encAct = if (e.kind != EvKind.PARTY) Act.FLEX else Act.YAWN; return }
        e.target[sp] = st; e.job[sp] = Job.GO_PACK
        val (x, y) = standPoint(sp, st)
        goTo(sp, x, y)
    }

    private fun spotPoint(sp: Sprite, st: Station): Pair<Float, Float> {
        val y = (st.y + st.h - sp.lp.height).coerceIn(0f, (screenH - sp.lp.height).toFloat())
        val x = if (st.type == PropType.BENCH) st.x - sp.lp.width * 0.3f else st.x + st.w - sp.lp.width * 0.55f
        return x.coerceIn(0f, (screenW - sp.lp.width).toFloat()) to y
    }

    private val shoutSet = setOf(Say.PR_SPOT, Say.PR_WIN, Say.PR_CHEER, Say.PR_SAVE, Say.BURP)

    /** Déroulé d'une tentative de record : le pareur crie, la barre monte… ou pas. */
    private fun stepPr(e: Happening, st: Station, pr: Pr, dt: Float) {
        if (pr.done) return
        pr.t += dt
        val ml = Squad.get(pr.lifter.id) ?: return
        val spot = pr.spotter
        val ms = spot?.let { Squad.get(it.id) }
        fun at(i: Int, time: Float, f: () -> Unit) { if (pr.step == i - 1 && pr.t >= time) { pr.step = i; f() } }
        at(1, 1.4f) { if (spot != null && ms != null) say(spot, ms, Say.PR_SPOT) }
        at(2, 3.5f) { if (spot != null && ms != null) say(spot, ms, Say.PR_SPOT) }
        if (pr.ok) at(3, 5.5f) {
            say(pr.lifter, ml, Say.PR_WIN)
            Squad.recordPr(pr.lifter.id, pr.lift, pr.kg, true)
            st.prLabel = "NEW PR ! ${Squad.fmtKg(pr.kg)}"
            if (Squad.state.value.settings.vibrate) st.view?.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
            var cheered = false
            for (o in e.crew) {
                if (o === pr.lifter || o.parked || o.dragged) continue
                val j = e.job[o]
                if (j == Job.HANG || j == Job.SPOT || j == Job.SPOT_PR || j == Job.FREESTYLE || j == Job.NONE) {
                    o.encMove = false; o.encAct = Act.VICTORY; e.job[o] = Job.HANG; e.timer[o] = 3f
                    if (!cheered) { Squad.get(o.id)?.let { say(o, it, Say.PR_CHEER) }; cheered = true }
                }
            }
            pr.done = true
            e.timer[pr.lifter] = 5f
        } else {
            at(3, 5.5f) { if (spot != null && ms != null) say(spot, ms, Say.PR_SAVE) }
            at(4, 7.2f) {
                say(pr.lifter, ml, Say.PR_FAIL)
                Squad.recordPr(pr.lifter.id, pr.lift, pr.kg, false)
                spot?.let { e.job[it] = Job.HANG; e.timer[it] = 1.5f; it.encAct = Act.IDLE }
                pr.done = true
                e.timer[pr.lifter] = 3f
            }
        }
    }

    private fun leaveStation(e: Happening, sp: Sprite) {
        val st = e.stations.firstOrNull { it.occupant === sp } ?: return
        st.occupant = null
        st.pr = null; st.prLabel = null
        // le bordel : ils laissent traîner du matos
        if (Squad.state.value.settings.mess && e.kind != EvKind.PARTY && rnd.nextFloat() < 0.45f) {
            val type = when (st.type) {
                PropType.DUMBBELLS -> listOf(com.superessence.squad.model.Clutter.DUMBBELL, com.superessence.squad.model.Clutter.DUMBBELL, com.superessence.squad.model.Clutter.KETTLEBELL).random()
                PropType.RACK, PropType.BENCH, PropType.LEG_PRESS -> listOf(com.superessence.squad.model.Clutter.PLATE, com.superessence.squad.model.Clutter.PLATE, com.superessence.squad.model.Clutter.BAR).random()
                PropType.PULLEY, PropType.PEC_DECK -> com.superessence.squad.model.Clutter.TOWEL
                else -> listOf(com.superessence.squad.model.Clutter.TOWEL, com.superessence.squad.model.Clutter.SHAKER, com.superessence.squad.model.Clutter.KETTLEBELL).random()
            }
            addClutter(type, st.x + rnd.nextFloat() * st.w, st.y + st.h - rnd.nextFloat() * 24 * density)
        }
        val (x, y) = standPoint(sp, st)
        sp.x = (x + (if (x > screenW / 2f) -1 else 1) * sp.lp.width * 0.6f).coerceIn(0f, (screenW - sp.lp.width).toFloat())
        sp.y = y; sp.tx = sp.x; sp.ty = sp.y
        sp.lp.x = sp.x.toInt(); sp.lp.y = sp.y.toInt()
        sp.encMove = false
        unpark(sp)
        e.job[sp] = Job.HANG
    }

    private fun gymPick(e: Happening, sp: Sprite, m: Mascot) {
        if ((e.timer[sp] ?: 0f) > 0f) return
        val free = e.stations.filter { it.placed && !it.removed && it.occupant == null && it.reserved == null }
        val st = free.filter { it.type !in e.used[sp]!! }.randomOrNull() ?: free.randomOrNull()
        if (st != null) {
            st.reserved = sp
            e.target[sp] = st; e.job[sp] = Job.GO_USE
            val (x, y) = standPoint(sp, st)
            goTo(sp, x, y)
        } else if (e.kind == EvKind.STREET || (rnd.nextFloat() < 0.3f && m.fitness > 40f)) {
            // en attendant : figures au sol
            val st2 = e.stations.random()
            e.job[sp] = Job.FREESTYLE; e.timer[sp] = 7f + rnd.nextFloat() * 4f
            e.freeAct[sp] = listOf(Act.HANDSTAND, if (m.fitness > 45f) Act.ONE_ARM_HS else Act.HANDSTAND, Act.PUSHUP, Act.STRETCH, Act.CHALK).random()
            goTo(sp, st2.x + (rnd.nextFloat() - 0.5f) * st2.w * 1.6f, st2.y + st2.h - sp.lp.height + rnd.nextFloat() * 40 * density)
            if (rnd.nextFloat() < 0.4f) say(sp, m, Say.STREET)
        } else {
            // pas de poste libre : il coache un pote
            val busy = e.stations.filter { it.occupant != null }.randomOrNull() ?: return
            e.job[sp] = Job.SPOT; e.timer[sp] = 6f
            val (x, y) = standPoint(sp, busy)
            goTo(sp, x + (if (x > screenW / 2f) -1 else 1) * busy.w * 0.55f, y)
            say(sp, m, Say.SPOT)
        }
    }

    private fun partyPick(e: Happening, sp: Sprite, m: Mascot) {
        if ((e.timer[sp] ?: 0f) > 0f) {
            if (arrived(sp) && sp.encMove) { sp.encMove = false; sp.encAct = sp.view.pose.carry?.let { Act.IDLE } ?: (e.target[sp]?.let { if (it.type == PropType.TABLE) Act.CHEERS else Act.DANCE } ?: Act.DANCE) }
            return
        }
        val speaker = e.stations.first { it.type == PropType.SPEAKER }
        val table = e.stations.first { it.type == PropType.TABLE }
        val r = rnd.nextFloat()
        val toTable = r < 0.28f || (m.arch == "marco" && r < 0.6f)
        val st = if (toTable) table else speaker
        e.target[sp] = st
        val zoneX = if (toTable) table.x + rnd.nextFloat() * table.w - sp.lp.width / 2f else screenW * (0.05f + rnd.nextFloat() * 0.9f) - sp.lp.width / 2f
        val zoneY = if (toTable) table.y + table.h - sp.lp.height + 8 * density else screenH * (0.42f + rnd.nextFloat() * 0.3f)
        goTo(sp, zoneX, zoneY, Act.WALK)
        e.timer[sp] = 7f + rnd.nextFloat() * 7f
        if (rnd.nextFloat() < 0.35f) say(sp, m, if (toTable) Say.CHEERS else Say.DANCE)
        // l'acte final est posé à l'arrivée (danse ou trinque)
    }

    // ------------------------------------------------------------------ rencontres entre mascottes

    private fun power(m: Mascot) = m.archetype.skill * (0.6f + m.fitness / 100f * 0.6f + m.level * 0.03f) * (0.7f + m.energy / 100f * 0.3f)

    private fun free(sp: Sprite) = !sp.dragged && sp.enc == null && sp.brain.act != Act.SLEEP && !inEvent(sp) && !sp.parked && sp !== cleaner && !postedTask(sp)

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

    // ------------------------------------------------------------------ métiers (tâches assignées)

    private inner class TapRun(val px: Float, val py: Float, val facing: Float) {
        var stage = 0
        var t = 0f
        var flagsOff = false
        var tapped = false
    }

    private inner class WorkState {
        var active = false
        var tapRun: TapRun? = null
        var tapAnim = 0f
        var fidgetT = 25f
        var fidgeting = false
        var lastHour = -1
        var lastWater = System.currentTimeMillis()
        var lastMove = System.currentTimeMillis()
        var lastTapAt = System.currentTimeMillis()
        val fired = HashSet<String>()
        var noteT = 400f
        var guardSayT = 0f
        var checkT = 0f
        var sign: com.superessence.squad.render.Sign? = null
        var signT = 0f
    }

    private val work = HashMap<String, WorkState>()
    private fun ws(sp: Sprite) = work.getOrPut(sp.id) { WorkState() }
    private val iconCache = HashMap<String, android.graphics.Bitmap?>()
    private var battPct = -1
    private var charging = false
    private var lastBattWarn = 101
    private var workCheckT = 0f
    private var picker: PickView? = null
    private var pickerLp: WindowManager.LayoutParams? = null
    private var pickerId: String? = null

    private val batteryReceiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context, i: Intent) {
            val lvl = i.getIntExtra(android.os.BatteryManager.EXTRA_LEVEL, -1)
            val sc = i.getIntExtra(android.os.BatteryManager.EXTRA_SCALE, 100)
            val st = i.getIntExtra(android.os.BatteryManager.EXTRA_STATUS, -1)
            val was = charging
            if (lvl >= 0) battPct = lvl * 100 / maxOf(1, sc)
            charging = st == android.os.BatteryManager.BATTERY_STATUS_CHARGING || st == android.os.BatteryManager.BATTERY_STATUS_FULL
            if (charging) lastBattWarn = 101
            for (sp in sprites.values) {
                val m = Squad.get(sp.id) ?: continue
                if (m.task.kind != com.superessence.squad.model.TaskKind.BATTERY || sp.brain.act == Act.SLEEP) continue
                if (charging && !was) say(sp, m, Say.BATT_CHARGE)
                else if (!charging && battPct in 0..20) {
                    val step = when { battPct <= 5 -> 5; battPct <= 10 -> 10; else -> 20 }
                    if (step < lastBattWarn) { say(sp, m, Say.BATT_LOW); if (Squad.state.value.settings.vibrate) sp.view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS) }
                }
            }
            if (!charging && battPct in 0..20) lastBattWarn = when { battPct <= 5 -> 5; battPct <= 10 -> 10; else -> 20 }
        }
    }

    private fun postedTask(sp: Sprite) = Squad.get(sp.id)?.task?.kind?.posted == true

    private fun isWorking(sp: Sprite, m: Mascot, phase: Phase) =
        m.task.kind.posted && phase != Phase.SLEEP && sp.enc == null && !inEvent(sp) && sp !== cleaner && !sp.parked

    private fun postOf(sp: Sprite, m: Mascot): Pair<Float, Float> {
        val maxX = (screenW - sp.lp.width).toFloat(); val maxY = (screenH - sp.lp.height).toFloat()
        if (m.task.postX < 0f || m.task.postY < 0f) {
            val fx = if (maxX > 0) sp.x / maxX else 0.5f; val fy = if (maxY > 0) sp.y / maxY else 0.5f
            Squad.editMascot(sp.id) { it.copy(task = it.task.copy(postX = fx, postY = fy)) }
            return sp.x to sp.y
        }
        return m.task.postX * maxX to m.task.postY * maxY
    }

    private fun appIcon(pkg: String): android.graphics.Bitmap? = iconCache.getOrPut(pkg) {
        runCatching { androidx.core.graphics.drawable.DrawableCompat.wrap(packageManager.getApplicationIcon(pkg)).let { d ->
            val b = android.graphics.Bitmap.createBitmap(96, 96, android.graphics.Bitmap.Config.ARGB_8888)
            val c = android.graphics.Canvas(b); d.setBounds(0, 0, 96, 96); d.draw(c); b
        } }.getOrNull()
    }

    private fun wrap(text: String, width: Int, max: Int): List<String> {
        val out = mutableListOf<String>(); var cur = ""
        for (w in text.split(' ', '\n').filter { it.isNotBlank() }) {
            if (cur.isEmpty()) cur = w else if (cur.length + 1 + w.length <= width) cur += " $w" else { out += cur; cur = w }
            if (out.size == max) break
        }
        if (cur.isNotEmpty() && out.size < max) out += cur
        return out.map { if (it.length > width + 2) it.take(width) + "…" else it }
    }

    private fun signFor(sp: Sprite, m: Mascot): com.superessence.squad.render.Sign? {
        val w = ws(sp)
        w.signT -= 1f
        if (w.sign != null && w.signT > 0f) return w.sign
        w.signT = 20f // ~1 s
        val task = m.task
        val S = com.superessence.squad.render.Sign::class
        w.sign = when (task.kind) {
            com.superessence.squad.model.TaskKind.LAUNCHER ->
                if (task.pkg.isEmpty()) com.superessence.squad.render.Sign(listOf("APPLI ?", "Touche-moi"))
                else com.superessence.squad.render.Sign(wrap(task.appLabel, 9, 2), appIcon(task.pkg))
            com.superessence.squad.model.TaskKind.CLOCK -> com.superessence.squad.render.Sign(listOf(java.text.SimpleDateFormat("HH:mm", java.util.Locale.FRANCE).format(java.util.Date())))
            com.superessence.squad.model.TaskKind.BATTERY -> com.superessence.squad.render.Sign(
                listOf(if (charging) "EN CHARGE" else "BATTERIE", if (battPct >= 0) "$battPct %" else "…"),
                accent = when { charging -> 0xFF5BC26A.toInt(); battPct in 0..20 -> 0xFFE53935.toInt(); battPct in 21..40 -> 0xFFE67E22.toInt(); else -> 0xFF5BC26A.toInt() },
            )
            com.superessence.squad.model.TaskKind.NOTE -> com.superessence.squad.render.Sign(if (task.note.isEmpty()) listOf("NOTE ?") else wrap(task.note, 13, 3), accent = 0xFF4AA3FF.toInt())
            else -> null
        }
        return w.sign
    }

    /** Fait travailler une mascotte à son poste. */
    private fun workStep(sp: Sprite, m: Mascot, phase: Phase, dt: Float) {
        val w = ws(sp)
        w.active = true
        val task = m.task
        val (px, py) = postOf(sp, m)
        // clic en cours
        w.tapRun?.let { stepTapRun(sp, m, w, it, dt, px, py); return }
        if (w.tapAnim > 0f) { w.tapAnim -= dt; sp.encMove = false; sp.encAct = Act.TAP; return }
        // petite bougeotte autour du poste
        w.fidgetT -= dt
        if (w.fidgetT <= 0f) {
            w.fidgetT = 25f + rnd.nextFloat() * 30f
            w.fidgeting = true
            goTo(sp, px + (rnd.nextFloat() - 0.5f) * 70 * density, py + (rnd.nextFloat() - 0.5f) * 30 * density)
            jobTalk(sp, m, idle = true)
        }
        if (!w.fidgeting && hypot(px - sp.x, py - sp.y) > 6f && !sp.encMove) goTo(sp, px, py)
        if (sp.encMove) {
            if (arrived(sp)) { sp.encMove = false; if (w.fidgeting) { w.fidgeting = false; goTo(sp, px, py) } }
            else { sp.encAct = Act.WALK; return }
        }
        sp.encAct = when (task.kind) {
            com.superessence.squad.model.TaskKind.LAUNCHER, com.superessence.squad.model.TaskKind.CLOCK,
            com.superessence.squad.model.TaskKind.BATTERY, com.superessence.squad.model.TaskKind.NOTE -> Act.SIGN
            com.superessence.squad.model.TaskKind.GUARD -> Act.GUARD
            com.superessence.squad.model.TaskKind.COACH -> if ((t / 6f).toInt() % 3 == 0) Act.STRETCH else Act.IDLE
            else -> Act.IDLE
        }
        // tâches périodiques (vérifiées toutes les 2 s)
        w.checkT -= dt
        if (w.checkT > 0f) return
        w.checkT = 2f
        val cal = java.util.Calendar.getInstance()
        val minute = cal.get(java.util.Calendar.HOUR_OF_DAY) * 60 + cal.get(java.util.Calendar.MINUTE)
        val doy = cal.get(java.util.Calendar.DAY_OF_YEAR)
        val now = System.currentTimeMillis()
        fun once(key: String, f: () -> Unit) { if (w.fired.add("$key-$doy")) f() }
        when (task.kind) {
            com.superessence.squad.model.TaskKind.CLOCK -> {
                val h = cal.get(java.util.Calendar.HOUR_OF_DAY)
                if (cal.get(java.util.Calendar.MINUTE) == 0 && w.lastHour != h) {
                    w.lastHour = h
                    showBubble(sp, m.name, "Il est $h h pile, ${m.archetype.nick} !", shout = true)
                    if (Squad.state.value.settings.vibrate) sp.view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                }
            }
            com.superessence.squad.model.TaskKind.COACH -> {
                val sch = Squad.state.value.schedule
                if (task.meals) for ((label, at) in listOf("petit-déj" to sch.breakfast, "déjeuner" to sch.lunch, "dîner" to sch.dinner))
                    if (minute == ((at - 10) % 1440 + 1440) % 1440) once("meal$at") { remind(sp, m, m.archetype.lines(Say.MEAL_SOON).random(), "$label à ${com.superessence.squad.model.fmtMin(at)}") }
                if (task.bedtime && minute == ((sch.bed - 15) % 1440 + 1440) % 1440) once("bed") { remind(sp, m, m.archetype.lines(Say.BED).random(), "Coucher à ${com.superessence.squad.model.fmtMin(sch.bed)}") }
                if (phase == Phase.FREE || phase == Phase.WORKOUT) {
                    if (task.water && now - w.lastWater > 75 * 60_000L) { w.lastWater = now; remind(sp, m, m.archetype.lines(Say.WATER).random(), null) }
                    else if (task.move && now - w.lastMove > 55 * 60_000L) { w.lastMove = now; remind(sp, m, m.archetype.lines(Say.MOVE).random(), null) }
                }
                for (r in task.reminders) if (minute == r.at) once("r${r.at}${r.text.hashCode()}") { remind(sp, m, r.text, r.text, shout = true) }
            }
            com.superessence.squad.model.TaskKind.NOTE -> {
                w.noteT -= 2f
                if (w.noteT <= 0f && task.note.isNotEmpty()) { w.noteT = 900f; showBubble(sp, m.name, "Pense-bête : ${task.note}") }
            }
            com.superessence.squad.model.TaskKind.TAPPER -> {
                if (task.tapX >= 0f) {
                    if (task.everyMin > 0 && now - w.lastTapAt > task.everyMin * 60_000L) { w.lastTapAt = now; startTapRun(sp, m) }
                    if (task.atMin >= 0 && minute == task.atMin) once("tap${task.atMin}") { w.lastTapAt = now; startTapRun(sp, m) }
                }
            }
            else -> {}
        }
    }

    /** Rappel : bulle, vibration et notification. */
    private fun remind(sp: Sprite, m: Mascot, text: String, notif: String?, shout: Boolean = false) {
        showBubble(sp, m.name, text.replace("{nick}", m.archetype.nick), shout)
        if (Squad.state.value.settings.vibrate) sp.view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(NotificationChannel("squad_reminders", "Rappels de la squad", NotificationManager.IMPORTANCE_DEFAULT))
        val n = NotificationCompat.Builder(this, "squad_reminders")
            .setSmallIcon(R.drawable.ic_stat)
            .setContentTitle("${m.name} te rappelle")
            .setContentText(notif ?: text.replace("{nick}", m.archetype.nick))
            .setAutoCancel(true)
            .setContentIntent(PendingIntent.getActivity(this, 9, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE))
            .build()
        nm.notify(2000 + (text.hashCode() and 0xfff), n)
    }

    /** Ce qu'il dit de son métier quand on le touche (ou de temps en temps). */
    private fun jobTalk(sp: Sprite, m: Mascot, idle: Boolean) {
        val task = m.task
        val nick = m.archetype.nick
        val txt = when (task.kind) {
            com.superessence.squad.model.TaskKind.CLOCK -> "Il est ${java.text.SimpleDateFormat("HH:mm", java.util.Locale.FRANCE).format(java.util.Date())}, $nick."
            com.superessence.squad.model.TaskKind.BATTERY -> if (battPct < 0) "Je surveille ta batterie." else "Batterie à $battPct %${if (charging) ", ça charge." else "."}"
            com.superessence.squad.model.TaskKind.NOTE -> if (task.note.isEmpty()) "Écris-moi une note dans ma fiche." else "Pense-bête : ${task.note}"
            com.superessence.squad.model.TaskKind.COACH -> Life.nextUp(Squad.state.value.schedule) + ". Et bois de l'eau !"
            com.superessence.squad.model.TaskKind.GUARD -> "Je surveille ma zone. Personne ne passe."
            com.superessence.squad.model.TaskKind.LAUNCHER -> if (task.pkg.isEmpty()) "Choisis-moi une appli dans ma fiche (appui long)." else "Touche-moi et j'ouvre ${task.appLabel}."
            com.superessence.squad.model.TaskKind.TAPPER -> if (task.tapX < 0f) "Montre-moi où cliquer, dans ma fiche." else "Touche-moi et je vais cliquer."
            else -> null
        } ?: return
        if (idle && rnd.nextFloat() < 0.6f) return
        showBubble(sp, m.name, txt)
    }

    private fun launchApp(sp: Sprite, m: Mascot) {
        val pkg = m.task.pkg
        if (pkg.isEmpty()) { jobTalk(sp, m, false); return }
        ws(sp).tapAnim = 0.7f
        say(sp, m, Say.OPEN_APP)
        android.os.Handler(mainLooper).postDelayed({
            val i = packageManager.getLaunchIntentForPackage(pkg)?.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            if (i == null || runCatching { startActivity(i) }.isFailure) showBubble(sp, m.name, "Je trouve plus ${m.task.appLabel}…")
        }, 450)
    }

    private fun startTapRun(sp: Sprite, m: Mascot) {
        val w = ws(sp)
        if (w.tapRun != null) return
        if (m.task.tapX < 0f) { jobTalk(sp, m, false); return }
        if (!TapService.isEnabled(this) || TapService.instance == null) { say(sp, m, Say.NEED_ACCESS); return }
        val px = m.task.tapX * screenW; val py = m.task.tapY * screenH
        val facing = if (px - sp.lp.width >= 0f) 1f else -1f
        val run = TapRun(px, py, facing)
        w.tapRun = run
        val wx = if (facing > 0f) px - sp.lp.width else px
        goTo(sp, wx, py - sp.lp.height * 0.643f)
        sp.encSpeed = 1.6f
        say(sp, m, Say.TAP_GO)
    }

    private fun setTouchable(sp: Sprite, on: Boolean) {
        if (sp.parked) return
        if (on) { sp.lp.flags = sp.lp.flags and WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE.inv(); sp.lp.alpha = 1f }
        else { sp.lp.flags = sp.lp.flags or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE; sp.lp.alpha = 0.8f }
        runCatching { wm.updateViewLayout(sp.view, sp.lp) }
    }

    private fun stepTapRun(sp: Sprite, m: Mascot, w: WorkState, run: TapRun, dt: Float, postX: Float, postY: Float) {
        when (run.stage) {
            0 -> {
                sp.encAct = Act.RUN
                if (arrived(sp)) { sp.encMove = false; run.stage = 1; run.t = 0f; sp.view.pose.facing = run.facing }
            }
            1 -> {
                run.t += dt
                sp.encAct = Act.TAP; sp.view.pose.facing = run.facing
                if (run.t > 0.25f && !run.flagsOff) { run.flagsOff = true; setTouchable(sp, false) }
                if (run.t > 0.45f && !run.tapped) {
                    run.tapped = true
                    // on masque aussi la bulle pendant le clic
                    hideBubble(sp)
                    val ok = TapService.instance?.tap(run.px, run.py) == true
                    if (!ok) say(sp, m, Say.NEED_ACCESS)
                    else Squad.editMascot(sp.id) { it.copy(xp = it.xp + 1) }
                }
                if (run.t > 1.0f) {
                    setTouchable(sp, true)
                    run.stage = 2
                    goTo(sp, postX, postY); sp.encSpeed = 1.3f
                }
            }
            else -> {
                sp.encAct = Act.WALK
                if (arrived(sp)) { sp.encMove = false; sp.encSpeed = 1f; w.tapRun = null }
            }
        }
    }

    /** Les vigiles repoussent ceux qui entrent dans leur zone. */
    private fun guardZones(dt: Float) {
        val guards = sprites.values.filter { g -> ws(g).active && Squad.get(g.id)?.task?.kind == com.superessence.squad.model.TaskKind.GUARD && g.brain.act != Act.SLEEP }
        if (guards.isEmpty()) return
        val rad = 115 * density * Squad.state.value.settings.size
        for (g in guards) {
            val w = ws(g)
            w.guardSayT -= dt
            for (o in sprites.values) {
                if (o === g || o.dragged || o.parked || inEvent(o) || o.enc != null) continue
                val dx = o.cx - g.cx; val dy = o.cy - g.cy
                val d = hypot(dx, dy)
                if (d < rad) {
                    val push = 110 * density * dt
                    val nx = if (d < 1f) 1f else dx / d; val ny = if (d < 1f) 0f else dy / d
                    o.x = (o.x + nx * push).coerceIn(0f, (screenW - o.lp.width).toFloat())
                    o.y = (o.y + ny * push).coerceIn(0f, (screenH - o.lp.height).toFloat())
                    if (hypot(o.tx - g.cx, o.ty - g.cy) < rad) { o.tx = (g.cx + nx * rad * 1.4f - o.lp.width / 2f).coerceIn(0f, (screenW - o.lp.width).toFloat()); o.ty = (g.cy + ny * rad * 1.4f - o.lp.height / 2f).coerceIn(0f, (screenH - o.lp.height).toFloat()) }
                    if (w.guardSayT <= 0f) {
                        w.guardSayT = 9f
                        g.view.pose.facing = if (dx > 0) 1f else -1f
                        g.punchT = 0.25f
                        Squad.get(g.id)?.let { showBubble(g, it.name, it.archetype.lines(Say.GUARD_OUT).random(), shout = true) }
                    }
                }
            }
        }
    }

    /** Vérifie régulièrement si un agent d'entretien a du boulot. */
    private fun workChecks(dt: Float) {
        workCheckT -= dt
        if (workCheckT > 0f) return
        workCheckT = 3f
        if (cleaner == null && (stains.isNotEmpty() || mess.isNotEmpty())) {
            val c = sprites.values.firstOrNull { free(it) && Squad.get(it.id)?.task?.kind == com.superessence.squad.model.TaskKind.CLEANER }
            if (c != null) startClean(manual = false, who0 = c)
        }
    }

    // ---- choix de l'endroit à cliquer

    private fun showPicker(id: String) {
        hidePicker()
        val name = Squad.get(id)?.name ?: return
        val v = PickView(this, density, name)
        v.listener = object : PickView.Listener {
            override fun onPickMove(v: PickView, dx: Float, dy: Float) {
                val lp = pickerLp ?: return
                lp.x = (lp.x + dx).toInt().coerceIn(-lp.width / 2, screenW - lp.width / 2)
                lp.y = (lp.y + dy).toInt().coerceIn(-(v.targetY.toInt()), screenH - v.targetY.toInt())
                runCatching { wm.updateViewLayout(v, lp) }
            }

            override fun onPickDone(v: PickView, ok: Boolean) {
                val lp = pickerLp ?: return
                val pid = pickerId
                if (ok && pid != null) {
                    val cx = lp.x + lp.width / 2f; val cy = lp.y + v.targetY
                    Squad.editMascot(pid) { it.copy(task = it.task.copy(tapX = cx / screenW, tapY = cy / screenH)) }
                    sprites[pid]?.let { sp -> Squad.get(pid)?.let { showBubble(sp, it.name, "Compris, je clique là !") } }
                }
                hidePicker()
            }
        }
        val w = (230 * density).toInt(); val h = (128 * density).toInt()
        val lp = WindowManager.LayoutParams(
            w, h, overlayType(),
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply { gravity = Gravity.TOP or Gravity.START; x = (screenW - w) / 2; y = screenH / 2 - h / 2 }
        runCatching { wm.addView(v, lp); picker = v; pickerLp = lp; pickerId = id }
    }

    private fun hidePicker() {
        picker?.let { runCatching { wm.removeView(it) } }
        picker = null; pickerLp = null; pickerId = null
    }

    // ------------------------------------------------------------------ taches, bordel et ménage

    private inner class StainItem(val view: StainView, val lp: WindowManager.LayoutParams) {
        val born = System.currentTimeMillis()
        var wiping = false
        val cx get() = lp.x + lp.width / 2f
        val cy get() = lp.y + lp.height / 2f
    }

    private inner class ClutterItem(val view: ClutterView, val lp: WindowManager.LayoutParams) {
        var spinTo = 0f
        val cx get() = lp.x + lp.width / 2f
        val cy get() = lp.y + lp.height / 2f
    }

    private val stains = mutableListOf<StainItem>()
    private val mess = mutableListOf<ClutterItem>()
    private var cleaner: Sprite? = null
    private val cleanQueue = ArrayDeque<Any>()
    private var cleanTarget: Any? = null
    private var cleanT = 0f

    private fun foodStain(arch: String): com.superessence.squad.model.Stain {
        return when (arch) {
            "marco" -> listOf(com.superessence.squad.model.Stain.BBQ, com.superessence.squad.model.Stain.KETCHUP, com.superessence.squad.model.Stain.GREASE, com.superessence.squad.model.Stain.MUSTARD)
            "tank" -> listOf(com.superessence.squad.model.Stain.SHAKE, com.superessence.squad.model.Stain.CRUMBS, com.superessence.squad.model.Stain.GREASE)
            "ragnar" -> listOf(com.superessence.squad.model.Stain.GREASE, com.superessence.squad.model.Stain.BBQ, com.superessence.squad.model.Stain.CRUMBS)
            "zero" -> listOf(com.superessence.squad.model.Stain.SODA, com.superessence.squad.model.Stain.CRUMBS, com.superessence.squad.model.Stain.KETCHUP)
            "rex" -> listOf(com.superessence.squad.model.Stain.KETCHUP, com.superessence.squad.model.Stain.MUSTARD, com.superessence.squad.model.Stain.GREASE)
            "jo" -> listOf(com.superessence.squad.model.Stain.CHOCO, com.superessence.squad.model.Stain.CRUMBS, com.superessence.squad.model.Stain.GREASE)
            "kenji" -> listOf(com.superessence.squad.model.Stain.CRUMBS, com.superessence.squad.model.Stain.SODA)
            else -> com.superessence.squad.model.Stain.entries
        }.random()
    }

    private fun spawnStain(sp: Sprite, m: Mascot, type0: com.superessence.squad.model.Stain? = null) {
        val type = type0 ?: foodStain(m.arch)
        val size = ((38 + rnd.nextInt(44)) * density).toInt()
        val v = StainView(this, type, rnd.nextInt())
        val lp = WindowManager.LayoutParams(
            size, size, overlayType(),
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            alpha = 0.8f
            x = (sp.cx + (rnd.nextFloat() - 0.5f) * 140 * density - size / 2f).toInt().coerceIn(0, screenW - size)
            y = (sp.y + rnd.nextFloat() * sp.lp.height - size / 2f).toInt().coerceIn(0, screenH - size)
        }
        runCatching { wm.addView(v, lp); stains += StainItem(v, lp) }
        while (stains.size > 30) removeStain(stains.first())
        if (rnd.nextFloat() < 0.28f) say(sp, m, Say.BURP)
    }

    private fun removeStain(s: StainItem) {
        runCatching { wm.removeView(s.view) }
        stains.remove(s)
    }

    private fun addClutter(type: com.superessence.squad.model.Clutter, cx: Float, bottomY: Float) {
        val k = Squad.state.value.settings.size
        val w = (type.w * density * k).toInt(); val h = (type.h * density * k).toInt()
        val v = ClutterView(this, type, rnd.nextInt(100))
        v.listener = clutterListener
        val lp = WindowManager.LayoutParams(
            w, h, overlayType(),
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = (cx - w / 2f).toInt().coerceIn(0, screenW - w)
            y = (bottomY - h).toInt().coerceIn(0, screenH - h)
        }
        val item = ClutterItem(v, lp)
        v.spin = (rnd.nextFloat() - 0.5f) * 30f; item.spinTo = v.spin
        runCatching { wm.addView(v, lp); mess += item }
        while (mess.size > 16) removeClutter(mess.first())
    }

    private fun removeClutter(c: ClutterItem) {
        runCatching { wm.removeView(c.view) }
        mess.remove(c)
    }

    private val clutterListener = object : ClutterView.Listener {
        override fun onClutterDrag(v: ClutterView, rawX: Float, rawY: Float, dx: Float, dy: Float) {
            val c = mess.firstOrNull { it.view === v } ?: return
            showTrash()
            c.lp.x = (c.lp.x + dx).toInt().coerceIn(0, screenW - c.lp.width)
            c.lp.y = (c.lp.y + dy).toInt().coerceIn(0, screenH - c.lp.height)
            runCatching { wm.updateViewLayout(v, c.lp) }
            trash?.hot = overTrash(rawX, rawY)
        }

        override fun onClutterDrop(v: ClutterView, rawX: Float, rawY: Float) {
            val c = mess.firstOrNull { it.view === v } ?: return
            val gone = overTrash(rawX, rawY)
            hideTrash()
            if (gone) { removeClutter(c); if (Squad.state.value.settings.vibrate) v.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY) }
        }

        override fun onClutterTap(v: ClutterView) {
            // petit coup de pied dedans
            val c = mess.firstOrNull { it.view === v } ?: return
            c.lp.x = (c.lp.x + (rnd.nextFloat() - 0.5f) * 60 * density).toInt().coerceIn(0, screenW - c.lp.width)
            c.spinTo += (rnd.nextFloat() - 0.5f) * 120f
            runCatching { wm.updateViewLayout(v, c.lp) }
            if (Squad.state.value.settings.vibrate) v.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        }
    }

    /** Un gars prend l'éponge et range tout le bordel. */
    private fun startClean(manual: Boolean, who0: Sprite? = null) {
        if (cleaner != null) return
        if (stains.isEmpty() && mess.isEmpty()) return
        val pool = sprites.values.filter { free(it) }
        val who = who0 ?: pool.firstOrNull { Squad.get(it.id)?.task?.kind == com.superessence.squad.model.TaskKind.CLEANER }
            ?: pool.firstOrNull { it.id.let { id -> Squad.get(id)?.arch == "sarge" } } ?: pool.randomOrNull()
        if (who == null) {
            if (manual) { stains.toList().forEach { removeStain(it) }; mess.toList().forEach { removeClutter(it) } }
            return
        }
        cleaner = who
        cleanQueue.clear(); cleanQueue.addAll(stains); cleanQueue.addAll(mess)
        cleanTarget = null
        who.brain.clearForce(); who.encSpeed = 1.5f
        Squad.get(who.id)?.let { say(who, it, Say.CLEAN) }
    }

    private fun cleaning(dt: Float) {
        val sp = cleaner ?: return
        if (sprites[sp.id] !== sp) { cleaner = null; return }
        if (sp.dragged) return
        val tgt = cleanTarget
        if (tgt == null) {
            // le plus proche d'abord
            val next = cleanQueue.minByOrNull { val (x, y) = posOf(it); hypot(x - sp.cx, y - sp.cy) }
            if (next == null) {
                cleaner = null; sp.encAct = null; sp.encMove = false; sp.encSpeed = 1f
                sp.brain.force(Act.FLEX, 2f)
                Squad.editMascot(sp.id) { it.withEvent("A nettoyé l'écran") }
                return
            }
            cleanQueue.remove(next)
            if (next is StainItem && next !in stains || next is ClutterItem && next !in mess) return
            cleanTarget = next; cleanT = 0f
            val (x, y) = posOf(next)
            goTo(sp, x - sp.lp.width / 2f, y - sp.lp.height * 0.45f)
            return
        }
        if (!arrived(sp)) return
        sp.encMove = false
        cleanT += dt
        when (tgt) {
            is StainItem -> {
                sp.encAct = Act.WIPE
                tgt.view.fade = (1f - cleanT / 1.6f).coerceAtLeast(0f); tgt.view.invalidate()
                if (cleanT >= 1.6f) { removeStain(tgt); cleanTarget = null }
            }
            is ClutterItem -> {
                sp.encAct = Act.CARRY
                if (cleanT >= 0.8f) { removeClutter(tgt); cleanTarget = null }
            }
            else -> cleanTarget = null
        }
    }

    private fun posOf(o: Any): Pair<Float, Float> = when (o) {
        is StainItem -> o.cx to o.cy
        is ClutterItem -> o.cx to o.cy
        else -> 0f to 0f
    }

    // ------------------------------------------------------------------ paroles

    private fun say(sp: Sprite, m: Mascot, what: Say) {
        val lines = m.archetype.lines(what)
        if (lines.isEmpty()) return
        showBubble(sp, m.name, lines.random(), what in shoutSet)
    }

    private fun showBubble(sp: Sprite, name: String, text: String, shout: Boolean = false) {
        val maxW = (220 * density).toInt().coerceAtMost(screenW - (16 * density).toInt())
        val b = sp.bubble ?: BubbleView(this, density)
        val (bw, bh) = b.set(name, text, maxW, shout)
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
        val jit = if (b.shout) (sin(t * 70f) * 2.5f * density) else 0f
        val nx = (bx + jit).toInt(); val ny = by.toInt()
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
        if (sp.brain.act == Act.SLEEP && !ws(sp).active) { say(sp, m, Say.NIGHT); return }
        if (ws(sp).active) {
            if (ws(sp).tapRun != null) return
            when (m.task.kind) {
                com.superessence.squad.model.TaskKind.LAUNCHER -> launchApp(sp, m)
                com.superessence.squad.model.TaskKind.TAPPER -> startTapRun(sp, m)
                else -> jobTalk(sp, m, idle = false)
            }
            return
        }
        if (m.task.kind == com.superessence.squad.model.TaskKind.CLEANER && sp !== cleaner) {
            if (stains.isEmpty() && mess.isEmpty()) showBubble(sp, m.name, "Tout est propre, ${m.archetype.nick}.") else startClean(manual = true, who0 = sp.takeIf { free(it) })
            return
        }
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
        if (inEvent(sp)) { saveOne(sp); return }
        if (m.task.kind.posted) {
            // on déplace son poste de travail
            val maxX = (screenW - sp.lp.width).toFloat(); val maxY = (screenH - sp.lp.height).toFloat()
            val fx = if (maxX > 0) sp.x / maxX else 0.5f; val fy = if (maxY > 0) sp.y / maxY else 0.5f
            Squad.editMascot(sp.id) { it.copy(task = it.task.copy(postX = fx, postY = fy)) }
            ws(sp).tapRun?.let { setTouchable(sp, true) }; ws(sp).tapRun = null
            showBubble(sp, m.name, "Ok, mon poste est ici maintenant.")
            return
        }
        val victim = sprites.values.filter { it !== sp && !it.dragged && it.enc == null && !inEvent(it) && !it.parked }
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
            .addAction(0, "Nettoyer", PendingIntent.getService(this, 3, Intent(this, OverlayService::class.java).setAction(ACTION_CLEAN), flags))
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
