package com.superessence.focus.ui

import androidx.activity.compose.BackHandler
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.focus.core.Card
import com.superessence.focus.core.Cards
import com.superessence.focus.core.Deck
import com.superessence.focus.core.Store

@Composable
fun CardsScreen() {
    @Suppress("UNUSED_VARIABLE") val rev = Store.rev.intValue
    val decks = Cards.decks()
    var deckId by rememberSaveable { mutableStateOf<Long?>(null) }
    var reviewing by rememberSaveable { mutableStateOf(false) }
    val deck = decks.firstOrNull { it.id == deckId }

    when {
        deck != null && reviewing -> { BackHandler { reviewing = false }; Review(deck) { reviewing = false } }
        deck != null -> { BackHandler { deckId = null }; DeckView(deck, onBack = { deckId = null }, onReview = { reviewing = true }) }
        else -> DeckList(decks) { deckId = it }
    }
}

@Composable
private fun DeckList(decks: List<Deck>, onOpen: (Long) -> Unit) {
    var adding by remember { mutableStateOf(false) }
    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Title("Mes fiches")
                    Body("Répétition espacée : tu revois chaque fiche juste avant de l'oublier.", size = 13.sp)
                }
                AddButton { adding = true }
            }
        }
        items(decks, key = { it.id }) { d ->
            val color = Color(Cards.COLORS[d.color % Cards.COLORS.size])
            Panel(Modifier.fillMaxWidth().clickable { onOpen(d.id) }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(Modifier.size(14.dp).clip(CircleShape).background(color))
                    Spacer(Modifier.width(10.dp))
                    Title(d.name, 19.sp, modifier = Modifier.weight(1f))
                    val due = d.dueCount()
                    if (due > 0) Pill(color = C.Yellow) { Text("$due à revoir", color = Color.Black, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 12.sp) }
                }
                Spacer(Modifier.height(10.dp))
                Body("${d.cards.size} fiches · ${d.mastered()} maîtrisées", size = 13.sp)
                Spacer(Modifier.height(8.dp))
                LinearProgressIndicator(
                    progress = { if (d.cards.isEmpty()) 0f else d.cards.sumOf { it.box - 1 }.toFloat() / (d.cards.size * 4) },
                    modifier = Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)),
                    color = color, trackColor = C.Surface2, drawStopIndicator = {},
                )
            }
        }
        if (decks.isEmpty()) item { Body("Crée ta première matière avec le bouton +", modifier = Modifier.padding(top = 30.dp)) }
    }
    if (adding) TextDialog("Nouvelle matière", "Ex. : Histoire, Maths, Anglais…", "", onDismiss = { adding = false }) {
        if (it.isNotBlank()) Cards.addDeck(it); adding = false
    }
}

