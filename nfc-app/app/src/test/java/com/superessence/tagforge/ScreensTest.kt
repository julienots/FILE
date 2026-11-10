package com.superessence.tagforge

import android.graphics.Bitmap
import android.nfc.NdefMessage
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.core.app.ApplicationProvider
import com.superessence.tagforge.data.Store
import com.superessence.tagforge.nfc.*
import com.superessence.tagforge.ui.*
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File

/** Affiche chaque écran (détecte les plantages) et enregistre une capture dans build/screenshots. */
@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class ScreensTest(private val name: String) {
    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun shots() = listOf("01_lire", "02_lire_tag", "03_ecrire_vide", "04_choix_type", "05_editeur_voix", "06_editeur_taches",
            "07_ecrire_contenu", "08_outils", "09_modeles", "10_guide", "11_emulation", "12_mot_de_passe", "13_reglages", "14_scan").map { arrayOf(it) }
    }

    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun render() {
        Store.init(ApplicationProvider.getApplicationContext())
        Nav.stack.clear(); WriteState.items.clear(); NfcBus.cancel(); NfcBus.lastTag = null
        val voice = RecordItem("google_voice", mapOf("text" to "Bienvenue ! Le Wi-Fi est sur la porte du frigo.", "lang" to "fr"))
        when (name) {
            "01_lire" -> Nav.tab = Tab.READ
            "02_lire_tag" -> {
                Nav.tab = Tab.READ
                val m = RecordTypes.buildMessage(listOf(voice, RecordItem("wifi", mapOf("ssid" to "MaBox", "pass" to "motdepasse123", "auth" to "WPA2"))))
                NfcBus.lastTag = TagInfo("04:A2:3B:1C:9F:61:80", listOf("NfcA", "MifareUltralight", "Ndef"), "NTAG215", "NFC Forum Type 2 (NTAG / Ultralight)",
                    496, m.byteArrayLength, true, true, m, NdefParser.parse(m), listOf("ATQA" to "44 00", "SAK" to "00", "Puce identifiée" to "NTAG215"),
                    listOf("000  04 A2 3B 15  ..;.  UID", "001  1C 9F 61 80  ..a.  UID"))
            }
            "03_ecrire_vide" -> Nav.tab = Tab.WRITE
            "04_choix_type" -> Nav.push(Sub.TypePicker)
            "05_editeur_voix" -> Nav.push(Sub.Editor(RecordTypes.byId("google_voice")!!, voice, -1))
            "06_editeur_taches" -> Nav.push(Sub.Editor(RecordTypes.byId("tasks")!!, Store.templates.first { it.name.contains("nuit") }.items[0], -1))
            "07_ecrire_contenu" -> { Nav.tab = Tab.WRITE; WriteState.items.add(voice); WriteState.items.add(RecordItem("speak", mapOf("text" to "Bonne journée !"))) }
            "08_outils" -> Nav.tab = Tab.TOOLS
            "09_modeles" -> Nav.tab = Tab.LIBRARY
            "10_guide" -> Nav.push(Sub.Guide)
            "11_emulation" -> { Store.emulationNdef = RecordTypes.buildMessage(listOf(voice)).toByteArray(); Store.emulationLabel = "Voix Google"; Nav.push(Sub.Emulation) }
            "12_mot_de_passe" -> Nav.push(Sub.Password)
            "13_reglages" -> Nav.push(Sub.Settings)
            "14_scan" -> { Nav.tab = Tab.WRITE; WriteState.items.add(voice); NfcBus.start(NfcOp.Write({ NdefMessage(arrayOf()) }, true, 1, null, false, null, false, "test")); NfcBus.count = 3 }
        }
        rule.mainClock.autoAdvance = false
        rule.setContent { TagForgeTheme { AppRoot(MainActivity.NfcState.READY) } }
        rule.mainClock.advanceTimeBy(900)
        if (System.getProperty("tagforge.screenshots") == "true") {
            // le dialogue de scan est dans sa propre fenêtre : on capture la fenêtre principale, sauf pour 14
            var bmp: Bitmap? = null
            rule.activityRule.scenario.onActivity { act ->
                val v = act.window.decorView.rootView
                bmp = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888).also { v.draw(android.graphics.Canvas(it)) }
            }
            val dir = File("build/screenshots").apply { mkdirs() }
            File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
        }
    }
}
