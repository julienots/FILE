package com.superessence.voxea.manual

import com.superessence.voxea.core.Mp3
import com.superessence.voxea.core.VoiceDsp
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.core.Wav
import com.superessence.voxea.core.defaultSettings
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.File

/** Rendu manuel : -Dvoxea.sample=/chemin/voix.wav (ignoré sinon). */
class RenderSamples {
    @Test fun render() {
        val path = System.getProperty("voxea.sample") ?: System.getenv("VOXEA_SAMPLE")
        assumeTrue(path != null && File(path).exists())
        val (x, sr) = Wav.read(File(path!!))
        for (t in VoiceType.entries) {
            val t0 = System.nanoTime()
            val y = VoiceDsp.process(x, sr, t.defaultSettings())
            val ms = (System.nanoTime() - t0) / 1_000_000
            Wav.write(File(path.replace(".wav", "_${t.name}.wav")), y, sr)
            File(path.replace(".wav", "_${t.name}.mp3")).outputStream().use { Mp3.encode(y, sr, it) }
            println("${t.name}: ${x.size / sr.toDouble()} s in $ms ms")
        }
    }
}
