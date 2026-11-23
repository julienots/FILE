package com.superessence.squad.ui

import android.app.TimePickerDialog
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Bedtime
import androidx.compose.material.icons.filled.Brush
import androidx.compose.material.icons.filled.ChatBubble
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.FitnessCenter
import androidx.compose.material.icons.filled.PanTool
import androidx.compose.material.icons.filled.Restaurant
import androidx.compose.material.icons.filled.Schedule
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.squad.engine.Life
import com.superessence.squad.engine.Phase
import com.superessence.squad.engine.Squad
import com.superessence.squad.engine.SquadState
import com.superessence.squad.model.Accessory
import com.superessence.squad.model.Act
import com.superessence.squad.model.Archetypes
import com.superessence.squad.model.Beard
import com.superessence.squad.model.HairStyle
import com.superessence.squad.model.Look
import com.superessence.squad.model.Mascot
import com.superessence.squad.model.Outfit
import com.superessence.squad.model.Palette
import com.superessence.squad.model.Say
import com.superessence.squad.model.fmtMin
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

// =====================================================================================
// Barre du haut
// =====================================================================================

@Composable
private fun TopBar(title: String, onBack: (() -> Unit)? = null, action: @Composable () -> Unit = {}) {
    Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        if (onBack != null) {
            Box(Modifier.size(44.dp).clip(CircleShape).clickable(onClick = onBack), contentAlignment = Alignment.Center) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, "Retour", tint = C.text)
            }
        } else Spacer(Modifier.width(8.dp))
        Text(title, style = H2, modifier = Modifier.weight(1f).padding(start = 4.dp))
        action()
    }
}

@Composable
private fun Card(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Box(modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(C.card).border(1.dp, C.line, RoundedCornerShape(20.dp)).padding(16.dp)) { content() }
}

@Composable
private fun Wordmark() {
    Row(verticalAlignment = Alignment.CenterVertically) {
        StudioMark(30.dp)
        Spacer(Modifier.width(10.dp))
        Text("SQUAD", style = H1.copy(fontSize = 28.sp))
        Text(".", style = H1.copy(fontSize = 28.sp, color = C.yellow))
    }
}

// =====================================================================================
// Accueil
// =====================================================================================

