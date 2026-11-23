#pragma once

#include "common/platform/httpclient.h"
#include "common/platform/ratelimiter.h"
#include "common/sources/booktext.h"
#include "common/sources/source.h"

#include <cstdint>
#include <string>

// Standard Ebooks (standardebooks.org): free, carefully produced public-domain books. Search and
// the newest releases come from its ebook list pages; a book is read from its single-page text,
// split into chapters.
namespace standardebooks {

class StandardEbooksSource : public sources::Source {
public:
    StandardEbooksSource(platform::HttpClient& http, platform::Scheduler& scheduler);

    std::string id() const override { return "standardebooks"; }
    std::string name() const override { return "Standard Ebooks"; }
    std::string kind() const override { return "novel"; }
    bool updates() const override { return false; }

    void search(const std::string& query, int offset, const std::vector<std::string>& contentRatings,
                std::function<void(sources::Result<sources::SearchPage>)> done) override;
    // The newest releases (the query's order and tags do not apply here).
    void browse(const sources::BrowseQuery& query, int offset,
                std::function<void(sources::Result<sources::SearchPage>)> done) override;
    void details(const std::string& bookId, std::function<void(sources::Result<sources::MangaDetails>)> done) override;
    void chapters(const std::string& bookId,
                  std::function<void(sources::Result<std::vector<sources::Chapter>>)> done) override;
    void pages(const sources::Chapter& chapter, std::function<void(sources::Result<sources::PageList>)> done) override;
    void chapterText(const sources::Chapter& chapter, std::function<void(sources::Result<sources::ChapterText>)> done) override;

    static constexpr int pageSize = 24;

private:
    void fetch(const std::string& url, std::function<void(platform::HttpResponse)> done);
    // The book's chapters, from the one book kept in memory or fetched.
    void book(const std::string& bookId, std::function<void(sources::Result<std::vector<sources::BookChapter>>)> done);
    void list(const std::string& query, int offset, std::function<void(sources::Result<sources::SearchPage>)> done);

    platform::HttpClient& m_http;
    platform::RateLimiter m_limiter;
    std::string m_bookId;
    std::vector<sources::BookChapter> m_book;
};

} // namespace standardebooks
