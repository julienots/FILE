package com.superessence.squad.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings as AndroidSettings
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.squad.engine.Squad
import com.superessence.squad.overlay.OverlayService
import kotlinx.coroutines.delay

class MainActivity : ComponentActivity() {
    companion object { const val EXTRA_MASCOT = "mascot" }

    /** Écran demandé de l'extérieur (appui long sur une mascotte à l'écran). */
    private val openMascot = mutableStateOf<String?>(null)
    private val overlayAllowed = mutableStateOf(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        Squad.init(this)
        openMascot.value = intent?.getStringExtra(EXTRA_MASCOT)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme(primary = C.yellow, background = C.bg, surface = C.card)) {
                App()
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        intent.getStringExtra(EXTRA_MASCOT)?.let { openMascot.value = it }
    }

    override fun onResume() {
        super.onResume()
        overlayAllowed.value = OverlayService.canRun(this)
        Squad.tick()
        val s = Squad.state.value.settings
        if (s.overlayOn && overlayAllowed.value) OverlayService.start(this)
    }

    fun enableOverlay() {
        if (!OverlayService.canRun(this)) {
            startActivity(Intent(AndroidSettings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:$packageName")))
            Squad.edit { it.copy(settings = it.settings.copy(overlayOn = true)) }
            return
        }
        if (Build.VERSION.SDK_INT >= 33 && ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), 7)
        }
        Squad.edit { it.copy(settings = it.settings.copy(overlayOn = true)) }
        OverlayService.start(this)
    }

    fun disableOverlay() {
        Squad.edit { it.copy(settings = it.settings.copy(overlayOn = false)) }
        OverlayService.stop(this)
    }

    fun startHappening(party: Boolean) {
        if (!OverlayService.canRun(this)) { enableOverlay(); return }
        Squad.edit { it.copy(settings = it.settings.copy(overlayOn = true)) }
        OverlayService.happening(this, party)
        moveTaskToBack(true)
    }

    fun openBatterySettings() {
        runCatching { startActivity(Intent(AndroidSettings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)) }
    }

    @Composable
    private fun App() {
        var splash by rememberSaveable { mutableStateOf(true) }
        var nav by rememberSaveable { mutableStateOf("home") }
        val state by Squad.state.collectAsStateWithLifecycle()
        val ext = openMascot.value

        LaunchedEffect(ext) {
            if (ext != null) { splash = false; nav = "detail:$ext"; openMascot.value = null }
        }
        LaunchedEffect(Unit) { while (true) { delay(30_000); Squad.tick() } }

        Box(Modifier.fillMaxSize().background(C.bg)) {
            if (splash) { StudioSplash { splash = false }; return@Box }
            BackHandler(enabled = nav != "home") {
                nav = when {
                    nav.startsWith("style:") -> "detail:" + nav.removePrefix("style:")
                    else -> "home"
                }
            }
            AnimatedContent(nav, transitionSpec = { fadeIn(tween(220)) togetherWith fadeOut(tween(150)) }, label = "nav") { route ->
                Box(Modifier.fillMaxSize().systemBarsPadding()) {
                    when {
                        route == "home" -> HomeScreen(
                            state, overlayAllowed.value,
                            onOpen = { nav = "detail:$it" }, onRecruit = { nav = "recruit" }, onRhythm = { nav = "rhythm" },
                            onOverlay = { on -> if (on) enableOverlay() else disableOverlay() },
                            onHappening = { party -> startHappening(party) },
                        )
                        route == "recruit" -> RecruitScreen(state, onBack = { nav = "home" }, onDone = { nav = "detail:$it" })
                        route == "rhythm" -> RhythmScreen(state, onBack = { nav = "home" }, onBattery = { openBatterySettings() })
                        route.startsWith("detail:") -> {
                            val id = route.removePrefix("detail:")
                            DetailScreen(state, id, onBack = { nav = "home" }, onStyle = { nav = "style:$id" })
                        }
                        route.startsWith("style:") -> {
                            val id = route.removePrefix("style:")
                            StyleScreen(state, id, onBack = { nav = "detail:$id" })
                        }
                    }
                }
            }
        }
    }
}