@Composable
fun HomeScreen(
    state: SquadState, overlayAllowed: Boolean,
    onOpen: (String) -> Unit, onRecruit: () -> Unit, onRhythm: () -> Unit, onOverlay: (Boolean) -> Unit,
) {
    val phase = Life.phaseNow(state.schedule)
    val t = animTime()
    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
        item {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f)) { Wordmark() }
                Box(Modifier.size(44.dp).clip(CircleShape).background(C.card).clickable(onClick = onRhythm), contentAlignment = Alignment.Center) {
                    Icon(Icons.Filled.Schedule, "Mon rythme", tint = C.yellow)
                }
            }
        }
        item {
            val now = SimpleDateFormat("HH:mm", Locale.FRANCE).format(Date())
            Card {
                Column {
                    Text("IL EST $now", style = Label)
                    Spacer(Modifier.height(4.dp))
                    Text(phaseLine(phase), style = H2)
                    Spacer(Modifier.height(2.dp))
                    Text(Life.nextUp(state.schedule), style = Small)
                    Spacer(Modifier.height(10.dp))
                    Text("Réglé sur ton rythme : lever ${fmtMin(state.schedule.wake)}, dodo ${fmtMin(state.schedule.bed)}", style = Small.copy(color = C.yellow), modifier = Modifier.clickable(onClick = onRhythm))
                }
            }
        }
        item {
            val on = state.settings.overlayOn && overlayAllowed
            Card {
                Column {
                    Text(if (on) "LA SQUAD EST EN LIBERTÉ" else "SUR TON ÉCRAN", style = Label.copy(color = if (on) C.green else C.dim))
                    Spacer(Modifier.height(6.dp))
                    Text(
                        if (on) "Ils vivent par-dessus tes applis. Fais-les glisser où tu veux, lâche-les sur la croix pour les ranger. Appui long = ouvrir sa fiche."
                        else "Lâche tes gars sur ton écran : ils marchent, mangent, dorment et parlent, même appli fermée.",
                        style = Small,
                    )
                    Spacer(Modifier.height(12.dp))
                    if (on) {
                        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                            GhostButton("Tout rappeler", Icons.Filled.Visibility, Modifier.weight(1f)) {
                                Squad.edit { s -> s.copy(mascots = s.mascots.map { it.copy(onScreen = true) }) }
                            }
                            GhostButton("Arrêter", Icons.Filled.VisibilityOff, Modifier.weight(1f), color = C.red) { onOverlay(false) }
                        }
                    } else {
                        YellowButton(
                            if (overlayAllowed) "Lâcher la squad sur l'écran" else "Autoriser l'affichage à l'écran",
                            Icons.Filled.PanTool, Modifier.fillMaxWidth(), enabled = state.mascots.isNotEmpty(),
                        ) { onOverlay(true) }
                        if (!overlayAllowed) {
                            Spacer(Modifier.height(8.dp))
                            Text("Android va te demander « Afficher par-dessus les autres applis » : active-le pour SQUAD. puis reviens.", style = Small.copy(fontSize = 12.sp))
                        }
                    }
                }
            }
        }
        item {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("MA SQUAD · ${state.mascots.size}/${Squad.MAX}", style = Label, modifier = Modifier.weight(1f))
            }
        }
        if (state.mascots.isEmpty()) {
            item {
                Card {
                    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.fillMaxWidth()) {
                        val a = Archetypes.all[((t / 2.5f).toInt()) % Archetypes.all.size]
                        MascotCanvas(a.look, Act.WAVE, Modifier.size(120.dp, 160.dp), seed = 3)
                        Text("Ta squad est vide", style = H2)
                        Spacer(Modifier.height(4.dp))
                        Text("Recrute ton premier gars. Chacun a sa gueule, son caractère et sa vie.", style = Small, textAlign = TextAlign.Center)
                        Spacer(Modifier.height(14.dp))
                        YellowButton("Recruter", Icons.Filled.Add, onClick = onRecruit)
                    }
                }
            }
        }
        items(state.mascots, key = { it.id }) { m -> MascotRow(m, phase, t) { onOpen(m.id) } }
        if (state.mascots.isNotEmpty() && state.mascots.size < Squad.MAX) {
            item { GhostButton("Recruter un nouveau gars", Icons.Filled.Add, Modifier.fillMaxWidth(), color = C.yellow, onClick = onRecruit) }
        }
        item {
            Text("Une création SuperEssence Studio", style = Small.copy(fontSize = 11.sp), textAlign = TextAlign.Center, modifier = Modifier.fillMaxWidth().padding(top = 12.dp))
        }
    }
}

private fun phaseLine(p: Phase) = when (p) {
    Phase.SLEEP -> "Tout le monde dort. Chut."
    Phase.WAKING -> "Réveil de la troupe, café en main"
    Phase.BREAKFAST -> "C'est l'heure du petit-déj"
    Phase.LUNCH -> "À table : déjeuner"
    Phase.DINNER -> "À table : dîner"
    Phase.WORKOUT -> "Séance d'entraînement"
    Phase.WIND_DOWN -> "Ça bâille, bientôt dodo"
    Phase.FREE -> "Temps libre"
}

@Composable
private fun MascotRow(m: Mascot, phase: Phase, t: Float, onClick: () -> Unit) {
    val act = previewAct(m, phase, t)
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(C.card).border(1.dp, C.line, RoundedCornerShape(20.dp))
            .clickable(onClick = onClick).padding(10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(88.dp, 118.dp).clip(RoundedCornerShape(14.dp)).background(C.card2)) {
            MascotCanvas(m.look, act, Modifier.fillMaxSize().padding(4.dp), m.fitness, m.id.hashCode() and 0xff)
        }
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(m.name, style = H2.copy(fontSize = 18.sp), modifier = Modifier.weight(1f))
                Box(
                    Modifier.size(36.dp).clip(CircleShape).clickable { Squad.editMascot(m.id) { it.copy(onScreen = !it.onScreen) } },
                    contentAlignment = Alignment.Center,
                ) {
                    Icon(if (m.onScreen) Icons.Filled.Visibility else Icons.Filled.VisibilityOff, "Sur l'écran", tint = if (m.onScreen) C.yellow else C.dim, modifier = Modifier.size(20.dp))
                }
            }
            Text("${m.archetype.title} · niv. ${m.level}", style = Small.copy(fontSize = 12.sp))
            Text("${act.label} · ${Life.mood(m)}", style = Small.copy(fontSize = 12.sp, color = C.yellow))
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                StatBar("Ventre", m.satiety, C.green, Modifier.weight(1f))
                StatBar("Énergie", m.energy, C.blue, Modifier.weight(1f))
                StatBar("Moral", m.mood, C.yellow, Modifier.weight(1f))
            }
        }
    }
}