@Composable
private fun DeckView(deck: Deck, onBack: () -> Unit, onReview: () -> Unit) {
    var adding by remember { mutableStateOf(false) }
    var editing by remember { mutableStateOf<Card?>(null) }
    var renaming by remember { mutableStateOf(false) }
    var deleting by remember { mutableStateOf(false) }
    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        item {
            Row(verticalAlignment = Alignment.CenterVertically) {
                IconButton(onBack) { Icon(Icons.AutoMirrored.Rounded.ArrowBack, "Retour", tint = C.Text) }
                Title(deck.name, 24.sp, modifier = Modifier.weight(1f))
                IconButton({ renaming = true }) { Icon(Icons.Rounded.Edit, "Renommer", tint = C.Muted) }
                IconButton({ deleting = true }) { Icon(Icons.Rounded.Delete, "Supprimer", tint = C.Muted) }
            }
            Spacer(Modifier.height(6.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Button(
                    onReview, Modifier.weight(1f).height(52.dp), enabled = deck.cards.isNotEmpty(),
                    shape = RoundedCornerShape(16.dp), colors = ButtonDefaults.buttonColors(C.Yellow, Color.Black),
                ) {
                    val due = deck.dueCount()
                    Text(if (due > 0) "Réviser ($due)" else "Réviser tout", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 17.sp)
                }
                Button(
                    { adding = true }, Modifier.height(52.dp), shape = RoundedCornerShape(16.dp),
                    colors = ButtonDefaults.buttonColors(C.Surface2, C.Text),
                ) { Icon(Icons.Rounded.Add, null); Spacer(Modifier.width(4.dp)); Text("Fiche") }
            }
        }
        items(deck.cards, key = { it.id }) { c ->
            Panel(Modifier.fillMaxWidth().clickable { editing = c }) {
                Row(verticalAlignment = Alignment.Top) {
                    Column(Modifier.weight(1f)) {
                        Body(c.q, color = C.Text, size = 15.sp, weight = FontWeight.Bold)
                        Spacer(Modifier.height(4.dp))
                        Text(c.a, color = C.Muted, fontFamily = Nunito, fontSize = 13.sp, maxLines = 2, overflow = TextOverflow.Ellipsis)
                    }
                    Spacer(Modifier.width(8.dp))
                    Row { repeat(5) { i -> Box(Modifier.padding(1.dp).size(6.dp).clip(CircleShape).background(if (i < c.box) C.Yellow else C.Surface2)) } }
                }
            }
        }
        if (deck.cards.isEmpty()) item { Body("Aucune fiche. Ajoute une question et sa réponse avec « + Fiche ».", modifier = Modifier.padding(top = 20.dp)) }
    }
    if (adding) CardDialog(null, onDismiss = { adding = false }) { q, a -> Cards.addCard(deck.id, q, a); adding = false }
    editing?.let { c ->
        CardDialog(c, onDismiss = { editing = null }, onDelete = { Cards.deleteCard(deck.id, c.id); editing = null }) { q, a ->
            Cards.editCard(deck.id, c.id, q, a); editing = null
        }
    }
    if (renaming) TextDialog("Renommer", "Nom de la matière", deck.name, onDismiss = { renaming = false }) {
        if (it.isNotBlank()) Cards.renameDeck(deck.id, it); renaming = false
    }
    if (deleting) AlertDialog(
        onDismissRequest = { deleting = false }, containerColor = C.Surface,
        title = { Title("Supprimer « ${deck.name} » ?", 19.sp) },
        text = { Body("Les ${deck.cards.size} fiches seront supprimées.", color = C.Text) },
        confirmButton = { TextButton({ deleting = false; Cards.deleteDeck(deck.id); onBack() }) { Text("Supprimer", color = C.Red) } },
        dismissButton = { TextButton({ deleting = false }) { Text("Annuler", color = C.Muted) } },
    )
}

@Composable
private fun Review(deck: Deck, onExit: () -> Unit) {
    val queue = remember(deck.id) {
        val now = System.currentTimeMillis()
        deck.cards.filter { it.due <= now }.ifEmpty { deck.cards }.shuffled().map { it.id }
    }
    var index by rememberSaveable { mutableIntStateOf(0) }
    var known by rememberSaveable { mutableIntStateOf(0) }
    var flipped by remember { mutableStateOf(false) }
    val rot by animateFloatAsState(if (flipped) 180f else 0f, tween(450), label = "flip")

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(onExit) { Icon(Icons.AutoMirrored.Rounded.ArrowBack, "Retour", tint = C.Text) }
            Title(deck.name, 20.sp, modifier = Modifier.weight(1f))
            Body("${(index + 1).coerceAtMost(queue.size)} / ${queue.size}", color = C.Text, weight = FontWeight.Bold)
        }
        LinearProgressIndicator(
            progress = { index.toFloat() / queue.size.coerceAtLeast(1) },
            modifier = Modifier.fillMaxWidth().padding(vertical = 10.dp).height(6.dp).clip(RoundedCornerShape(3.dp)),
            color = C.Yellow, trackColor = C.Surface2, drawStopIndicator = {},
        )
        if (index >= queue.size) {
            Column(Modifier.fillMaxSize(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
                FocusMark(110.dp, glow = 1f)
                Spacer(Modifier.height(20.dp))
                Title("Bravo !", 34.sp)
                Body("$known / ${queue.size} fiches sues", color = C.Text, size = 17.sp)
                Spacer(Modifier.height(24.dp))
                Button(onExit, shape = RoundedCornerShape(16.dp), colors = ButtonDefaults.buttonColors(C.Yellow, Color.Black)) {
                    Text("Terminer", fontFamily = Fredoka, fontWeight = FontWeight.Bold)
                }
            }
            return
        }
        val card = deck.cards.firstOrNull { it.id == queue[index] }
        if (card == null) { index++; return }
        Box(
            Modifier.fillMaxWidth().weight(1f).padding(vertical = 12.dp)
                .graphicsLayer { rotationY = rot; cameraDistance = 14f * density }
                .clip(RoundedCornerShape(28.dp))
                .background(if (rot > 90f) C.Surface2 else C.Surface)
                .clickable { flipped = !flipped }
                .padding(28.dp),
            contentAlignment = Alignment.Center,
        ) {
            val back = rot > 90f
            Column(
                Modifier.graphicsLayer { rotationY = if (back) 180f else 0f },
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Body(if (back) "RÉPONSE" else "QUESTION", color = C.Yellow, size = 12.sp, weight = FontWeight.Bold)
                Spacer(Modifier.height(16.dp))
                Text(
                    if (back) card.a else card.q, color = C.Text, fontFamily = if (back) Nunito else Fredoka,
                    fontWeight = if (back) FontWeight.SemiBold else FontWeight.Bold,
                    fontSize = if (back) 20.sp else 26.sp, textAlign = TextAlign.Center, lineHeight = if (back) 28.sp else 32.sp,
                )
                if (!back) { Spacer(Modifier.height(24.dp)); Body("Touche pour retourner", size = 12.sp) }
            }
        }
        if (flipped) {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(
                    { Cards.answer(deck.id, card.id, false); flipped = false; index++ },
                    Modifier.weight(1f).height(58.dp), shape = RoundedCornerShape(18.dp),
                    colors = ButtonDefaults.buttonColors(C.Surface2, C.Text),
                ) { Text("À revoir", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 17.sp) }
                Button(
                    { Cards.answer(deck.id, card.id, true); known++; flipped = false; index++ },
                    Modifier.weight(1f).height(58.dp), shape = RoundedCornerShape(18.dp),
                    colors = ButtonDefaults.buttonColors(C.Yellow, Color.Black),
                ) { Text("Je savais ✓", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 17.sp) }
            }
        } else Spacer(Modifier.height(58.dp))
    }
}

