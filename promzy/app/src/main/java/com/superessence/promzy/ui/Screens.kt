package com.superessence.promzy.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
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
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.LocalFireDepartment
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.WifiOff
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.promzy.PromzyViewModel
import com.superessence.promzy.data.Category
import com.superessence.promzy.data.Deal
import com.superessence.promzy.data.Feed
import kotlinx.coroutines.delay

enum class Tab(val label: String, val icon: ImageVector) {
    TOP("Top", Icons.Default.LocalFireDepartment),
    LIVE("En direct", Icons.Default.Bolt),
    SEARCH("Chercher", Icons.Default.Search),
    FAVS("Favoris", Icons.Default.Favorite),
}

@Composable
fun MainScreen(vm: PromzyViewModel, startTab: Tab = Tab.TOP) {
    var tab by rememberSaveable { mutableStateOf(startTab) }
    Column(Modifier.fillMaxSize().background(Ink)) {
        Header(vm)
        Box(Modifier.weight(1f)) {
            when (tab) {
                Tab.TOP -> FeedScreen(vm, Feed.HOT, ranked = true)
                Tab.LIVE -> FeedScreen(vm, Feed.NEW, ranked = false)
                Tab.SEARCH -> SearchScreen(vm)
                Tab.FAVS -> FavoritesScreen(vm)
            }
        }
        NavigationBar(containerColor = Color(0xFF111114), tonalElevation = 0.dp, modifier = Modifier.navigationBarsPadding().height(64.dp)) {
            Tab.entries.forEach { t ->
                NavigationBarItem(
                    selected = tab == t, onClick = { tab = t },
                    icon = { Icon(t.icon, null) },
                    label = { Text(t.label, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 11.sp) },
                    colors = NavigationBarItemDefaults.colors(
                        selectedIconColor = Color.Black, indicatorColor = Yellow, selectedTextColor = Yellow,
                        unselectedIconColor = TextLow, unselectedTextColor = TextLow,
                    ),
                )
            }
        }
    }
}

