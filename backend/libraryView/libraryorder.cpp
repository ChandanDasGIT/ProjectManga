#include "libraryView/libraryorder.h"

#include <algorithm>
#include <cctype>

namespace libraryview {

namespace {

std::string folded(std::string_view s)
{
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string trimmed(std::string_view s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return std::string(s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1));
}

} // namespace

const std::vector<SortOption>& sortOptions()
{
    static const std::vector<SortOption> options = {
        {Sort::Added, "added", "Recently added"},
        {Sort::Title, "title", "Title A–Z"},
        {Sort::MostNew, "new", "Most new chapters"},
        {Sort::Updated, "updated", "Recently updated"},
        {Sort::Read, "read", "Recently read"},
    };
    return options;
}

Sort sortFromKey(std::string_view key)
{
    for (const auto& o : sortOptions())
        if (key == o.key) return o.sort;
    return Sort::Added;
}

Kind kindFromKey(std::string_view key)
{
    if (key == "manga") return Kind::Manga;
    if (key == "novel") return Kind::Novel;
    return Kind::All;
}

const char* keyOf(Kind kind)
{
    switch (kind) {
    case Kind::Manga: return "manga";
    case Kind::Novel: return "novel";
    case Kind::All: break;
    }
    return "all";
}

std::vector<const library::Entry*> arrange(const std::vector<library::Entry>& entries, std::string_view filter, Sort sort,
                                           Kind kind, const std::function<bool(const std::string& sourceId)>& isNovel)
{
    const std::string wanted = folded(trimmed(filter));
    std::vector<const library::Entry*> out;
    for (const auto& e : entries) {
        if (kind != Kind::All && isNovel && isNovel(e.sourceId) != (kind == Kind::Novel)) continue;
        if (wanted.empty() || folded(e.title).find(wanted) != std::string::npos) out.push_back(&e);
    }

    // Ties (and entries never read or updated) fall back to the newest added first.
    auto newerAdded = [](const library::Entry* a, const library::Entry* b) { return a->addedMs > b->addedMs; };
    switch (sort) {
    case Sort::Added:
        std::stable_sort(out.begin(), out.end(), newerAdded);
        break;
    case Sort::Title:
        std::stable_sort(out.begin(), out.end(), [](const auto* a, const auto* b) { return folded(a->title) < folded(b->title); });
        break;
    case Sort::MostNew:
        std::stable_sort(out.begin(), out.end(), [&](const auto* a, const auto* b) {
            return a->newCount() != b->newCount() ? a->newCount() > b->newCount() : newerAdded(a, b);
        });
        break;
    case Sort::Updated:
        std::stable_sort(out.begin(), out.end(), [&](const auto* a, const auto* b) {
            return a->lastNewMs != b->lastNewMs ? a->lastNewMs > b->lastNewMs : newerAdded(a, b);
        });
        break;
    case Sort::Read:
        std::stable_sort(out.begin(), out.end(), [&](const auto* a, const auto* b) {
            return a->lastReadMs != b->lastReadMs ? a->lastReadMs > b->lastReadMs : newerAdded(a, b);
        });
        break;
    }
    return out;
}

} // namespace libraryview
