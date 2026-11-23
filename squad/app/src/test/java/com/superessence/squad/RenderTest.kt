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

    @Test fun fightPoses() {
        val acts = listOf(Act.RUN, Act.GUARD, Act.PUNCH, Act.HIT, Act.KO, Act.VICTORY, Act.HIGHFIVE)
        val cw = 220; val ch = 300
        val pairs = listOf("rocco" to "ragnar", "tank" to "boss", "kenji" to "rex")
        val bmp = Bitmap.createBitmap(cw * 2 * acts.size, ch * pairs.size, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp); c.drawColor(Color.rgb(40, 44, 52))
        val p = Pose()
        pairs.forEachIndexed { row, (l, r) ->
            acts.forEachIndexed { col, act ->
                for (side in 0..1) {
                    val a = Archetypes.byId(if (side == 0) l else r)
                    p.act = if (side == 1 && act == Act.PUNCH) Act.HIT else act
                    p.facing = if (side == 0) 1f else -1f; p.t = 0.7f + col * 0.3f; p.punch = 1f; p.seed = 2
                    p.fx = if (p.act == Act.HIT) "BAM!" else if (p.act == Act.KO) "K.O." else null; p.fxAge = 0.15f
                    c.save(); c.translate(((col * 2 + side) * cw).toFloat(), (row * ch).toFloat())
                    MascotPainter.draw(c, a.look, p, cw.toFloat(), ch.toFloat())
                    c.restore()
                }
            }
        }
        save(bmp, "fight.png")
    }

    @Test fun gymAndParty() {
        val types = com.superessence.squad.model.PropType.entries
        val cw = 330; val ch = 330
        val bmp = Bitmap.createBitmap(cw * types.size, ch * 2 + 300, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp); c.drawColor(Color.rgb(40, 44, 52))
        val users = listOf("tank", "rocco", "sarge", "ragnar", "kenji", "boss", "marco", "zero")
        types.forEachIndexed { i, ty ->
            c.save(); c.translate((i * cw).toFloat(), 0f)
            com.superessence.squad.render.PropPainter.draw(c, ty, cw.toFloat(), ch.toFloat(), 0.8f, null)
            c.restore()
            val o = Archetypes.byId(users[i % users.size]).let { com.superessence.squad.render.Occupant(it.look, 60f, 2) }
            c.save(); c.translate((i * cw).toFloat(), ch.toFloat())
            com.superessence.squad.render.PropPainter.draw(c, ty, cw.toFloat(), ch.toFloat(), 0.8f, if (ty.gym) o else null)
            c.restore()
        }
        val acts = listOf(Act.CARRY, Act.DANCE, Act.DANCE, Act.DANCE, Act.DANCE, Act.CHEERS, Act.SQUAT, Act.PULLDOWN)
        val p = Pose()
        acts.forEachIndexed { i, a ->
            p.act = a; p.t = 1.1f + i; p.seed = i - 1; p.carry = types[i % 5]; p.fx = null
            c.save(); c.translate(i * 215f, ch * 2f)
            MascotPainter.draw(c, Archetypes.all[i].look, p, 210f, 295f)
            c.restore()
        }
        save(bmp, "events.png")
    }

    @Test fun street() {
        val types = com.superessence.squad.model.PropType.entries.filter { it.gym }
        val cw = 300; val ch = 330
        val bmp = Bitmap.createBitmap(cw * 6, ch * 4 + 220, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp); c.drawColor(Color.rgb(40, 44, 52))
        val users = listOf("tank", "rocco", "sarge", "ragnar", "kenji", "boss", "marco", "zero", "duke", "jo", "ace")
        types.forEachIndexed { i, ty ->
            val a = Archetypes.byId(users[i % users.size])
            val pr = ty.pr != null
            val o = com.superessence.squad.render.Occupant(a.look, 60f, i, grind = if (pr) 0.45f else -1f, strain = if (pr) 0.9f else 0f, label = if (pr) "PR 142,5 kg" else null)
            c.save(); c.translate(((i % 6) * cw).toFloat(), ((i / 6) * ch).toFloat())
            com.superessence.squad.render.PropPainter.draw(c, ty, cw.toFloat(), ch.toFloat(), 1.7f + i * 2.1f, o)
            c.restore()
        }
        // barre de traction : les 3 figures
        for (j in 0..2) {
            val o = com.superessence.squad.render.Occupant(Archetypes.byId("kenji").look, 60f, j)
            c.save(); c.translate((j * cw).toFloat(), ch * 2f)
            com.superessence.squad.render.PropPainter.draw(c, com.superessence.squad.model.PropType.PULLUP_BAR, cw.toFloat(), ch.toFloat(), 0.9f + (if (j == 1) 2.0f else 0f), o)
            c.restore()
        }
        val acts = listOf(Act.ONE_ARM_HS, Act.SPOTTING, Act.DRINK)
        val p = Pose()
        acts.forEachIndexed { i, a ->
            p.act = a; p.t = 1.3f + i; p.seed = i; p.carry = null; p.fx = null; p.strain = 0f; p.k = -1f
            c.save(); c.translate((3 + i) * cw.toFloat(), ch * 2f)
            MascotPainter.draw(c, Archetypes.all[i + 3].look, p, cw.toFloat(), ch.toFloat())
            c.restore()
        }
        val acts2 = listOf(Act.WIPE, Act.CHALK, Act.STRETCH, Act.HANDSTAND)
        acts2.forEachIndexed { i, a ->
            p.act = a; p.t = 1.1f + i; p.seed = i
            c.save(); c.translate(i * cw.toFloat(), ch * 3f)
            MascotPainter.draw(c, Archetypes.all[i + 6].look, p, cw.toFloat(), ch.toFloat())
            c.restore()
        }
        com.superessence.squad.model.Clutter.entries.forEachIndexed { i, cl ->
            c.save(); c.translate(i * 150f, ch * 4f); com.superessence.squad.render.PropPainter.clutter(c, cl, 140f, 100f, i); c.restore()
        }
        com.superessence.squad.model.Stain.entries.forEachIndexed { i, st ->
            c.save(); c.translate(1200f + (i % 4) * 150f, ch * 3f + (i / 4) * 150f); com.superessence.squad.render.PropPainter.stain(c, st, 140f, 140f, i * 7, 1f); c.restore()
        }
        save(bmp, "street.png")
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
        for (a in Archetypes.all) for (s in listOf(Say.TAP, Say.RANDOM, Say.EAT, Say.WAKE, Say.DRAG, Say.PET, Say.FRIEND, Say.HUNGRY, Say.NIGHT, Say.TAUNT, Say.WIN, Say.LOSE, Say.OUCH, Say.REPLY, Say.BUMPED))
            assertTrue("${a.id} $s", a.lines(s).isNotEmpty())
    }
}