// =====================================================================================
// Fiche d'une mascotte
// =====================================================================================

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun DetailScreen(state: SquadState, id: String, onBack: () -> Unit, onStyle: () -> Unit) {
    val m = state.mascots.firstOrNull { it.id == id }
    if (m == null) { LaunchedEffect(Unit) { onBack() }; return }
    val a = m.archetype
    val phase = Life.phaseNow(state.schedule)
    val t = animTime()
    var forced by remember { mutableStateOf<Act?>(null) }
    var line by remember { mutableStateOf<String?>(a.lines(Say.TAP).random()) }
    var lineKey by remember { mutableStateOf(0) }
    var rename by remember { mutableStateOf(false) }
    var confirmDelete by remember { mutableStateOf(false) }

    LaunchedEffect(lineKey) { delay(4200); line = null }
    LaunchedEffect(forced) { if (forced != null) { delay(3500); forced = null } }
    fun speak(s: Say) { line = a.lines(s).randomOrNull(); lineKey++ }

    val act = forced ?: previewAct(m, phase, t)

    Column(Modifier.fillMaxSize()) {
        TopBar(m.name, onBack) {
            Box(Modifier.size(44.dp).clip(CircleShape).clickable { rename = true }, contentAlignment = Alignment.Center) {
                Icon(Icons.Filled.Edit, "Renommer", tint = C.dim)
            }
        }
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 16.dp, vertical = 4.dp)) {
            // Scène
            Box(Modifier.fillMaxWidth().height(290.dp).clip(RoundedCornerShape(24.dp)).background(C.card2), contentAlignment = Alignment.BottomCenter) {
                Canvas(Modifier.fillMaxSize()) {
                    drawRect(androidx.compose.ui.graphics.Brush.verticalGradient(listOf(Color(0xFF1A1A1A), Color(0xFF101010))))
                    drawRect(C.yellow.copy(alpha = 0.06f), topLeft = androidx.compose.ui.geometry.Offset(0f, size.height * 0.86f))
                }
                MascotCanvas(
                    m.look, act, Modifier.size(170.dp, 240.dp).padding(bottom = 6.dp).clickable {
                        if (act == Act.SLEEP) speak(Say.NIGHT) else { forced = Act.TALK; speak(Say.TAP) }
                    },
                    m.fitness, m.id.hashCode() and 0xff, talking = line != null,
                )
                if (line != null) {
                    Box(
                        Modifier.align(Alignment.TopCenter).padding(12.dp).clip(RoundedCornerShape(14.dp)).background(Color.White)
                            .border(2.dp, C.yellow, RoundedCornerShape(14.dp)).padding(horizontal = 12.dp, vertical = 8.dp),
                    ) { Text(line!!, style = Body.copy(color = Color.Black, fontWeight = FontWeight.Bold, fontSize = 14.sp)) }
                }
                Text(
                    "${act.label} · ${Life.mood(m)}", style = Label.copy(color = C.yellow),
                    modifier = Modifier.align(Alignment.BottomStart).padding(12.dp),
                )
            }
            Spacer(Modifier.height(12.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(a.title.uppercase(), style = Label.copy(color = C.yellow))
                    Text("Niveau ${m.level} · ${m.xp} XP · ${m.days} jour${if (m.days > 1) "s" else ""} dans la squad", style = Small)
                }
            }
            Spacer(Modifier.height(12.dp))
            Card {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    StatBar("Ventre", m.satiety, C.green)
                    StatBar("Énergie", m.energy, C.blue)
                    StatBar("Moral", m.mood, C.yellow)
                    StatBar("Forme physique", m.fitness, Color(0xFFFF7A45))
                }
            }
            Spacer(Modifier.height(12.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                ActionTile("Nourrir", Icons.Filled.Restaurant, Modifier.weight(1f)) {
                    val r = Squad.feed(m.id)
                    forced = Act.EAT
                    if (r == null || r.startsWith("Je suis plein")) { line = r; lineKey++ } else { line = "${a.lines(Say.FED).random()} ($r)"; lineKey++ }
                }
                ActionTile("Muscu", Icons.Filled.FitnessCenter, Modifier.weight(1f)) {
                    if (Squad.train(m.id)) { forced = Act.TRAIN; speak(Say.TRAIN) } else { forced = Act.YAWN; speak(Say.SLEEPY) }
                }
                ActionTile("Check", Icons.Filled.PanTool, Modifier.weight(1f)) { Squad.pet(m.id); forced = Act.FLEX; speak(Say.PET) }
                ActionTile("Parler", Icons.Filled.ChatBubble, Modifier.weight(1f)) { forced = Act.TALK; speak(Say.RANDOM) }
            }
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                GhostButton(if (m.onScreen) "Sur l'écran" else "Rangé", if (m.onScreen) Icons.Filled.Visibility else Icons.Filled.VisibilityOff, Modifier.weight(1f), color = if (m.onScreen) C.yellow else C.dim) {
                    Squad.editMascot(m.id) { it.copy(onScreen = !it.onScreen) }
                }
                GhostButton("Son style", Icons.Filled.Brush, Modifier.weight(1f), onClick = onStyle)
            }
            Spacer(Modifier.height(16.dp))
            Card {
                Column {
                    Text("PERSONNALITÉ", style = Label)
                    Spacer(Modifier.height(6.dp))
                    Text(a.bio, style = Body)
                    Spacer(Modifier.height(10.dp))
                    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        a.traits.forEach { Chip(it, true) {} }
                    }
                    Spacer(Modifier.height(10.dp))
                    Text("Plats préférés : ${a.food.joinToString(", ")}", style = Small)
                    Text("Il t'appelle « ${a.nick} »", style = Small)
                }
            }
            Spacer(Modifier.height(12.dp))
            Card {
                Column {
                    Text("SA JOURNÉE (CALÉE SUR LA TIENNE)", style = Label)
                    Spacer(Modifier.height(8.dp))
                    val s = state.schedule
                    listOf(
                        "Réveil" to s.wake, "Petit-déj" to s.breakfast, "Déjeuner" to s.lunch,
                    ).plus(if (s.workoutOn) listOf("Entraînement" to s.workout) else emptyList())
                        .plus(listOf("Dîner" to s.dinner, "Dodo" to s.bed))
                        .forEach { (k, v) ->
                            Row(Modifier.fillMaxWidth().padding(vertical = 3.dp)) {
                                Text(fmtMin(v), style = Body.copy(fontWeight = FontWeight.Bold, color = C.yellow), modifier = Modifier.width(64.dp))
                                Text(k, style = Body)
                            }
                        }
                }
            }
            Spacer(Modifier.height(12.dp))
            Card {
                Column {
                    Text("JOURNAL", style = Label)
                    Spacer(Modifier.height(8.dp))
                    val fmt = SimpleDateFormat("EEE HH:mm", Locale.FRANCE)
                    if (m.log.isEmpty()) Text("Rien pour l'instant.", style = Small)
                    m.log.take(15).forEach {
                        Row(Modifier.padding(vertical = 3.dp)) {
                            Text(fmt.format(Date(it.t)), style = Small.copy(fontSize = 12.sp), modifier = Modifier.width(84.dp))
                            Text(it.text, style = Body.copy(fontSize = 14.sp))
                        }
                    }
                }
            }
            Spacer(Modifier.height(16.dp))
            GhostButton("Virer de la squad", Icons.Filled.Delete, Modifier.fillMaxWidth(), color = C.red) { confirmDelete = true }
            Spacer(Modifier.height(24.dp))
        }
    }

    if (rename) {
        var name by remember { mutableStateOf(m.name) }
        AlertDialog(
            onDismissRequest = { rename = false },
            title = { Text("Son nom", style = H2) },
            text = {
                TextField(
                    name, { name = it.take(18) }, singleLine = true,
                    keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Words),
                    colors = TextFieldDefaults.colors(focusedContainerColor = C.card2, unfocusedContainerColor = C.card2, focusedIndicatorColor = C.yellow),
                )
            },
            confirmButton = { TextButton({ if (name.isNotBlank()) Squad.editMascot(m.id) { it.copy(name = name.trim()) }; rename = false }) { Text("OK", color = C.yellow) } },
            dismissButton = { TextButton({ rename = false }) { Text("Annuler", color = C.dim) } },
            containerColor = C.card,
        )
    }
    if (confirmDelete) {
        AlertDialog(
            onDismissRequest = { confirmDelete = false },
            title = { Text("Virer ${m.name} ?", style = H2) },
            text = { Text("Il quitte la squad pour de bon, avec son niveau et son journal. Pour juste le cacher de l'écran, utilise plutôt « Rangé ».", style = Body) },
            confirmButton = { TextButton({ confirmDelete = false; Squad.remove(m.id); onBack() }) { Text("Virer", color = C.red) } },
            dismissButton = { TextButton({ confirmDelete = false }) { Text("Garder", color = C.text) } },
            containerColor = C.card,
        )
    }
}

