package com.superessence.promzy

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.superessence.promzy.data.Category
import com.superessence.promzy.data.Deal
import com.superessence.promzy.data.DealRepository
import com.superessence.promzy.data.Favorites
import com.superessence.promzy.data.Feed
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

data class FeedState(
    val deals: List<Deal> = emptyList(),
    val loading: Boolean = false,
    val error: String? = null,
    val updatedAt: Long = 0L,
    val freshIds: Set<String> = emptySet(),
)

data class SearchState(
    val query: String = "",
    val results: List<Deal> = emptyList(),
    val loading: Boolean = false,
    val searched: Boolean = false,
    val error: String? = null,
)

open class PromzyViewModel(app: Application) : AndroidViewModel(app) {
    private val repo = DealRepository(app)
    val favorites = Favorites(app)

    val feeds: Map<Feed, MutableStateFlow<FeedState>> =
        Feed.entries.associateWith { MutableStateFlow(FeedState(deals = repo.cached(it))) }
    val category = MutableStateFlow(Category.ALL)
    private val _search = MutableStateFlow(SearchState())
    val search: StateFlow<SearchState> = _search
    val recent = MutableStateFlow(favorites.recentSearches)

    private var liveJob: Job? = null
    private var searchJob: Job? = null

    /** Rafraîchissement « en direct » : toutes les 60 s tant que l'app est au premier plan. */
    fun startLive() {
        if (liveJob?.isActive == true) return
        liveJob = viewModelScope.launch {
            while (true) {
                Feed.entries.forEach { refresh(it) }
                delay(60_000)
            }
        }
    }

    fun stopLive() { liveJob?.cancel(); liveJob = null }

    fun refresh(feed: Feed) {
        val st = feeds.getValue(feed)
        st.update { it.copy(loading = true, error = null) }
        viewModelScope.launch {
            try {
                val deals = repo.load(feed)
                st.update { old ->
                    val before = old.deals.map { it.id }.toSet()
                    val fresh = if (old.updatedAt == 0L) emptySet() else deals.map { it.id }.filterNot { it in before }.toSet()
                    FeedState(deals, false, null, System.currentTimeMillis(), fresh + old.freshIds.intersect(deals.map { it.id }.toSet()))
                }
            } catch (e: Exception) {
                st.update { it.copy(loading = false, error = "Connexion impossible. Vérifie ta connexion Internet.") }
            }
        }
    }

    fun runSearch(q: String) {
        val query = q.trim()
        if (query.isEmpty()) return
        recent.value = (listOf(query) + recent.value.filterNot { it.equals(query, true) }).take(8)
        favorites.recentSearches = recent.value
        searchJob?.cancel()
        _search.value = SearchState(query = query, loading = true)
        searchJob = viewModelScope.launch {
            try {
                val r = repo.search(query)
                _search.value = SearchState(query, r, false, true)
            } catch (e: Exception) {
                _search.value = SearchState(query, emptyList(), false, true, "Recherche impossible pour le moment.")
            }
        }
    }

    fun clearSearch() { searchJob?.cancel(); _search.value = SearchState() }
    fun clearRecent() { recent.value = emptyList(); favorites.recentSearches = emptyList() }
}
