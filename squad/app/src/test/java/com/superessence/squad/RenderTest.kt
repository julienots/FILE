package com.superessence.squad

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import com.superessence.squad.engine.Life
import com.superessence.squad.engine.Phase
import com.superessence.squad.model.Act
import com.superessence.squad.model.Archetypes
import com.superessence.squad.model.Say
import com.superessence.squad.model.Schedule
import com.superessence.squad.render.MascotPainter
import com.superessence.squad.render.Pose
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.GraphicsMode
import java.io.File
import java.util.Calendar

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class RenderTest {
    private val out = File("build/renders").apply { mkdirs() }

    private fun save(b: Bitmap, name: String) = File(out, name).outputStream().use { b.compress(Bitmap.CompressFormat.PNG, 100, it) }

    @Test fun allArchetypesAllActs() {
        val acts = listOf(Act.IDLE, Act.WALK, Act.EAT, Act.TRAIN, Act.FLEX, Act.PHONE, Act.COFFEE, Act.WAVE, Act.SLEEP, Act.YAWN, Act.DRAGGED, Act.PUSHUP)
        val cw = 200; val ch = 280
        val bmp = Bitmap.createBitmap(cw * acts.size, ch * Archetypes.all.size, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp)
        c.drawColor(Color.rgb(40, 44, 52))
        val pose = Pose()
        Archetypes.all.forEachIndexed { row, a ->
            acts.forEachIndexed { col, act ->
                pose.act = act; pose.t = 1.3f + col * 0.4f; pose.fitness = 30f; pose.seed = 3
                c.save(); c.translate((col * cw).toFloat(), (row * ch).toFloat())
                MascotPainter.draw(c, a.look, pose, cw.toFloat(), ch.toFloat())
                c.restore()
            }
        }
        save(bmp, "grid.png")
        // Galerie « vitrine » : un par archétype, grand
        val big = Bitmap.createBitmap(6 * 300, 2 * 420, Bitmap.Config.ARGB_8888)
        val bc = Canvas(big); bc.drawColor(Color.rgb(16, 16, 16))
        Archetypes.all.forEachIndexed { i, a ->
            pose.act = Act.IDLE; pose.t = 0.5f; pose.fitness = if (a.id == "tank") 90f else 40f
            bc.save(); bc.translate((i % 6) * 300f, (i / 6) * 420f)
            MascotPainter.draw(bc, a.look, pose, 300f, 400f)
            val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE; textSize = 26f; textAlign = Paint.Align.CENTER }
            bc.drawText(a.name, 150f, 412f, tp)
            bc.restore()
        }
        save(big, "lineup.png")
    }

    @Test fun phasesFollowSchedule() {
        val s = Schedule(wake = 7 * 60, wakeWeekend = 9 * 60, bed = 23 * 60, breakfast = 7 * 60 + 30, lunch = 12 * 60 + 30, dinner = 20 * 60, workout = 18 * 60)
        val cal = Calendar.getInstance().apply { set(2026, Calendar.NOVEMBER, 24) } // mardi
        fun at(h: Int, m: Int): Phase { cal.set(Calendar.HOUR_OF_DAY, h); cal.set(Calendar.MINUTE, m); return Life.phaseAt(s, cal) }
        assertEquals(Phase.SLEEP, at(3, 0))
        assertEquals(Phase.SLEEP, at(23, 10))
        assertEquals(Phase.WAKING, at(7, 5))
        assertEquals(Phase.BREAKFAST, at(7, 40))
        assertEquals(Phase.LUNCH, at(12, 45))
        assertEquals(Phase.WORKOUT, at(18, 20))
        assertEquals(Phase.DINNER, at(20, 10))
        assertEquals(Phase.WIND_DOWN, at(22, 45))
        assertEquals(Phase.FREE, at(15, 0))
        cal.set(2026, Calendar.NOVEMBER, 29) // dimanche
        assertEquals(Phase.SLEEP, at(8, 0))
    }

    @Test fun everyoneHasLines() {
        for (a in Archetypes.all) for (s in listOf(Say.TAP, Say.RANDOM, Say.EAT, Say.WAKE, Say.DRAG, Say.PET, Say.FRIEND, Say.HUNGRY, Say.NIGHT))
            assertTrue("${a.id} $s", a.lines(s).isNotEmpty())
    }
}
