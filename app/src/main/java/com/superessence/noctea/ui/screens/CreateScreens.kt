package com.superessence.noctea.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.data.CustomMeditation
import com.superessence.noctea.data.Sounds
import com.superessence.noctea.data.TextMeditation
import com.superessence.noctea.generator.Ambience
import com.superessence.noctea.generator.DURATIONS
import com.superessence.noctea.generator.GeneratedMeditation
import com.superessence.noctea.generator.GeneratorParams
import com.superessence.noctea.generator.Goal
import com.superessence.noctea.generator.MusicIntensity
import com.superessence.noctea.generator.NarrationStyle
import com.superessence.noctea.tts.RemoteHttpTtsProvider
import com.superessence.noctea.tts.TextComposer
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.LocalNav
import com.superessence.noctea.ui.Routes
import com.superessence.noctea.ui.components.ArtIllustration
import com.superessence.noctea.ui.components.ChipRow
import com.superessence.noctea.ui.components.GlassCard
import com.superessence.noctea.ui.components.MoonLoader
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.components.NightChip
import com.superessence.noctea.ui.components.PrimaryButton
import com.superessence.noctea.ui.components.formatDuration
import com.superessence.noctea.ui.theme.Cormorant
import com.superessence.noctea.ui.theme.LocalPalette
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.io.File
import java.util.UUID

