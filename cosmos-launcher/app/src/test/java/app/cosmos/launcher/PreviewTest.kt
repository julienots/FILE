package app.cosmos.launcher

import android.graphics.Bitmap
import android.graphics.Canvas
import android.view.KeyEvent
import android.view.View
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File
import java.io.FileOutputStream

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "w960dp-h540dp-xhdpi")
class PreviewTest {
    private fun shot(v: LauncherView, name: String) {
        val bmp = Bitmap.createBitmap(1920, 1080, Bitmap.Config.ARGB_8888)
        v.render(Canvas(bmp))
        val dir = File(System.getProperty("cosmos.out")!!).apply { mkdirs() }
        FileOutputStream(File(dir, "$name.png")).use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
    }

    private fun key(v: LauncherView, code: Int) {
        v.onKeyDown(code, KeyEvent(KeyEvent.ACTION_DOWN, code))
    }

    private fun run(v: LauncherView, secs: Float) { var t = 0f; while (t < secs) { v.update(1f / 30f); t += 1f / 30f } }

    @Test
    fun capture() {
        assumeTrue(System.getProperty("cosmos.shots") == "true")
        val ctx = RuntimeEnvironment.getApplication()
        val v = LauncherView(ctx)
        v.layout(0, 0, 1920, 1080)
        val rows = Catalog.rows()
        rows.forEach { r -> r.tiles.forEachIndexed { i, t -> if (t.system == null && i % 3 != 2) t.fakeInstalled = true } }
        val all = (1..30).map { Tile("a$it", "Application $it", "", 0xFF7C4DFF.toInt() + it * 997, 0xFF2A1670.toInt(), "A") .also { t -> t.fakeInstalled = true } }
        v.setContent(rows, all)
        run(v, 0.5f); shot(v, "01_intro_warp")
        run(v, 3f); shot(v, "02_home")
        key(v, KeyEvent.KEYCODE_DPAD_RIGHT); key(v, KeyEvent.KEYCODE_DPAD_RIGHT); run(v, 0.12f); shot(v, "03_moving")
        run(v, 2f); shot(v, "04_netflix_focus")
        key(v, KeyEvent.KEYCODE_DPAD_DOWN); run(v, 2.2f); shot(v, "05_row2")
        key(v, KeyEvent.KEYCODE_DPAD_DOWN); key(v, KeyEvent.KEYCODE_DPAD_DOWN); run(v, 2.2f); shot(v, "06_system")
        key(v, KeyEvent.KEYCODE_MENU); run(v, 2f); shot(v, "07_all_apps")
        key(v, KeyEvent.KEYCODE_MENU); run(v, 1.5f)
        key(v, KeyEvent.KEYCODE_DPAD_UP); key(v, KeyEvent.KEYCODE_DPAD_UP); key(v, KeyEvent.KEYCODE_DPAD_UP); run(v, 1.5f)
        key(v, KeyEvent.KEYCODE_DPAD_CENTER); run(v, 0.35f); shot(v, "08_launch")
        v.resetAfterLaunch(); run(v, 50f); shot(v, "09_idle")
    }
}
