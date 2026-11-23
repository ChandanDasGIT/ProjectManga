#pragma once

#include "common/sources/mangatypes.h"

#include <functional>
#include <string>
#include <vector>

// A place manga come from. Each one answers the same four questions; the answers arrive later,
// through the callback, on the same thread.
namespace sources {

class Source {
public:
    virtual ~Source() = default;

    virtual std::string id() const = 0;            // stored in the library: "mangadex", "web"
    virtual std::string name() const = 0;          // shown to the user: "MangaDex", "Web link"
    // "manga" (pages are images) or "novel" (chapters are text).
    virtual std::string kind() const { return "manga"; }
    // False for finished books: the update checker leaves them alone.
    virtual bool updates() const { return true; }

    // Sources that cannot search answer with an error. contentRatings: which ratings to include
    // ("safe", "suggestive", "erotica"); sources without ratings ignore it.
    virtual void search(const std::string& query, int offset, const std::vector<std::string>& contentRatings,
                        std::function<void(Result<SearchPage>)> done) = 0;
    virtual void details(const std::string& mangaId, std::function<void(Result<MangaDetails>)> done) = 0;
    // All chapters, oldest first.
    virtual void chapters(const std::string& mangaId, std::function<void(Result<std::vector<Chapter>>)> done) = 0;
    virtual void pages(const Chapter& chapter, std::function<void(Result<PageList>)> done) = 0;

    // Listing by popularity, tags and so on (Discover). Sources that cannot answer with an error.
    virtual void browse(const BrowseQuery&, int, std::function<void(Result<SearchPage>)> done)
    {
        done(Result<SearchPage>::failure(name() + " cannot be browsed"));
    }
    // A novel chapter's text. Manga sources answer with an error.
    virtual void chapterText(const Chapter&, std::function<void(Result<ChapterText>)> done)
    {
        done(Result<ChapterText>::failure(name() + " has no chapter text"));
    }
    // The tags browse() can filter by.
    virtual void tags(std::function<void(Result<std::vector<Tag>>)> done)
    {
        done(Result<std::vector<Tag>>::failure(name() + " has no tags"));
    }
};

} // namespace sources
