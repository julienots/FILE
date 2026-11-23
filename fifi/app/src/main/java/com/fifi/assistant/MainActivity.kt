package com.fifi.assistant

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.Gravity
import android.view.View
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.*

class MainActivity : Activity(), FifiEngine.Listener {

    private lateinit var face: FaceView
    private lateinit var status: TextView
    private lateinit var said: TextView
    private lateinit var heard: TextView
    private lateinit var talk: Button
    private lateinit var wakeSwitch: Switch
    private var pendingAsk = false
    private val prefs by lazy { getSharedPreferences("fifi", MODE_PRIVATE) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        FifiEngine.init(this)
        window.statusBarColor = 0xFF1E1233.toInt()
        window.navigationBarColor = 0xFF120A20.toInt()
        setContentView(buildUi())
        askPermissions()
        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(i: Intent?) {
        if (i?.action == Intent.ACTION_ASSIST || i?.action == Intent.ACTION_VOICE_COMMAND) {
            if (hasMic()) FifiEngine.askNow() else pendingAsk = true
        }
    }

    override fun onResume() {
        super.onResume()
        FifiEngine.activityVisible = true
        FifiEngine.addListener(this)
        wakeSwitch.isChecked = prefs.getBoolean("wake", false)
    }

    override fun onPause() {
        FifiEngine.activityVisible = false
        FifiEngine.removeListener(this)
        super.onPause()
    }

    // ------------------------------------------------------------------ UI

    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()

    private fun buildUi(): View {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            background = GradientDrawable(
                GradientDrawable.Orientation.TOP_BOTTOM,
                intArrayOf(0xFF1E1233.toInt(), 0xFF3A1F5C.toInt(), 0xFF120A20.toInt())
            )
            setPadding(dp(20), dp(24), dp(20), dp(16))
        }
        root.addView(TextView(this).apply {
            text = "Fifi"
            textSize = 40f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
            setTextColor(0xFFFFD36E.toInt())
            gravity = Gravity.CENTER
        })
        root.addView(TextView(this).apply {
            text = "L'assistante qui a la flemme"
            textSize = 15f
            setTextColor(0xB3FFFFFF.toInt())
            gravity = Gravity.CENTER
        })

        face = FaceView(this)
        root.addView(face, LinearLayout.LayoutParams(MATCH_PARENT, 0, 1f))

        status = TextView(this).apply {
            textSize = 16f; setTextColor(0xFF9BE7FF.toInt()); gravity = Gravity.CENTER
        }
        root.addView(status)
        heard = TextView(this).apply {
            textSize = 14f; setTextColor(0x99FFFFFF.toInt()); gravity = Gravity.CENTER
            setPadding(0, dp(6), 0, 0)
        }
        root.addView(heard)
        said = TextView(this).apply {
            textSize = 19f; setTextColor(Color.WHITE); gravity = Gravity.CENTER
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
            setPadding(dp(8), dp(10), dp(8), dp(14))
            minLines = 2
        }
        root.addView(said)

        talk = Button(this).apply {
            text = "🎤  Parler à Fifi"
            textSize = 18f
            isAllCaps = false
            setTextColor(0xFF2B1B3D.toInt())
            background = GradientDrawable().apply {
                cornerRadius = dp(32).toFloat(); setColor(0xFFFFD36E.toInt())
            }
            setOnClickListener {
                if (!hasMic()) { askPermissions(); return@setOnClickListener }
                if (FifiEngine.state == FifiEngine.State.SPEAKING || FifiEngine.state == FifiEngine.State.LISTENING) FifiEngine.stopTalking()
                else FifiEngine.askNow()
            }
        }
        root.addView(talk, LinearLayout.LayoutParams(MATCH_PARENT, dp(64)))

        wakeSwitch = Switch(this).apply {
            text = "Écoute permanente « Dis Fifi »"
            textSize = 15f
            setTextColor(Color.WHITE)
            setPadding(0, dp(14), 0, 0)
            setOnCheckedChangeListener { _, on ->
                if (on == prefs.getBoolean("wake", false) && on == FifiEngine.wakeMode) return@setOnCheckedChangeListener
                if (on && !hasMic()) { isChecked = false; askPermissions(); return@setOnCheckedChangeListener }
                prefs.edit().putBoolean("wake", on).apply()
                if (on) FifiService.start(this@MainActivity) else FifiService.stop(this@MainActivity)
            }
        }
        root.addView(wakeSwitch, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))