@Composable
fun CreateScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val scope = rememberCoroutineScope()
    val snap by app.store.state.collectAsState()
    var goal by rememberSaveable { mutableStateOf(Goal.SLEEP) }
    var minutes by rememberSaveable { mutableStateOf(10) }
    var ambience by rememberSaveable { mutableStateOf(Ambience.RAIN) }
    var style by rememberSaveable { mutableStateOf(NarrationStyle.SOFT) }
    var music by rememberSaveable { mutableStateOf(MusicIntensity.LIGHT) }
    var loading by remember { mutableStateOf(false) }
    var result by remember { mutableStateOf<Pair<CustomMeditation, GeneratedMeditation>?>(null) }
    var showScript by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    val ent = app.entitlements

    fun generate(seed: Long) {
        if (app.store.generationsToday() >= ent.dailyGenerationLimit()) {
            message = "Tu as atteint la limite de ${ent.dailyGenerationLimit()} créations aujourd'hui en version gratuite."
            return
        }
        loading = true
        message = null
        scope.launch {
            val params = GeneratorParams(goal, minutes, ambience, style, music, seed)
            val g = app.generator.generate(params)
            delay(1400) // le temps d'une respiration : l'animation accompagne la composition
            val meta = CustomMeditation(UUID.randomUUID().toString(), System.currentTimeMillis(), g.title, params, g.durationMs)
            app.store.edit { it.copy(customs = (listOf(meta) + it.customs).take(60)) }
            app.store.countGeneration()
            result = meta to g
            showScript = false
            loading = false
        }
    }

    NightBackground {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(TabContentPadding)) {
            Column(Modifier.statusBarsPadding().padding(horizontal = 20.dp, vertical = 12.dp)) {
                Text("Créer ma méditation", style = MaterialTheme.typography.displayMedium)
                Text("Compose une séance unique, guidée par la voix de NOCTÉA.", style = MaterialTheme.typography.bodyMedium)
            }
            Label("Mon objectif")
            ChipRow(Goal.entries, goal, { it.label }, { goal = it })
            Label("Durée")
            ChipRow(DURATIONS, minutes, { "$it min" }, { m -> minutes = m.coerceAtMost(ent.maxGenerationMinutes()) })
            Label("Ambiance")
            ChipRow(Ambience.entries, ambience, { it.label }, { ambience = it })
            Label("Style de narration")
            ChipRow(NarrationStyle.entries, style, { it.label }, { style = it })
            Text(style.description, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 24.dp, vertical = 4.dp))
            Label("Musique")
            ChipRow(MusicIntensity.entries, music, { it.label }, { music = it })
            Spacer(Modifier.height(24.dp))
            Box(Modifier.fillMaxWidth().padding(horizontal = 20.dp), contentAlignment = Alignment.Center) {
                if (loading) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        MoonLoader()
                        Text("Composition de ta méditation…", style = MaterialTheme.typography.bodyMedium, fontStyle = FontStyle.Italic)
                    }
                } else {
                    PrimaryButton("Générer ma méditation", Modifier.fillMaxWidth(), icon = { Icon(Icons.Filled.AutoAwesome, null, tint = Color(0xFF1A1430)) }) {
                        generate(System.nanoTime())
                    }
                }
            }
            message?.let { Text(it, color = p.moon, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(20.dp)) }

            AnimatedVisibility(result != null && !loading, enter = fadeIn() + slideInVertically { it / 4 }) {
                val (meta, g) = result ?: return@AnimatedVisibility
                GlassCard(Modifier.fillMaxWidth().padding(20.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.size(76.dp).clip(RoundedCornerShape(18.dp))) { ArtIllustration(g.art, Modifier.fillMaxSize()) }
                        Spacer(Modifier.width(14.dp))
                        Column(Modifier.weight(1f)) {
                            Text(g.title, style = MaterialTheme.typography.headlineSmall)
                            Text("${formatDuration(g.durationMs)} · ${g.paragraphs.size} phrases · ${style.label}", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                    Spacer(Modifier.height(14.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalAlignment = Alignment.CenterVertically) {
                        PrimaryButton("Écouter", Modifier.weight(1f), icon = { Icon(Icons.Filled.PlayArrow, null, tint = Color(0xFF1A1430)) }) {
                            app.engine.play(MediaIds.custom(meta.id), resume = false)
                            nav.navigate(Routes.PLAYER)
                        }
                        IconButton(onClick = { generate(System.nanoTime()) }) { Icon(Icons.Filled.Refresh, "Générer une variante", tint = p.accent) }
                    }
                    Spacer(Modifier.height(8.dp))
                    NightChip(if (showScript) "Masquer le script" else "Afficher le script", showScript) { showScript = !showScript }
                    if (showScript) {
                        Spacer(Modifier.height(10.dp))
                        g.paragraphs.forEach {
                            Text(it, fontFamily = Cormorant, fontSize = 18.sp, color = p.text, modifier = Modifier.padding(bottom = 8.dp))
                        }
                    }
                }
            }

            GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 8.dp), onClick = { nav.navigate(Routes.CUSTOM_TEXT) }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Filled.Edit, null, tint = p.moon)
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text("Écrire mon propre texte", style = MaterialTheme.typography.titleMedium)
                        Text("Transforme n'importe quel texte en séance audio, avec de vraies pauses.", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }

            if (snap.customs.isNotEmpty()) {
                Label("Mes créations")
                snap.customs.take(15).forEach { c ->
                    Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
                        com.superessence.noctea.ui.components.TrackRow(
                            com.superessence.noctea.ui.components.CardData(c.id, c.title, "${c.params.goal.label}", c.params.ambience.art, c.durationMs, MediaIds.custom(c.id) in snap.favorites),
                            onClick = { app.engine.play(MediaIds.custom(c.id)); nav.navigate(Routes.PLAYER) },
                            onFav = { app.store.toggleFavorite(MediaIds.custom(c.id)) },
                            modifier = Modifier.weight(1f),
                        )
                        IconButton(onClick = { app.store.edit { s -> s.copy(customs = s.customs.filter { it.id != c.id }, favorites = s.favorites - MediaIds.custom(c.id)) } }) {
                            Icon(Icons.Filled.Delete, "Supprimer", tint = p.textDim)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun Label(text: String) {
    Text(text, style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(start = 20.dp, end = 20.dp, top = 18.dp, bottom = 8.dp))
}

@Composable
fun CustomTextScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val scope = rememberCoroutineScope()
    val snap by app.store.state.collectAsState()
    var title by rememberSaveable { mutableStateOf("") }
    var text by rememberSaveable {
        mutableStateOf("Maintenant... tu peux simplement fermer les yeux.\n\nTu n'as plus rien à faire. Plus rien à prouver.\n\nLaisse doucement tes épaules se relâcher...\nPrends une grande inspiration...\nEt expire lentement...\n\n[pause 10]\nCe moment est à toi.")
    }
    var pauseFactor by rememberSaveable { mutableStateOf(1.2f) }
    var ambience by rememberSaveable { mutableStateOf("rain_soft") }
    var progress by remember { mutableStateOf<Pair<Int, Int>?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    val useRemote = snap.settings.ttsProvider == "remote" && snap.settings.remoteUrl.isNotBlank()

    SubScreen("Mon texte") {
        Text(
            "Écris ton texte : « ... » crée une pause, une ligne vide une pause longue, et [pause 10] un silence de 10 secondes.",
            style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(horizontal = 20.dp),
        )
        Spacer(Modifier.height(12.dp))
        val colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = p.accent, unfocusedBorderColor = p.cardBorder, cursorColor = p.moon)
        OutlinedTextField(title, { title = it.take(60) }, Modifier.fillMaxWidth().padding(horizontal = 20.dp), placeholder = { Text("Titre") }, singleLine = true, shape = RoundedCornerShape(16.dp), colors = colors)
        Spacer(Modifier.height(10.dp))
        OutlinedTextField(text, { text = it.take(6000) }, Modifier.fillMaxWidth().heightIn(min = 220.dp).padding(horizontal = 20.dp).imePadding(), shape = RoundedCornerShape(16.dp), colors = colors)
        LabeledSlider("Longueur des pauses", pauseFactor, "x${"%.1f".format(pauseFactor)}", 0.6f..2.5f) { pauseFactor = it }
        Text("Ambiance", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(horizontal = 20.dp, vertical = 6.dp))
        ChipRow(listOf("") + Sounds.all.map { it.id }, ambience, { if (it.isEmpty()) "Aucune" else Sounds.get(it)!!.name }, { ambience = it })
        Spacer(Modifier.height(12.dp))
        GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp), padding = 14.dp) {
            Text("Voix utilisée : ${if (useRemote) "Serveur vocal personnalisé" else "Voix de l'appareil (Android)"}", style = MaterialTheme.typography.titleSmall)
            Text(
                if (useRemote) "La qualité dépend du serveur configuré." else "Les textes libres utilisent le moteur vocal de ton téléphone : la qualité dépend de l'appareil. La voix premium de NOCTÉA est utilisée pour la bibliothèque et le générateur.",
                style = MaterialTheme.typography.bodySmall,
            )
            Spacer(Modifier.height(8.dp))
            NightChip("Réglages de la voix", false) { nav.navigate(Routes.VOICE) }
        }
        Spacer(Modifier.height(18.dp))
        val pr = progress
        if (pr != null) {
            Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                MoonLoader(size = 80.dp)
                Text("Synthèse vocale… ${pr.first}/${pr.second}", style = MaterialTheme.typography.bodyMedium)
            }
        } else {
            PrimaryButton("Créer l'audio", Modifier.fillMaxWidth().padding(horizontal = 20.dp), enabled = text.isNotBlank()) {
                error = null
                val fragments = TextComposer.split(text, pauseFactor)
                if (fragments.isEmpty()) { error = "Le texte est vide."; return@PrimaryButton }
                progress = 0 to fragments.size
                scope.launch {
                    val id = UUID.randomUUID().toString()
                    val dir = File(app.filesDir, "tts/$id")
                    try {
                        val s = app.store.value.settings
                        val provider = if (useRemote) RemoteHttpTtsProvider(s.remoteUrl, s.remoteApiKey, s.remoteVoice, 1f) else app.systemTts.also {
                            it.rate = s.ttsRate; it.pitch = s.ttsPitch; it.voiceName = s.systemVoiceName
                        }
                        val clips = TextComposer.synthesize(provider, fragments, dir) { i, n -> progress = i to n }
                        val tm = TextMeditation(
                            id, System.currentTimeMillis(), title.ifBlank { fragments.first().text.take(40) }, text,
                            provider.id, clips, ambience, 0.4f, clips.sumOf { it.ms + it.pauseAfterMs } + 5500,
                        )
                        app.store.edit { it.copy(texts = listOf(tm) + it.texts) }
                        app.engine.play(MediaIds.text(id), resume = false)
                        nav.navigate(Routes.PLAYER) { popUpTo(Routes.CREATE) }
                    } catch (e: Exception) {
                        dir.deleteRecursively()
                        error = e.message ?: "La synthèse vocale a échoué."
                    } finally {
                        progress = null
                    }
                }
            }
        }
        error?.let { Text(it, color = Color(0xFFFF9A9A), style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(20.dp)) }

        if (snap.texts.isNotEmpty()) {
            Text("Mes textes enregistrés", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(20.dp))
            snap.texts.forEach { t ->
                Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
                    com.superessence.noctea.ui.components.TrackRow(
                        com.superessence.noctea.ui.components.CardData(t.id, t.title, "Mon texte", "moon", t.durationMs, MediaIds.text(t.id) in snap.favorites),
                        onClick = { app.engine.play(MediaIds.text(t.id)); nav.navigate(Routes.PLAYER) },
                        onFav = { app.store.toggleFavorite(MediaIds.text(t.id)) },
                        modifier = Modifier.weight(1f),
                    )
                    IconButton(onClick = {
                        File(app.filesDir, "tts/${t.id}").deleteRecursively()
                        app.store.edit { s -> s.copy(texts = s.texts.filter { it.id != t.id }, favorites = s.favorites - MediaIds.text(t.id)) }
                    }) { Icon(Icons.Filled.Delete, "Supprimer", tint = p.textDim) }
                }
            }
        }
        Spacer(Modifier.height(40.dp))
    }
}
