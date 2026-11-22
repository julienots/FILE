package com.superessence.sleepcut

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.superessence.sleepcut.ui.MainScreen
import com.superessence.sleepcut.ui.SleepTheme

class MainActivity : ComponentActivity() {
    private val notifPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }
    private var resumeTick by mutableIntStateOf(0)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(statusBarStyle = SystemBarStyle.dark(0x00000000), navigationBarStyle = SystemBarStyle.dark(0x00000000))
        TimerController.load(this)
        TimerService.createChannels(this)
        handleShortcut(intent)
        if (Build.VERSION.SDK_INT >= 33) notifPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
        setContent {
            SleepTheme {
                val st by TimerController.state.collectAsStateWithLifecycle()
                val settings by TimerController.settings.collectAsStateWithLifecycle()
                MainScreen(st, settings, resumeTick, actions)
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleShortcut(intent)
    }

    override fun onResume() {
        super.onResume()
        resumeTick++ // re-vérifie les autorisations au retour des réglages
    }

    private fun handleShortcut(i: Intent?) {
        val m = i?.getIntExtra("minutes", 0) ?: 0
        if (i?.action == ACTION_SHORTCUT && m > 0) TimerController.start(this, m)
    }

    val actions = object : Actions {
        override fun start(minutes: Int) = TimerController.start(this@MainActivity, minutes)
        override fun startAt(endAt: Long) = TimerController.startAt(this@MainActivity, endAt)
        override fun extend(minutes: Int) = TimerController.extend(this@MainActivity, minutes)
        override fun cancel() = TimerController.cancel(this@MainActivity)
        override fun stopNow() = TimerController.stopNow(this@MainActivity)
        override fun settings(f: (com.superessence.sleepcut.Settings) -> com.superessence.sleepcut.Settings) = TimerController.updateSettings(this@MainActivity, f)
        override fun openNotificationAccess() = startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
        override fun openDndAccess() = startActivity(Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS))
        @SuppressLint("BatteryLife")
        override fun openBattery() {
            runCatching { startActivity(Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$packageName"))) }
                .onFailure { startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)) }
        }
        override fun openExactAlarm() {
            if (Build.VERSION.SDK_INT >= 31) runCatching { startActivity(Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM, Uri.parse("package:$packageName"))) }
        }
        override fun batteryOk() = getSystemService(PowerManager::class.java).isIgnoringBatteryOptimizations(packageName)
        override fun exactOk() = TimerController.canExact(this@MainActivity)
        override fun sessionOk() = MusicStopper.hasSessionAccess(this@MainActivity)
        override fun dndOk() = MusicStopper.hasDndAccess(this@MainActivity)
        override fun nowPlaying() = MusicStopper.nowPlaying(this@MainActivity)
        override fun trackRemaining() = MusicStopper.remainingOfCurrentTrack(this@MainActivity)
    }

    companion object { const val ACTION_SHORTCUT = "com.superessence.sleepcut.SHORTCUT" }
}

interface Actions {
    fun start(minutes: Int)
    fun startAt(endAt: Long)
    fun extend(minutes: Int)
    fun cancel()
    fun stopNow()
    fun settings(f: (com.superessence.sleepcut.Settings) -> com.superessence.sleepcut.Settings)
    fun openNotificationAccess()
    fun openDndAccess()
    fun openBattery()
    fun openExactAlarm()
    fun batteryOk(): Boolean
    fun exactOk(): Boolean
    fun sessionOk(): Boolean
    fun dndOk(): Boolean
    fun nowPlaying(): List<Pair<String, String>>
    fun trackRemaining(): Long?
}