@Composable
private fun ActionTile(label: String, icon: ImageVector, modifier: Modifier, onClick: () -> Unit) {
    Column(
        modifier.clip(RoundedCornerShape(16.dp)).background(C.card).border(1.dp, C.line, RoundedCornerShape(16.dp))
            .clickable(onClick = onClick).padding(vertical = 12.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Icon(icon, null, tint = C.yellow, modifier = Modifier.size(22.dp))
        Spacer(Modifier.height(4.dp))
        Text(label, style = Small.copy(color = C.text, fontWeight = FontWeight.Bold, fontSize = 12.sp))
    }
}

@Composable
private fun Chip(text: String, selected: Boolean, onClick: () -> Unit) {
    Text(
        text,
        style = Small.copy(color = if (selected) Color.Black else C.text, fontWeight = FontWeight.Bold, fontSize = 13.sp),
        modifier = Modifier.clip(RoundedCornerShape(50)).background(if (selected) C.yellow else C.card2)
            .clickable(onClick = onClick).padding(horizontal = 12.dp, vertical = 7.dp),
    )
}

// =====================================================================================
// Recrutement
// =====================================================================================

@Composable
fun RecruitScreen(state: SquadState, onBack: () -> Unit, onDone: (String) -> Unit) {
    val t = animTime()
    val full = state.mascots.size >= Squad.MAX
    Column(Modifier.fillMaxSize()) {
        TopBar("Recruter", onBack)
        LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            item { Text("${Archetypes.all.size} caractères. Chacun vit à sa façon, mais tous suivent tes horaires.", style = Small) }
            items(Archetypes.all, key = { it.id }) { a ->
                val acts = a.hobbies.keys.filter { it != Act.WALK && it != Act.PUSHUP } + Act.WAVE
                val act = acts[((t / 3f).toInt() + a.id.length) % acts.size]
                Row(
                    Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(C.card).border(1.dp, C.line, RoundedCornerShape(20.dp)).padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Box(Modifier.size(96.dp, 128.dp).clip(RoundedCornerShape(14.dp)).background(C.card2)) {
                        MascotCanvas(a.look, act, Modifier.fillMaxSize().padding(4.dp), seed = a.id.hashCode() and 0xff)
                    }
                    Spacer(Modifier.width(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(a.name, style = H2.copy(fontSize = 18.sp))
                        Text(a.title.uppercase(), style = Label.copy(color = C.yellow))
                        Spacer(Modifier.height(4.dp))
                        Text(a.bio, style = Small.copy(fontSize = 12.5.sp), maxLines = 3)
                        Spacer(Modifier.height(4.dp))
                        Text(a.traits.joinToString(" · "), style = Small.copy(fontSize = 12.sp, color = C.text))
                        Spacer(Modifier.height(8.dp))
                        YellowButton(if (full) "Squad pleine" else "Recruter", Icons.Filled.Add, enabled = !full) {
                            Squad.add(a)?.let { onDone(it.id) }
                        }
                    }
                }
            }
        }
    }
}

// =====================================================================================
// Style
// =====================================================================================

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun StyleScreen(state: SquadState, id: String, onBack: () -> Unit) {
    val m = state.mascots.firstOrNull { it.id == id } ?: run { LaunchedEffect(Unit) { onBack() }; return }
    val L = m.look
    fun set(f: (Look) -> Look) = Squad.editMascot(id) { it.copy(look = f(it.look)) }
    val t = animTime()
    Column(Modifier.fillMaxSize()) {
        TopBar("Style de ${m.name}", onBack)
        Box(Modifier.fillMaxWidth().height(200.dp).padding(horizontal = 16.dp).clip(RoundedCornerShape(24.dp)).background(C.card2), contentAlignment = Alignment.Center) {
            MascotCanvas(L, if ((t / 4f).toInt() % 2 == 0) Act.IDLE else Act.FLEX, Modifier.size(140.dp, 190.dp), m.fitness, 1)
        }
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            Section("COUPE") { FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) { HairStyle.entries.forEach { v -> Chip(v.label, L.hair == v) { set { it.copy(hair = v) } } } } }
            Section("BARBE") { FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) { Beard.entries.forEach { v -> Chip(v.label, L.beard == v) { set { it.copy(beard = v) } } } } }
            Section("TENUE") { FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) { Outfit.entries.forEach { v -> Chip(v.label, L.outfit == v) { set { it.copy(outfit = v) } } } } }
            Section("ACCESSOIRE") { FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) { Accessory.entries.forEach { v -> Chip(v.label, L.accessory == v) { set { it.copy(accessory = v) } } } } }
            Section("PEAU") { Swatches(Palette.skins, L.skin) { c -> set { it.copy(skin = c) } } }
            Section("CHEVEUX ET BARBE") { Swatches(Palette.hairs, L.hairColor) { c -> set { it.copy(hairColor = c) } } }
            Section("COULEUR DE LA TENUE") { Swatches(Palette.clothes, L.outfitColor) { c -> set { it.copy(outfitColor = c) } } }
            Section("PANTALON") { Swatches(Palette.clothes, L.pantsColor) { c -> set { it.copy(pantsColor = c) } } }
            ToggleRow("Tatouages", L.tattoos) { v -> set { it.copy(tattoos = v) } }
            ToggleRow("Cicatrice", L.scar) { v -> set { it.copy(scar = v) } }
            GhostButton("Revenir à son look d'origine", modifier = Modifier.fillMaxWidth()) { set { m.archetype.look } }
            Spacer(Modifier.height(20.dp))
        }
    }
}

