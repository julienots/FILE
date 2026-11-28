package com.superessence.voxea

import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import com.superessence.voxea.ui.AppViewModel
import com.superessence.voxea.ui.EditScreen
import com.superessence.voxea.ui.HomeScreen
import com.superessence.voxea.ui.IntroScreen
import com.superessence.voxea.ui.PrompterScreen
import com.superessence.voxea.ui.Screen
import com.superessence.voxea.ui.SettingsScreen
import com.superessence.voxea.ui.VoxeaTheme
import com.superessence.voxea.ui.Vx

class MainActivity : ComponentActivity() {
    private val vm: AppViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        handleIntent(intent)
        setContent {
            VoxeaTheme {
                val screen by vm.screen.collectAsState()
                val toast by vm.toast.collectAsState()
                LaunchedEffect(toast) {
                    toast?.let { Toast.makeText(this@MainActivity, it, Toast.LENGTH_LONG).show(); vm.toast(null) }
                }
                BackHandler(enabled = screen !is Screen.Intro && screen !is Screen.Home) { vm.back() }
                val mod = if (screen is Screen.Intro || screen is Screen.Prompter) Modifier.fillMaxSize() else Modifier.fillMaxSize().safeDrawingPadding()
                Box(Modifier.fillMaxSize().background(Vx.Bg)) {
                    Box(mod) {
                        when (val s = screen) {
                            Screen.Intro -> IntroScreen(vm.settings.introSeen, onSeen = { vm.settings.introSeen = true }) {
                                if (pendingText == null) vm.go(Screen.Home) else { vm.go(Screen.Home); consumePending() }
                            }
                            Screen.Home -> HomeScreen(vm)
                            is Screen.Edit -> EditScreen(vm, s.id, s.tab)
                            is Screen.Prompter -> PrompterScreen(vm, s.id)
                            Screen.SettingsScreen -> SettingsScreen(vm)
                        }
                    }
                }
            }
        }
    }

    private var pendingText: Pair<String, String?>? = null
    private var pendingUri: android.net.Uri? = null

    private fun consumePending() {
        pendingText?.let { vm.importText(it.first, it.second) }
        pendingUri?.let { vm.importUri(it) }
        pendingText = null; pendingUri = null
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
        if (vm.screen.value != Screen.Intro) consumePending()
    }

    /** Script partagé depuis une autre app (Claude, Fichiers…) ou ouvert avec VOXÉA. */
    private fun handleIntent(i: Intent?) {
        when (i?.action) {
            Intent.ACTION_SEND -> {
                val text = i.getStringExtra(Intent.EXTRA_TEXT)
                @Suppress("DEPRECATION") val uri = i.getParcelableExtra<android.net.Uri>(Intent.EXTRA_STREAM)
                if (text != null) pendingText = text to i.getStringExtra(Intent.EXTRA_SUBJECT) else pendingUri = uri
            }
            Intent.ACTION_VIEW -> pendingUri = i.data
        }
        if (pendingText == null && pendingUri == null) return
        // Données reçues alors que l'app tourne déjà : on importe sans rejouer l'intro
        if (vm.screen.value != Screen.Intro) consumePending()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val s = vm.screen.value
        if (s is Screen.Prompter && vm.settings.volumeKeys && vm.prompter.value.recording &&
            (event.keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || event.keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
                event.keyCode == KeyEvent.KEYCODE_HEADSETHOOK || event.keyCode == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE)
        ) {
            if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) vm.project(s.id)?.let { vm.prompterNext(it) }
            return true
        }
        return super.dispatchKeyEvent(event)
    }
}
