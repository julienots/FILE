package com.superessence.pleinphare

import android.content.Context
import android.content.Intent
import android.database.ContentObserver
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.MediaStore
import android.provider.Settings
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.pleinphare.data.MusicLibrary
import com.superessence.pleinphare.data.Song
import com.superessence.pleinphare.data.filterSongs
import com.superessence.pleinphare.data.groupByDay
import com.superessence.pleinphare.data.sortByDate
import com.superessence.pleinphare.playback.PlayerConnection
import com.superessence.pleinphare.ui.Fredoka
import com.superessence.pleinphare.ui.Ink
import com.superessence.pleinphare.ui.LibraryScreen
import com.superessence.pleinphare.ui.LogoMark
import com.superessence.pleinphare.ui.Nunito
import com.superessence.pleinphare.ui.PlayerScreen
import com.superessence.pleinphare.ui.PleinPhareTheme
import com.superessence.pleinphare.ui.SplashScreen
import com.superessence.pleinphare.ui.SuperYellow
import com.superessence.pleinphare.ui.TextMid
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    private lateinit var connection: PlayerConnection

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        connection = PlayerConnection(applicationContext)
        setContent {
            PleinPhareTheme {
                AppRoot(connection) { on ->
                    if (on) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                    else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        if (MusicLibrary.hasPermission(this)) connection.connect()
    }

    override fun onStop() {
        connection.release()
        super.onStop()
    }

    fun connectPlayer() = connection.connect()
}

private object Prefs {
    fun of(ctx: Context) = ctx.getSharedPreferences("ui", Context.MODE_PRIVATE)
}

@Composable
private fun AppRoot(connection: PlayerConnection, applyKeepScreenOn: (Boolean) -> Unit) {
    val ctx = LocalContext.current
    val prefs = remember { Prefs.of(ctx) }
    var splashDone by rememberSaveable { mutableStateOf(false) }
    var granted by remember { mutableStateOf(MusicLibrary.hasPermission(ctx)) }
    var songs by remember { mutableStateOf(MusicLibrary.cache.orEmpty()) }
    var loading by remember { mutableStateOf(MusicLibrary.cache == null) }
    var reloadTick by remember { mutableIntStateOf(0) }
    var newestFirst by remember { mutableStateOf(prefs.getBoolean("newestFirst", true)) }
    var keepScreenOn by remember { mutableStateOf(prefs.getBoolean("keepScreenOn", true)) }
    var showPlayer by rememberSaveable { mutableStateOf(false) }
    var searchOpen by rememberSaveable { mutableStateOf(false) }
    var query by rememberSaveable { mutableStateOf("") }
    val player by connection.state.collectAsState()

    LaunchedEffect(keepScreenOn) { applyKeepScreenOn(keepScreenOn) }

    // Chargement (et rechargement automatique quand de nouveaux morceaux arrivent sur le téléphone)
    LaunchedEffect(granted, reloadTick) {
        if (!granted) return@LaunchedEffect
        (ctx as? MainActivity)?.connectPlayer()
        songs = withContext(Dispatchers.IO) { MusicLibrary.load(ctx) }
        loading = false
    }
    DisposableEffect(granted) {
        if (!granted) return@DisposableEffect onDispose { }
        val handler = Handler(Looper.getMainLooper())
        val reload = Runnable { reloadTick++ }
        val observer = object : ContentObserver(handler) {
            override fun onChange(selfChange: Boolean) {
                handler.removeCallbacks(reload)
                handler.postDelayed(reload, 1500)
            }
        }
        ctx.contentResolver.registerContentObserver(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, true, observer)
        onDispose {
            handler.removeCallbacks(reload)
            ctx.contentResolver.unregisterContentObserver(observer)
        }
    }

    val ordered = remember(songs, newestFirst) { sortByDate(songs, newestFirst) }
    val visible = remember(ordered, query) { filterSongs(ordered, query) }
    val groups = remember(visible) { groupByDay(visible) }
    val byId = remember(songs) { songs.associateBy { it.id } }
    val currentSong = player.currentId?.let { byId[it] }

    BackHandler(enabled = showPlayer) { showPlayer = false }
    BackHandler(enabled = !showPlayer && searchOpen) { searchOpen = false; query = "" }

    Box(Modifier.fillMaxSize().background(Ink)) {
        when {
            !splashDone -> SplashScreen { splashDone = true }
            !granted -> PermissionScreen { granted = true }
            else -> AnimatedContent(
                targetState = showPlayer && player.hasMedia,
                transitionSpec = {
                    if (targetState) slideInVertically { it / 3 } + fadeIn() togetherWith fadeOut()
                    else fadeIn() togetherWith slideOutVertically { it / 3 } + fadeOut()
                },
                label = "screens",
            ) { inPlayer ->
                if (inPlayer) {
                    PlayerScreen(
                        p = player, song = currentSong,
                        keepScreenOn = keepScreenOn,
                        onToggleKeepScreenOn = { keepScreenOn = !keepScreenOn; prefs.edit().putBoolean("keepScreenOn", keepScreenOn).apply() },
                        onClose = { showPlayer = false },
                        onTogglePlay = connection::togglePlay,
                        onNext = { connection.next() },
                        onPrevious = { connection.previous() },
                        onSeek = { connection.seekTo(it) },
                        onShuffle = { connection.toggleShuffle() },
                        onRepeat = { connection.cycleRepeat() },
                    )
                } else {
                    LibraryScreen(
                        groups = groups,
                        totalCount = songs.size,
                        loading = loading,
                        newestFirst = newestFirst,
                        onToggleSort = { newestFirst = !newestFirst; prefs.edit().putBoolean("newestFirst", newestFirst).apply() },
                        searchOpen = searchOpen,
                        onSearchOpen = { searchOpen = it },
                        query = query,
                        onQuery = { query = it },
                        keepScreenOn = keepScreenOn,
                        onToggleKeepScreenOn = { keepScreenOn = !keepScreenOn; prefs.edit().putBoolean("keepScreenOn", keepScreenOn).apply() },
                        player = player,
                        currentSong = currentSong,
                        onPlaySong = { s -> connection.playList(visible, visible.indexOf(s)) },
                        onPlayAll = { connection.playList(visible, 0); showPlayer = true },
                        onShuffleAll = {
                            if (visible.isNotEmpty()) connection.playList(visible, visible.indices.random(), shuffle = true)
                            showPlayer = true
                        },
                        onOpenPlayer = { showPlayer = true },
                        onTogglePlay = connection::togglePlay,
                        onNext = { connection.next() },
                    )
                }
            }
        }
    }
}

@Composable
private fun PermissionScreen(onGranted: () -> Unit) {
    val ctx = LocalContext.current
    var refused by remember { mutableStateOf(false) }
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { res ->
        if (MusicLibrary.hasPermission(ctx)) onGranted() else refused = true
    }
    fun ask() {
        val perms = buildList {
            add(MusicLibrary.permission)
            if (Build.VERSION.SDK_INT >= 33) add(android.Manifest.permission.POST_NOTIFICATIONS)
        }
        launcher.launch(perms.toTypedArray())
    }
    LaunchedEffect(Unit) { ask() }

    Column(
        Modifier.fillMaxSize().background(Ink).padding(28.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        LogoMark(Modifier.size(130.dp))
        Spacer(Modifier.height(18.dp))
        Text("Accès à ta musique", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 28.sp, color = SuperYellow)
        Spacer(Modifier.height(12.dp))
        Text(
            "Plein Phare a besoin de lire les fichiers audio du téléphone pour les afficher, du plus récent au plus ancien. " +
                "Rien ne quitte ton téléphone.",
            fontFamily = Nunito, fontSize = 18.sp, color = TextMid, textAlign = TextAlign.Center,
        )
        Spacer(Modifier.height(28.dp))
        Box(
            Modifier.fillMaxWidth().height(64.dp).clip(RoundedCornerShape(20.dp)).background(SuperYellow)
                .clickable {
                    if (refused) {
                        ctx.startActivity(
                            Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", ctx.packageName, null)),
                        )
                    } else ask()
                },
            contentAlignment = Alignment.Center,
        ) {
            Text(
                if (refused) "Ouvrir les réglages" else "Autoriser",
                fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 20.sp, color = Ink,
            )
        }
        if (refused) {
            Spacer(Modifier.height(12.dp))
            Text(
                "Dans Réglages › Autorisations, active « Musique et audio », puis reviens ici.",
                fontFamily = Nunito, fontSize = 15.sp, color = TextMid, textAlign = TextAlign.Center,
            )
        }
    }
    // Retour depuis les réglages : on revérifie.
    DisposableEffect(Unit) {
        val activity = ctx as? ComponentActivity
        val obs = androidx.lifecycle.LifecycleEventObserver { _, e ->
            if (e == androidx.lifecycle.Lifecycle.Event.ON_RESUME && MusicLibrary.hasPermission(ctx)) onGranted()
        }
        activity?.lifecycle?.addObserver(obs)
        onDispose { activity?.lifecycle?.removeObserver(obs) }
    }
}
