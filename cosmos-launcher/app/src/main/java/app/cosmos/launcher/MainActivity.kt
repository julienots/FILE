package app.cosmos.launcher

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.view.WindowManager
import android.widget.Toast
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private lateinit var view: LauncherView
    private val io = Executors.newSingleThreadExecutor()
    private var rows: List<Row> = Catalog.rows()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        view = LauncherView(this)
        view.onLaunch = { launch(it) }
        setContentView(view)
        view.requestFocus()
        view.setContent(rows, emptyList())
        immersive()
    }

    override fun onResume() {
        super.onResume()
        immersive()
        view.resetAfterLaunch()
        refresh()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        // bouton Home pressé alors que le menu est déjà affiché
        view.goHome()
    }

    private fun refresh() {
        io.execute {
            val pm = packageManager
            rows.forEach { r -> r.tiles.forEach { if (it.system == null) Catalog.resolve(pm, it) } }
            val all = Catalog.allApps(this)
            view.post { view.setContent(rows, all) }
        }
    }

    private fun launch(t: Tile) {
        val intent: Intent? = when {
            t.system != null -> Catalog.systemIntent(t.system)
            t.launchIntent != null -> t.launchIntent
            t.storePkg != null -> Catalog.storeIntent(t.storePkg!!)
            else -> null
        }
        try {
            if (intent == null) throw ActivityNotFoundException()
            startActivity(intent)
        } catch (e: Exception) {
            Toast.makeText(this, "Impossible d'ouvrir ${t.label}", Toast.LENGTH_SHORT).show()
            view.resetAfterLaunch()
        }
    }

    @Suppress("DEPRECATION")
    private fun immersive() {
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        // un launcher ne se ferme jamais : on laisse la vue gérer (touche BACK déjà consommée)
    }

    override fun onDestroy() {
        super.onDestroy()
        io.shutdown()
    }
}
