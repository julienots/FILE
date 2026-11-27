package com.superessence.bodymotion

import android.graphics.Bitmap
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import com.superessence.bodymotion.anim.AnimationClip
import com.superessence.bodymotion.data.ProjectInfo
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.rig.HumanBone.*
import com.superessence.bodymotion.ui.AppNav
import com.superessence.bodymotion.ui.Routes
import com.superessence.bodymotion.ui.theme.BmTheme
import com.superessence.bodymotion.ui.theme.C
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File
import kotlin.math.PI
import kotlin.math.sin

/** Parcourt chaque écran (détecte les plantages) et enregistre des captures avec -Pscreenshots=true (build/screenshots). */
@RunWith(ParameterizedRobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w393dp-h852dp-xxhdpi")
class ScreenshotTest(private val name: String, private val route: String, private val atMs: Long) {
    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters(name = "{0}")
        fun shots() = listOf(
            arrayOf("01_intro_super_essence", Routes.INTRO, 2600L),
            arrayOf("02_intro_bodymotion", Routes.INTRO, 5400L),
            arrayOf("03_accueil", Routes.HOME, 800L),
            arrayOf("04_personnages", Routes.CHARACTERS, 800L),
            arrayOf("05_fiche_personnage", "character", 800L),
            arrayOf("06_bibliotheque", Routes.LIBRARY, 1500L),
            arrayOf("07_editeur", "editor", 1200L),
            arrayOf("08_export", "export", 800L),
            arrayOf("09_projet", "project", 800L),
            arrayOf("10_parametres", Routes.SETTINGS, 800L),
        )
    }

    @get:Rule val rule = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    /** Animation de démonstration (marche) générée directement sur le squelette du mannequin. */
    private fun demoClip(store: com.superessence.bodymotion.data.Store): AnimationClip {
        val chInfo = store.characters.value.first { it.isSample }
        val ch = store.load(chInfo.id)!!
        val sk = ch.skeleton; val map = ch.boneMap
        val n = 48
        val poses = (0 until n).map { f ->
            val ph = f * 2 * PI / n
            val local = Array(sk.nodes.size) { sk.nodes[it].rest.r }
            fun rot(b: HumanBone, axis: Vec3, deg: Double) { val i = map[b]!!; local[i] = (local[i] * Quat.axisAngle(axis, Math.toRadians(deg))).normalized() }
            rot(LeftUpperLeg, Vec3.X, -28 * sin(ph)); rot(RightUpperLeg, Vec3.X, 28 * sin(ph))
            rot(LeftLowerLeg, Vec3.X, 30 + 25 * sin(ph + 1)); rot(RightLowerLeg, Vec3.X, 30 - 25 * sin(ph + 1))
            rot(LeftUpperArm, Vec3.Z, -72.0); rot(RightUpperArm, Vec3.Z, 72.0)
            rot(LeftUpperArm, Vec3.Y, 25 * sin(ph)); rot(RightUpperArm, Vec3.Y, 25 * sin(ph))
            rot(LeftLowerArm, Vec3.Y, 35.0); rot(RightLowerArm, Vec3.Y, -35.0)
            rot(Spine, Vec3.Y, 6 * sin(ph))
            com.superessence.bodymotion.retarget.SkeletalPose(local, sk.nodes[map[Hips]!!].rest.t + Vec3(0.0, -0.02 * sin(2 * ph), 0.0), map[Hips]!!)
        }
        return AnimationClip.fromPoses("demo-walk", "Marche_demo", chInfo.id, 30.0, sk, map, poses).also {
            it.category = "Walk"; it.interpolatedFrames.addAll(20..23)
        }
    }

    @Test
    fun capture() {
        val store = BodyMotionApp.store
        val clip = demoClip(store)
        store.saveClip(clip)
        val sample = store.characters.value.first { it.isSample }
        val project = ProjectInfo("p1", "Chevalier — Locomotion", sample.id, listOf(clip.id), System.currentTimeMillis(), "Unity")
        store.saveProject(project)
        if (route != Routes.INTRO) store.updateSettings { it.copy(introSeen = true) }
        val start = when (route) {
            "character" -> "character/${sample.id}"
            "editor" -> Routes.editor(clip.id)
            "export" -> Routes.export(listOf(clip.id))
            "project" -> Routes.project(project.id)
            else -> route
        }
        rule.mainClock.autoAdvance = false
        rule.setContent { BmTheme { Box(Modifier.fillMaxSize().background(C.Bg)) { AppNav(start) } } }
        rule.mainClock.advanceTimeBy(atMs)
        if (System.getProperty("bm.screenshots") != "true") return
        var bmp: Bitmap? = null
        rule.activityRule.scenario.onActivity { act ->
            val v = act.window.decorView.rootView
            val b = Bitmap.createBitmap(v.width, v.height, Bitmap.Config.ARGB_8888)
            v.draw(android.graphics.Canvas(b)); bmp = b
        }
        val dir = File("build/screenshots").apply { mkdirs() }
        File(dir, "$name.png").outputStream().use { bmp!!.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }
}

