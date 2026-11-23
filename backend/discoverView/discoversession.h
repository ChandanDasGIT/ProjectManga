#pragma once

#include "common/platform/httpclient.h"
#include "common/sources/source.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// The Discover page's state: rows ("shelves") of titles by popularity, rating and so on, the
// categories to browse by, and the titles of the chosen categories, all with English chapters and
// within the chosen content ratings.
namespace discoverview {

struct Shelf {
    std::string key;                // "foryou", "popular", "updated", "rated", "new"
    std::string title;
    std::vector<sources::MangaSummary> items;
    bool loading = false;
    std::string error;
};

struct Demographic {
    const char* key;                // "shounen"
    const char* label;              // "Shounen"
};
const std::vector<Demographic>& demographics();

class DiscoverSession {
public:
    explicit DiscoverSession(sources::Source& source);

    // Loads the shelves and the category list (again).
    void load();
    // A change reloads the shelves and the chosen categories' titles.
    void setContentRatings(std::vector<std::string> ratings);
    // "For you": popular titles with all of these tags, without the ones in `owned`. No tags: no shelf.
    void setForYou(std::vector<std::string> tagIds, std::set<std::string> owned);

    const std::vector<Shelf>& shelves() const { return m_shelves; }
    const std::vector<sources::Tag>& tags() const { return m_tags; }
    bool tagsLoading() const { return m_tagsLoading; }
    const std::string& tagsError() const { return m_tagsError; }

    // Browsing by category: titles with all chosen tags and any chosen demographic.
    void toggleTag(const std::string& tagId);
    void toggleDemographic(const std::string& key);
    void clearBrowse();
    bool browsing() const { return !m_selectedTags.empty() || !m_selectedDemographics.empty(); }
    bool tagSelected(const std::string& tagId) const;
    bool demographicSelected(const std::string& key) const;
    const std::vector<sources::MangaSummary>& results() const { return m_results; }
    bool resultsLoading() const { return m_resultsLoading; }
    const std::string& resultsError() const { return m_resultsError; }
    bool canLoadMore() const { return browsing() && !m_resultsLoading && m_resultsError.empty() && m_nextOffset < m_total; }
    void loadMore();

    std::function<void()> onChanged;

private:
    void loadShelf(std::size_t index);
    sources::BrowseQuery shelfQuery(const std::string& key) const;
    void startBrowse();
    void fetchResults();
    void rebuildShelfList();
    void changed();

    sources::Source& m_source;
    std::vector<std::string> m_ratings{"safe"};
    std::vector<std::string> m_forYouTags;
    std::set<std::string> m_owned;
    std::vector<Shelf> m_shelves;
    std::map<std::string, int> m_shelfGenerations;   // by shelf key

    std::vector<sources::Tag> m_tags;
    bool m_tagsLoading = false;
    std::string m_tagsError;

    std::vector<std::string> m_selectedTags;
    std::vector<std::string> m_selectedDemographics;
    std::vector<sources::MangaSummary> m_results;
    bool m_resultsLoading = false;
    std::string m_resultsError;
    int m_total = 0;
    int m_nextOffset = 0;
    int m_browseGeneration = 0;
    platform::Lifetime m_lifetime;
};

} // namespace discoverview