@Composable
private fun Section(title: String, content: @Composable () -> Unit) {
    Column { Text(title, style = Label); Spacer(Modifier.height(8.dp)); content() }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun Swatches(colors: IntArray, selected: Int, onPick: (Int) -> Unit) {
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        colors.forEach { c ->
            Box(
                Modifier.size(36.dp).clip(CircleShape).background(Color(c))
                    .border(if (c == selected) 3.dp else 1.dp, if (c == selected) C.yellow else C.line, CircleShape)
                    .clickable { onPick(c) },
            )
        }
    }
}

@Composable
private fun ToggleRow(label: String, value: Boolean, sub: String? = null, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().clickable { onChange(!value) }.padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(label, style = Body)
            if (sub != null) Text(sub, style = Small.copy(fontSize = 12.sp))
        }
        Switch(value, onChange, colors = SwitchDefaults.colors(checkedThumbColor = Color.Black, checkedTrackColor = C.yellow))
    }
}

// =====================================================================================
// Mon rythme + réglages
// =====================================================================================

@Composable
fun RhythmScreen(state: SquadState, onBack: () -> Unit, onBattery: () -> Unit) {
    val ctx = LocalContext.current
    val s = state.schedule
    val st = state.settings
    fun pick(current: Int, apply: (Int) -> Unit) {
        TimePickerDialog(ctx, { _, h, mi -> apply(h * 60 + mi) }, current / 60, current % 60, true).show()
    }
    fun sched(f: (com.superessence.squad.model.Schedule) -> com.superessence.squad.model.Schedule) = Squad.edit { it.copy(schedule = f(it.schedule)) }
    fun sett(f: (com.superessence.squad.model.Settings) -> com.superessence.squad.model.Settings) = Squad.edit { it.copy(settings = f(it.settings)) }

    Column(Modifier.fillMaxSize()) {
        TopBar("Mon rythme", onBack)
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            Text("Ta squad vit à ton rythme : ils dorment quand tu dors, mangent quand tu manges et s'entraînent à ton heure.", style = Small)
            Card {
                Column {
                    Text("SOMMEIL", style = Label)
                    TimeRow("Réveil (semaine)", s.wake) { pick(s.wake) { v -> sched { it.copy(wake = v) } } }
                    TimeRow("Réveil (week-end)", s.wakeWeekend) { pick(s.wakeWeekend) { v -> sched { it.copy(wakeWeekend = v) } } }
                    TimeRow("Coucher", s.bed) { pick(s.bed) { v -> sched { it.copy(bed = v) } } }
                }
            }
            Card {
                Column {
                    Text("REPAS", style = Label)
                    TimeRow("Petit-déj", s.breakfast) { pick(s.breakfast) { v -> sched { it.copy(breakfast = v) } } }
                    TimeRow("Déjeuner", s.lunch) { pick(s.lunch) { v -> sched { it.copy(lunch = v) } } }
                    TimeRow("Dîner", s.dinner) { pick(s.dinner) { v -> sched { it.copy(dinner = v) } } }
                }
            }
            Card {
                Column {
                    Text("SPORT", style = Label)
                    ToggleRow("Séance quotidienne", s.workoutOn) { v -> sched { it.copy(workoutOn = v) } }
                    if (s.workoutOn) TimeRow("Heure de la séance", s.workout) { pick(s.workout) { v -> sched { it.copy(workout = v) } } }
                }
            }
            Card {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("À L'ÉCRAN", style = Label)
                    SliderRow("Taille", st.size, 0.6f..1.6f) { v -> sett { it.copy(size = v) } }
                    SliderRow("Vitesse de marche", st.speed, 0.3f..2f) { v -> sett { it.copy(speed = v) } }
                    SliderRow("Bavardage", st.chatter, 0f..2.5f) { v -> sett { it.copy(chatter = v) } }
                    ToggleRow("Rester visibles en dormant", st.showSleeping, "Sinon ils disparaissent la nuit et reviennent au réveil") { v -> sett { it.copy(showSleeping = v) } }
                    ToggleRow("Vibrations", st.vibrate) { v -> sett { it.copy(vibrate = v) } }
                }
            }
            Card {
                Column {
                    Text("SI ILS DISPARAISSENT TOUT SEULS", style = Label)
                    Spacer(Modifier.height(6.dp))
                    Text("Certains téléphones (Xiaomi, Samsung, Huawei…) coupent les applis en arrière-plan. Retire SQUAD. de l'optimisation de batterie pour garder ta squad en vie.", style = Small)
                    Spacer(Modifier.height(10.dp))
                    GhostButton("Réglages batterie", modifier = Modifier.fillMaxWidth(), onClick = onBattery)
                }
            }
            Column(Modifier.fillMaxWidth().padding(vertical = 12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                StudioMark(36.dp)
                Spacer(Modifier.height(6.dp))
                Text("SQUAD. 1.0 · SuperEssence Studio", style = Small.copy(fontSize = 12.sp))
            }
        }
    }
}

@Composable
private fun TimeRow(label: String, value: Int, onClick: () -> Unit) {
    Row(Modifier.fillMaxWidth().clickable(onClick = onClick).padding(vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(label, style = Body, modifier = Modifier.weight(1f))
        Text(fmtMin(value), style = Body.copy(fontWeight = FontWeight.Bold, color = C.yellow, fontSize = 17.sp))
    }
}

@Composable
private fun SliderRow(label: String, value: Float, range: ClosedFloatingPointRange<Float>, onChange: (Float) -> Unit) {
    Column {
        Row { Text(label, style = Body, modifier = Modifier.weight(1f)); Text("×%.1f".format(value), style = Small) }
        Slider(value, onChange, valueRange = range, colors = SliderDefaults.colors(thumbColor = C.yellow, activeTrackColor = C.yellow, inactiveTrackColor = C.line))
    }
}
