#include "discoverView/recommend.h"

#include <algorithm>
#include <map>

namespace discoverview {

std::vector<std::string> tagsToKeep(const std::vector<sources::Tag>& tags)
{
    std::vector<std::string> out;
    for (const auto& t : tags)
        if ((t.group == "genre" || t.group == "theme") && !t.id.empty()) out.push_back(t.id);
    return out;
}

std::vector<std::string> topTags(const std::vector<library::Entry>& entries, std::size_t count)
{
    struct Score {
        int count = 0;
        std::int64_t latestAdded = 0;
    };
    std::map<std::string, Score> scores;
    for (const auto& e : entries)
        for (const auto& tag : e.tags) {
            auto& s = scores[tag];
            ++s.count;
            s.latestAdded = std::max(s.latestAdded, e.addedMs);
        }

    std::vector<std::pair<std::string, Score>> ranked(scores.begin(), scores.end());
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.second.count != b.second.count) return a.second.count > b.second.count;
        return a.second.latestAdded > b.second.latestAdded;
    });
    std::vector<std::string> out;
    for (std::size_t i = 0; i < ranked.size() && i < count; ++i) out.push_back(ranked[i].first);
    return out;
}

std::set<std::string> ownedIds(const std::vector<library::Entry>& entries, const std::string& sourceId)
{
    std::set<std::string> out;
    for (const auto& e : entries)
        if (e.sourceId == sourceId) out.insert(e.mangaId);
    return out;
}

} // namespace discoverview
