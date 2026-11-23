#include "discoverView/discoversession.h"

#include <algorithm>

namespace discoverview {

namespace {

struct ShelfKind {
    const char* key;
    const char* title;
    const char* order;
};

const ShelfKind fixedShelves[] = {
    {"popular", "Popular", "followedCount"},
    {"updated", "Recently updated", "latestUploadedChapter"},
    {"rated", "Top rated", "rating"},
    {"new", "New on MangaDex", "createdAt"},
};

template <typename T>
bool toggle(std::vector<T>& list, const T& value)
{
    const auto it = std::find(list.begin(), list.end(), value);
    if (it == list.end()) list.push_back(value);
    else list.erase(it);
    return true;
}

} // namespace

const std::vector<Demographic>& demographics()
{
    static const std::vector<Demographic> list = {
        {"shounen", "Shounen"}, {"shoujo", "Shoujo"}, {"seinen", "Seinen"}, {"josei", "Josei"}};
    return list;
}

DiscoverSession::DiscoverSession(sources::Source& source) : m_source(source)
{
    rebuildShelfList();
}

void DiscoverSession::rebuildShelfList()
{
    std::vector<Shelf> shelves;
    if (!m_forYouTags.empty()) shelves.push_back({"foryou", "For you", {}, false, {}});
    for (const auto& kind : fixedShelves) shelves.push_back({kind.key, kind.title, {}, false, {}});
    // Keep what is already loaded for shelves that stay.
    for (auto& shelf : shelves)
        for (const auto& old : m_shelves)
            if (old.key == shelf.key) shelf = old;
    m_shelves = std::move(shelves);
}

sources::BrowseQuery DiscoverSession::shelfQuery(const std::string& key) const
{
    sources::BrowseQuery q;
    q.contentRatings = m_ratings;
    if (key == "foryou") {
        // All of the top tags: "any of them" mostly lists the same titles as Popular.
        q.includedTags = m_forYouTags;
        q.anyTag = false;
        q.order = "followedCount";
        // Ask for more than shown: some of them may already be in the library.
        q.limit = 24 + static_cast<int>(std::min<std::size_t>(m_owned.size(), 24));
        return q;
    }
    for (const auto& kind : fixedShelves)
        if (key == kind.key) q.order = kind.order;
    return q;
}

void DiscoverSession::load()
{
    for (std::size_t i = 0; i < m_shelves.size(); ++i) loadShelf(i);

    m_tagsLoading = true;
    m_tagsError.clear();
    changed();
    auto alive = m_lifetime.watch();
    m_source.tags([this, alive](sources::Result<std::vector<sources::Tag>> r) {
        if (alive.expired()) return;
        m_tagsLoading = false;
        if (r.ok()) m_tags = std::move(r.value);
        else m_tagsError = r.error;
        changed();
    });
}

void DiscoverSession::loadShelf(std::size_t index)
{
    if (index >= m_shelves.size()) return;
    Shelf& shelf = m_shelves[index];
    shelf.loading = true;
    shelf.error.clear();
    const std::string key = shelf.key;
    const int generation = ++m_shelfGenerations[key];
    changed();

    auto alive = m_lifetime.watch();
    m_source.browse(shelfQuery(key), 0, [this, alive, key, generation](sources::Result<sources::SearchPage> r) {
        if (alive.expired()) return;
        if (m_shelfGenerations[key] != generation) return;
        for (Shelf& s : m_shelves) {
            if (s.key != key) continue;
            s.loading = false;
            if (!r.ok()) {
                s.error = r.error;
            } else {
                s.items.clear();
                for (auto& item : r.value.items)
                    if (key != "foryou" || !m_owned.count(item.mangaId)) s.items.push_back(std::move(item));
                if (s.items.size() > 24) s.items.resize(24);
            }
            changed();
        }
    });
}

void DiscoverSession::setContentRatings(std::vector<std::string> ratings)
{
    if (ratings.empty()) ratings = {"safe"};
    if (ratings == m_ratings) return;
    m_ratings = std::move(ratings);
    for (std::size_t i = 0; i < m_shelves.size(); ++i) loadShelf(i);
    if (browsing()) startBrowse();
}

void DiscoverSession::setForYou(std::vector<std::string> tagIds, std::set<std::string> owned)
{
    if (tagIds == m_forYouTags && owned == m_owned) return;
    const bool tagsChanged = tagIds != m_forYouTags;
    m_forYouTags = std::move(tagIds);
    m_owned = std::move(owned);
    rebuildShelfList();
    if (!m_forYouTags.empty()) {
        if (tagsChanged || m_shelves.front().items.empty()) {
            loadShelf(0);
        } else {                                          // only the library changed: drop titles now owned
            auto& items = m_shelves.front().items;
            std::erase_if(items, [&](const auto& m) { return m_owned.count(m.mangaId) > 0; });
        }
    }
    changed();
}

bool DiscoverSession::tagSelected(const std::string& tagId) const
{
    return std::find(m_selectedTags.begin(), m_selectedTags.end(), tagId) != m_selectedTags.end();
}

bool DiscoverSession::demographicSelected(const std::string& key) const
{
    return std::find(m_selectedDemographics.begin(), m_selectedDemographics.end(), key) != m_selectedDemographics.end();
}

void DiscoverSession::toggleTag(const std::string& tagId)
{
    toggle(m_selectedTags, tagId);
    startBrowse();
}

void DiscoverSession::toggleDemographic(const std::string& key)
{
    const bool known = std::any_of(demographics().begin(), demographics().end(),
                                   [&](const Demographic& d) { return key == d.key; });
    if (!known) return;
    toggle(m_selectedDemographics, key);
    startBrowse();
}

void DiscoverSession::clearBrowse()
{
    m_selectedTags.clear();
    m_selectedDemographics.clear();
    startBrowse();
}

void DiscoverSession::startBrowse()
{
    ++m_browseGeneration;
    m_results.clear();
    m_resultsError.clear();
    m_resultsLoading = false;
    m_total = 0;
    m_nextOffset = 0;
    if (!browsing()) {
        changed();
        return;
    }
    fetchResults();
}

void DiscoverSession::loadMore()
{
    if (canLoadMore()) fetchResults();
}

void DiscoverSession::fetchResults()
{
    sources::BrowseQuery q;
    q.contentRatings = m_ratings;
    q.includedTags = m_selectedTags;
    q.demographics = m_selectedDemographics;
    q.order = "followedCount";

    m_resultsLoading = true;
    changed();
    const int generation = m_browseGeneration;
    auto alive = m_lifetime.watch();
    m_source.browse(q, m_nextOffset, [this, alive, generation](sources::Result<sources::SearchPage> r) {
        if (alive.expired() || generation != m_browseGeneration) return;
        m_resultsLoading = false;
        if (!r.ok()) {
            m_resultsError = r.error;
        } else {
            for (auto& item : r.value.items) {
                const bool seen = std::any_of(m_results.begin(), m_results.end(),
                                              [&](const auto& m) { return m.mangaId == item.mangaId; });
                if (!seen) m_results.push_back(std::move(item));
            }
            m_total = r.value.total;
            m_nextOffset = std::max(r.value.nextOffset, m_nextOffset + 1);
        }
        changed();
    });
}

void DiscoverSession::changed()
{
    if (onChanged) onChanged();
}

} // namespace discoverview
