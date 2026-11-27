package com.superessence.pleinphare

import android.graphics.Bitmap
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import org.robolectric.annotation.GraphicsMode
import com.superessence.pleinphare.data.Song
import com.superessence.pleinphare.data.groupByDay
import com.superessence.pleinphare.data.sortByDate
import com.superessence.pleinphare.playback.PlayerUi
import com.superessence.pleinphare.ui.LibraryScreen
import com.superessence.pleinphare.ui.PlayerScreen
import com.superessence.pleinphare.ui.PleinPhareTheme
import com.superessence.pleinphare.ui.SplashScreen
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import java.time.LocalDate
import java.time.ZoneId

/** Rendu des écrans principaux (images dans app/build/screenshots). */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(qualifiers = "w411dp-h891dp-xxhdpi")
class ScreenshotTest {
    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    private fun save(name: String) {
        var bmp: Bitmap? = null
        rule.activityRule.scenario.onActivity { act ->
            val v = act.window.decorView.rootView
            val b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
            v.draw(android.graphics.Canvas(b))
            bmp = b
        }
        val dir = File("build/screenshots").apply { mkdirs() }
        File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    private val today = LocalDate.now()
    private fun sec(daysAgo: Long, h: Int) = today.minusDays(daysAgo).atTime(h, 0).atZone(ZoneId.systemDefault()).toEpochSecond()
    private val songs = listOf(
        Song(1, "Nuit d'été sur l'autoroute", "Les Phares", "Route 66", 1, 214_000, sec(0, 9)),
        Song(2, "Kilomètre zéro", "Nova Drive", "", 2, 188_000, sec(0, 8)),
        Song(3, "Station-service", "Lune Jaune", "", 3, 241_000, sec(1, 20)),
        Song(4, "Rétroviseur", "Les Phares", "", 1, 199_000, sec(1, 19)),
        Song(5, "Aire de repos", "Camille B.", "", 4, 265_000, sec(6, 12)),
        Song(6, "Feux de croisement", "Nova Drive", "", 2, 177_000, sec(15, 12)),
        Song(7, "Péage", "Artiste inconnu", "", 5, 230_000, sec(15, 11)),
        Song(8, "Ligne blanche", "Lune Jaune", "", 3, 212_000, sec(400, 11)),
    )
    private val playing = PlayerUi(
        connected = true, hasMedia = true, currentId = 3, title = "Station-service", artist = "Lune Jaune",
        isPlaying = true, positionMs = 83_000, durationMs = 241_000, shuffle = false, index = 2, count = 8,
    )

    @Test fun splash() {
        rule.mainClock.autoAdvance = false
        rule.setContent { PleinPhareTheme { SplashScreen {} } }
        rule.mainClock.advanceTimeBy(2_600); save("1-splash-superessence")
        rule.mainClock.advanceTimeBy(4_200); save("2-splash-pleinphare")
    }

    @Test fun library() {
        val g = groupByDay(sortByDate(songs, true))
        rule.mainClock.autoAdvance = false
        rule.setContent {
            PleinPhareTheme {
                LibraryScreen(
                    g, songs.size, false, true, {}, false, {}, "", {}, true, {}, playing, songs[2],
                    {}, {}, {}, {}, {}, {},
                )
            }
        }
        rule.mainClock.advanceTimeBy(500); save("3-liste-par-date")
    }

    @Test fun player() {
        rule.mainClock.autoAdvance = false
        rule.setContent { PleinPhareTheme { PlayerScreen(playing, songs[2], true, {}, {}, {}, {}, {}, {}, {}, {}) } }
        rule.mainClock.advanceTimeBy(500); save("4-lecteur")
    }

    @Test @Config(qualifiers = "w891dp-h411dp-xxhdpi")
    fun playerLandscape() {
        rule.mainClock.autoAdvance = false
        rule.setContent { PleinPhareTheme { PlayerScreen(playing, songs[2], true, {}, {}, {}, {}, {}, {}, {}, {}) } }
        rule.mainClock.advanceTimeBy(500); save("5-lecteur-paysage")
    }
}
