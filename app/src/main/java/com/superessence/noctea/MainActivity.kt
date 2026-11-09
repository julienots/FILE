package com.superessence.noctea

import android.content.ComponentName
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.mutableStateOf
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.media3.session.MediaController
import androidx.media3.session.SessionToken
import com.google.common.util.concurrent.ListenableFuture
import com.superessence.noctea.audio.PlaybackService
import com.superessence.noctea.ui.NocteaRoot

class MainActivity : ComponentActivity() {

    private var controllerFuture: ListenableFuture<MediaController>? = null
    /** Demande de navigation provenant d'une notification ("player", "ritual"…). */
    val pendingRoute = mutableStateOf<String?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen()
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(android.graphics.Color.TRANSPARENT),
        )
        super.onCreate(savedInstanceState)
        handleIntent(intent)
        val app = application as NocteaApp
        setContent { NocteaRoot(app, this, skipSplash = savedInstanceState != null) }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent?) {
        intent ?: return
        when {
            intent.getBooleanExtra(EXTRA_OPEN_PLAYER, false) -> pendingRoute.value = "player"
            intent.getStringExtra(EXTRA_OPEN_ROUTE) != null -> pendingRoute.value = intent.getStringExtra(EXTRA_OPEN_ROUTE)
        }
    }

    override fun onStart() {
        super.onStart()
        // Connexion à la session : garde le service lié pendant que l'interface est visible.
        val token = SessionToken(this, ComponentName(this, PlaybackService::class.java))
        controllerFuture = MediaController.Builder(this, token).buildAsync()
    }

    override fun onStop() {
        controllerFuture?.let { MediaController.releaseFuture(it) }
        controllerFuture = null
        super.onStop()
    }

    companion object {
        const val EXTRA_OPEN_PLAYER = "open_player"
        const val EXTRA_OPEN_ROUTE = "open_route"
    }
}
