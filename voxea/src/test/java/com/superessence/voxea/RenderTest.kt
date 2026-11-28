package com.superessence.voxea

import com.superessence.voxea.core.Line
import com.superessence.voxea.core.Renderer
import com.superessence.voxea.core.LocalConverter
import com.superessence.voxea.core.Audio
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.core.Wav
import com.superessence.voxea.data.Project
import com.superessence.voxea.data.ProjectStore
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.nio.file.Files
import java.util.zip.ZipFile
import kotlin.math.PI
import kotlin.math.sin

class RenderTest {
    private fun tone(sec: Double, lead: Double = 0.0) = FloatArray(((sec + 2 * lead) * 44100).toInt()) { i ->
        val t = i / 44100.0; if (t < lead || t > lead + sec) 0.0005f else (0.3 * sin(2 * PI * 220 * t)).toFloat()
    }

    @Test fun rendersRecordedLinesAndSkipsOthers() {
        val root = Files.createTempDirectory("voxea").toFile()
        val store = ProjectStore(root)
        val p = Project.create("Scène 1", "", listOf(Line(0, "LÉA", "a", VoiceType.FEMME), Line(1, "MARC", "b"), Line(2, "LÉA", "c")))
        store.save(p)
        Wav.write(store.recFile(p, 0), tone(0.5), 44100)
        Wav.write(store.recFile(p, 2), tone(0.5), 44100)
        val r = Renderer(store).render(p, LocalConverter)
        assertEquals(listOf("01_LEA.mp3", "03_LEA.mp3"), r.files.map { it.name })
        assertEquals(listOf(1), r.skipped.map { it.index })
        assertTrue(r.full!!.length() > 1000)
        assertEquals("00_DIALOGUE_COMPLET.mp3", r.full!!.name)
        val zip = Renderer(store).zip(p, r, File(root, "z.zip"))
        assertEquals(3, ZipFile(zip).size())
    }

    @Test fun trimKeepsSpeechAndSmallMargin() {
        val x = tone(0.4, lead = 1.0)
        val y = Audio.trimSilence(x, 44100)
        assertTrue("len=${y.size}", y.size in (0.4 * 44100).toInt()..(0.8 * 44100).toInt())
        assertEquals(0, Audio.trimSilence(FloatArray(0), 44100).size)
        assertEquals(100, Audio.trimSilence(FloatArray(100), 44100).size) // silence total : inchangé
    }

    @Test fun resample() {
        assertEquals(44100, Audio.resample(FloatArray(24000), 24000, 44100).size)
    }
}