@Composable
private fun Header(vm: PromzyViewModel) {
    val live by vm.feeds.getValue(Feed.NEW).collectAsState()
    val hot by vm.feeds.getValue(Feed.HOT).collectAsState()
    val updated = maxOf(live.updatedAt, hot.updatedAt)
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) { while (true) { delay(1000); now = System.currentTimeMillis() } }
    val pulse by rememberInfiniteTransition(label = "live").animateFloat(0.25f, 1f, infiniteRepeatable(tween(800, easing = LinearEasing), RepeatMode.Reverse), label = "p")
    Row(Modifier.fillMaxWidth().statusBarsPadding().padding(start = 16.dp, end = 16.dp, top = 10.dp, bottom = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        PromzyLockup()
        Spacer(Modifier.weight(1f))
        Column(horizontalAlignment = Alignment.End) {
            Row(
                Modifier.clip(RoundedCornerShape(50)).background(Live.copy(alpha = 0.14f)).padding(horizontal = 9.dp, vertical = 3.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Box(Modifier.size(7.dp).clip(CircleShape).background(Live.copy(alpha = if (live.error == null) pulse else 0.4f)))
                Spacer(Modifier.width(5.dp))
                Text(if (live.error == null) "EN DIRECT" else "HORS LIGNE", color = Live, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 11.sp, letterSpacing = 0.8.sp)
            }
            if (updated > 0) {
                val s = ((now - updated) / 1000).coerceAtLeast(0)
                Text(if (s < 5) "à jour" else "maj il y a ${if (s < 60) "$s s" else "${s / 60} min"}", color = TextLow, fontFamily = Nunito, fontSize = 11.sp, modifier = Modifier.padding(top = 2.dp))
            }
        }
    }
}

@Composable
private fun CategoryChips(selected: Category, onSelect: (Category) -> Unit) {
    LazyRow(contentPadding = PaddingValues(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(vertical = 6.dp)) {
        items(Category.entries) { c ->
            val on = c == selected
            Text(
                c.label, color = if (on) Color.Black else TextMid, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 13.sp,
                modifier = Modifier.clip(RoundedCornerShape(50)).background(if (on) Yellow else Card)
                    .border(1.dp, if (on) Yellow else Line, RoundedCornerShape(50))
                    .clickable { onSelect(c) }.padding(horizontal = 14.dp, vertical = 7.dp),
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FeedScreen(vm: PromzyViewModel, feed: Feed, ranked: Boolean) {
    val st by vm.feeds.getValue(feed).collectAsState()
    val cat by vm.category.collectAsState()
    val favs by vm.favorites.items.collectAsState()
    val favIds = favs.map { it.id }.toSet()
    val list = st.deals.filter { cat.matches(it) }.let { l -> if (ranked) l else l.sortedByDescending { it.publishedAt } }
    Column(Modifier.fillMaxSize()) {
        CategoryChips(cat) { vm.category.value = it }
        PullToRefreshBox(isRefreshing = st.loading && st.deals.isNotEmpty(), onRefresh = { vm.refresh(feed) }, modifier = Modifier.weight(1f)) {
            when {
                st.deals.isEmpty() && st.loading -> Loading()
                st.deals.isEmpty() && st.error != null -> Message(Icons.Default.WifiOff, "Pas de connexion", st.error!!, "Réessayer") { vm.refresh(feed) }
                list.isEmpty() -> Message(Icons.Default.Search, "Rien pour l'instant", "Aucune promo « ${cat.label} » dans ce flux. Les offres arrivent en continu, reviens dans un instant.", null) {}
                else -> LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(bottom = 16.dp)) {
                    if (st.error != null) item { Banner("Hors ligne : dernières promos enregistrées.") }
                    item {
                        Text(
                            if (ranked) "Les promos les plus chaudes" else "Publiées à l'instant",
                            color = TextHi, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 20.sp,
                            modifier = Modifier.padding(start = 16.dp, top = 6.dp, bottom = 4.dp),
                        )
                    }
                    itemsIndexed(list, key = { _, d -> d.id }) { i, d ->
                        if (ranked && i == 0) HeroDealCard(d, d.id in favIds, { vm.favorites.toggle(d) }, d.id in st.freshIds)
                        else DealCard(d, d.id in favIds, { vm.favorites.toggle(d) }, rank = if (ranked) i + 1 else null, fresh = d.id in st.freshIds)
                    }
                    item { Credit() }
                }
            }
        }
    }
}

private val Suggestions = listOf("iPhone", "PS5", "AirPods", "TV OLED", "Lego", "Nike", "Aspirateur robot", "Switch", "SSD", "Vol pas cher", "Parfum", "Jeux gratuits")

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun SearchScreen(vm: PromzyViewModel) {
    val st by vm.search.collectAsState()
    val recent by vm.recent.collectAsState()
    val favs by vm.favorites.items.collectAsState()
    val favIds = favs.map { it.id }.toSet()
    var text by rememberSaveable { mutableStateOf(st.query) }
    val focus = LocalFocusManager.current
    fun go(q: String) { text = q; focus.clearFocus(); vm.runSearch(q) }

    Column(Modifier.fillMaxSize()) {
        TextField(
            value = text, onValueChange = { text = it },
            placeholder = { Text("Écris ce que tu cherches…", fontFamily = Nunito, color = TextLow) },
            leadingIcon = { Icon(Icons.Default.Search, null, tint = Yellow) },
            trailingIcon = { if (text.isNotEmpty()) IconButton(onClick = { text = ""; vm.clearSearch() }) { Icon(Icons.Default.Close, "Effacer", tint = TextMid) } },
            singleLine = true,
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
            keyboardActions = KeyboardActions(onSearch = { go(text) }),
            shape = RoundedCornerShape(18.dp),
            colors = TextFieldDefaults.colors(
                focusedContainerColor = Card, unfocusedContainerColor = Card, focusedIndicatorColor = Color.Transparent,
                unfocusedIndicatorColor = Color.Transparent, cursorColor = Yellow, focusedTextColor = TextHi, unfocusedTextColor = TextHi,
            ),
            textStyle = androidx.compose.ui.text.TextStyle(fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 16.sp),
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
        )
        when {
            st.loading -> Loading()
            st.searched -> {
                if (st.results.isEmpty()) Message(Icons.Default.Search, "Aucune promo trouvée", st.error ?: "Rien pour « ${st.query} » en ce moment. Essaie un autre mot ou reviens plus tard : les offres changent en continu.", null) {}
                else LazyColumn(contentPadding = PaddingValues(bottom = 16.dp)) {
                    item {
                        Text("${st.results.size} promo${if (st.results.size > 1) "s" else ""} pour « ${st.query} »", color = TextHi, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 18.sp, modifier = Modifier.padding(start = 16.dp, top = 4.dp, bottom = 4.dp))
                    }
                    items(st.results, key = { it.id }) { d -> DealCard(d, d.id in favIds, { vm.favorites.toggle(d) }) }
                    item { Credit() }
                }
            }
            else -> Column(Modifier.padding(horizontal = 16.dp)) {
                if (recent.isNotEmpty()) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        SectionTitle("Recherches récentes", Modifier.weight(1f))
                        Text("Effacer", color = TextLow, fontFamily = Nunito, fontSize = 13.sp, modifier = Modifier.clickable { vm.clearRecent() }.padding(6.dp))
                    }
                    recent.forEach { r ->
                        Row(Modifier.fillMaxWidth().clickable { go(r) }.padding(vertical = 9.dp), verticalAlignment = Alignment.CenterVertically) {
                            Icon(Icons.Default.History, null, tint = TextLow, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(10.dp))
                            Text(r, color = TextMid, fontFamily = Nunito, fontSize = 15.sp)
                        }
                    }
                    Spacer(Modifier.height(14.dp))
                }
                SectionTitle("Idées de recherche")
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Suggestions.forEach { s ->
                        Text(
                            s, color = TextHi, fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 14.sp,
                            modifier = Modifier.clip(RoundedCornerShape(50)).background(Card).border(1.dp, Line, RoundedCornerShape(50))
                                .clickable { go(s) }.padding(horizontal = 14.dp, vertical = 8.dp),
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun FavoritesScreen(vm: PromzyViewModel) {
    val favs by vm.favorites.items.collectAsState()
    if (favs.isEmpty()) Message(Icons.Default.Favorite, "Aucun favori", "Touche le cœur d'une promo pour la garder ici, même hors connexion.", null) {}
    else LazyColumn(contentPadding = PaddingValues(bottom = 16.dp)) {
        item { Text("Mes promos gardées", color = TextHi, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 20.sp, modifier = Modifier.padding(start = 16.dp, top = 8.dp, bottom = 4.dp)) }
        items(favs, key = { it.id }) { d -> DealCard(d, true, { vm.favorites.toggle(d) }) }
    }
}

@Composable
private fun SectionTitle(t: String, modifier: Modifier = Modifier) =
    Text(t, color = TextHi, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 17.sp, modifier = modifier.padding(vertical = 8.dp))

@Composable
private fun Loading() = Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        CircularProgressIndicator(color = Yellow, strokeWidth = 3.dp)
        Spacer(Modifier.height(14.dp))
        Text("Chasse aux promos en cours…", color = TextMid, fontFamily = Nunito, fontSize = 14.sp)
    }
}

@Composable
private fun Banner(t: String) = Text(
    t, color = Color.Black, fontFamily = Nunito, fontWeight = FontWeight.SemiBold, fontSize = 13.sp,
    modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 4.dp).clip(RoundedCornerShape(12.dp)).background(Yellow).padding(10.dp),
)

@Composable
private fun Credit() = Text(
    "Promos issues des flux publics de Dealabs, mises à jour toutes les minutes.\nPromzy · SuperEssence Studio",
    color = TextLow, fontFamily = Nunito, fontSize = 11.sp, textAlign = TextAlign.Center,
    modifier = Modifier.fillMaxWidth().padding(20.dp),
)

@Composable
private fun Message(icon: ImageVector, title: String, body: String, action: String?, onAction: () -> Unit) {
    Box(Modifier.fillMaxSize().padding(32.dp), contentAlignment = Alignment.Center) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Box(Modifier.size(64.dp).clip(CircleShape).background(Card), contentAlignment = Alignment.Center) { Icon(icon, null, tint = Yellow, modifier = Modifier.size(30.dp)) }
            Spacer(Modifier.height(14.dp))
            Text(title, color = TextHi, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 19.sp)
            Spacer(Modifier.height(6.dp))
            Text(body, color = TextMid, fontFamily = Nunito, fontSize = 14.sp, textAlign = TextAlign.Center)
            action?.let {
                Spacer(Modifier.height(16.dp))
                Text(it, color = Color.Black, fontFamily = Nunito, fontWeight = FontWeight.Bold, modifier = Modifier.clip(RoundedCornerShape(50)).background(Yellow).clickable(onClick = onAction).padding(horizontal = 20.dp, vertical = 10.dp))
            }
        }
    }
}