        root.addView(Switch(this).apply {
            text = "Couper le « bip » du micro pendant l'écoute"
            textSize = 14f
            setTextColor(0xCCFFFFFF.toInt())
            isChecked = FifiEngine.muteBeep
            setPadding(0, dp(8), 0, 0)
            setOnCheckedChangeListener { _, on ->
                FifiEngine.muteBeep = on; prefs.edit().putBoolean("muteBeep", on).apply()
            }
        }, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))

        // Niveau de flemme
        val flemmeLabel = TextView(this).apply {
            textSize = 14f; setTextColor(0xCCFFFFFF.toInt()); setPadding(0, dp(10), 0, 0)
        }
        root.addView(flemmeLabel)
        val levels = listOf("Motivée (rare)" to 0.03f, "Normale" to 0.15f, "Grosse flemme" to 0.35f, "Flemme absolue" to 0.6f)
        root.addView(SeekBar(this).apply {
            max = levels.size - 1
            val cur = prefs.getFloat("flemme", 0.15f)
            progress = levels.indexOfFirst { it.second == cur }.coerceAtLeast(1)
            fun show(i: Int) { flemmeLabel.text = "Niveau de flemme : ${levels[i].first}" }
            show(progress)
            FifiEngine.setFlemme(levels[progress].second)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, fromUser: Boolean) {
                    show(p); FifiEngine.setFlemme(levels[p].second)
                    prefs.edit().putFloat("flemme", levels[p].second).apply()
                }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        }, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))

        root.addView(TextView(this).apply {
            text = "Faire de Fifi l'assistant du téléphone (appui long sur Accueil) ›"
            textSize = 13f
            setTextColor(0xFFFFD36E.toInt())
            setPadding(0, dp(10), 0, 0)
            setOnClickListener {
                val i = Intent(Settings.ACTION_VOICE_INPUT_SETTINGS)
                runCatching { startActivity(i) }.onFailure {
                    runCatching { startActivity(Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS)) }
                }
            }
        })
        root.addView(TextView(this).apply {
            text = "Essaie : « Dis Fifi, quelle heure il est ? » · « raconte une blague » · « allume la lampe » · « on est quel jour ? » · « mets de la musique » · « pile ou face » · « c'est quoi un ornithorynque ? »"
            textSize = 12f
            setTextColor(0x80FFFFFF.toInt())
            setPadding(0, dp(8), 0, 0)
        })
        return root
    }

    // --------------------------------------------------------------- moteur

    override fun onState(state: FifiEngine.State) {
        face.state = state
        status.text = when (state) {
            FifiEngine.State.IDLE -> "Fifi dort. Appuie sur le bouton (ou dis « Dis Fifi » si l'écoute est activée)."
            FifiEngine.State.WAITING_WAKE -> "Elle fait semblant de dormir… dis « Dis Fifi »"
            FifiEngine.State.LISTENING -> "Fifi t'écoute (à moitié)…"
            FifiEngine.State.SPEAKING -> "Fifi parle…"
        }
        talk.text = when (state) {
            FifiEngine.State.LISTENING, FifiEngine.State.SPEAKING -> "✋  Stop"
            else -> "🎤  Parler à Fifi"
        }
    }

    override fun onHeard(text: String) { heard.text = if (text.isBlank()) "" else "Toi : « $text »" }
    override fun onReply(text: String) { said.text = text }

    // ----------------------------------------------------------- permissions

    private fun hasMic() = checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED

    private fun askPermissions() {
        val perms = mutableListOf(Manifest.permission.RECORD_AUDIO)
        if (Build.VERSION.SDK_INT >= 33) perms += Manifest.permission.POST_NOTIFICATIONS
        val missing = perms.filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) requestPermissions(missing.toTypedArray(), 1)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (hasMic() && pendingAsk) { pendingAsk = false; FifiEngine.askNow() }
        if (hasMic() && prefs.getBoolean("wake", false)) FifiService.start(this)
    }
}
