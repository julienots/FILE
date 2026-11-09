package com.superessence.noctea.ui.screens

import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.superessence.noctea.NocteaApp
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.notify.Reminders
import com.superessence.noctea.ui.components.MoonMark
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.components.PrimaryButton
import com.superessence.noctea.ui.components.SecondaryButton
import com.superessence.noctea.ui.theme.NocteaTheme
import com.superessence.noctea.ui.theme.Palettes
import kotlinx.coroutines.delay
import java.time.LocalTime

/** Réveil doux : s'affiche au-dessus de l'écran verrouillé et lance la séance du matin. */
class WakeActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val app = application as NocteaApp
        val trackId = app.store.value.alarm.trackId
        app.engine.play(MediaIds.track(trackId), resume = false)
        Reminders.reschedule(this) // programme le prochain réveil
        setContent {
            NocteaTheme(Palettes.Aurore) {
                var now by remember { mutableStateOf(LocalTime.now()) }
                LaunchedEffect(Unit) { while (true) { delay(10_000); now = LocalTime.now() } }
                NightBackground {
                    Column(
                        Modifier.fillMaxSize().padding(32.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center,
                    ) {
                        MoonMark(130.dp)
                        Text("%02d:%02d".format(now.hour, now.minute), style = MaterialTheme.typography.displayLarge)
                        Text("Bonjour… prends ton temps.", style = MaterialTheme.typography.headlineSmall)
                        Spacer(Modifier.height(40.dp))
                        PrimaryButton("Je suis réveillé·e", Modifier.fillMaxWidth()) { finish() }
                        Spacer(Modifier.height(12.dp))
                        SecondaryButton("Encore 10 minutes", Modifier.fillMaxWidth()) {
                            app.engine.stop()
                            Reminders.snooze(this@WakeActivity, 10)
                            finish()
                        }
                    }
                }
            }
        }
    }
}