@Composable
fun AddButton(onClick: () -> Unit) {
    Box(Modifier.size(48.dp).clip(CircleShape).background(C.Yellow).clickable(onClick = onClick), contentAlignment = Alignment.Center) {
        Icon(Icons.Rounded.Add, "Ajouter", tint = Color.Black)
    }
}

@Composable
fun fieldColors() = OutlinedTextFieldDefaults.colors(
    focusedBorderColor = C.Yellow, unfocusedBorderColor = C.Line, cursorColor = C.Yellow,
    focusedLabelColor = C.Yellow, unfocusedLabelColor = C.Muted, focusedTextColor = C.Text, unfocusedTextColor = C.Text,
)

@Composable
fun TextDialog(title: String, label: String, initial: String, onDismiss: () -> Unit, onOk: (String) -> Unit) {
    var t by remember { mutableStateOf(initial) }
    AlertDialog(
        onDismissRequest = onDismiss, containerColor = C.Surface,
        title = { Title(title, 20.sp) },
        text = { OutlinedTextField(t, { t = it }, label = { Text(label) }, singleLine = true, colors = fieldColors(), shape = RoundedCornerShape(14.dp)) },
        confirmButton = { TextButton({ onOk(t) }) { Text("OK", color = C.Yellow) } },
        dismissButton = { TextButton(onDismiss) { Text("Annuler", color = C.Muted) } },
    )
}

@Composable
private fun CardDialog(card: Card?, onDismiss: () -> Unit, onDelete: (() -> Unit)? = null, onOk: (String, String) -> Unit) {
    var q by remember { mutableStateOf(card?.q ?: "") }
    var a by remember { mutableStateOf(card?.a ?: "") }
    AlertDialog(
        onDismissRequest = onDismiss, containerColor = C.Surface,
        title = { Title(if (card == null) "Nouvelle fiche" else "Modifier la fiche", 20.sp) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedTextField(q, { q = it }, label = { Text("Question (recto)") }, colors = fieldColors(), shape = RoundedCornerShape(14.dp), minLines = 2)
                OutlinedTextField(a, { a = it }, label = { Text("Réponse (verso)") }, colors = fieldColors(), shape = RoundedCornerShape(14.dp), minLines = 3)
            }
        },
        confirmButton = { TextButton({ if (q.isNotBlank() && a.isNotBlank()) onOk(q, a) }) { Text("Enregistrer", color = C.Yellow) } },
        dismissButton = {
            Row {
                if (onDelete != null) TextButton(onDelete) { Text("Supprimer", color = C.Red) }
                TextButton(onDismiss) { Text("Annuler", color = C.Muted) }
            }
        },
    )
}
