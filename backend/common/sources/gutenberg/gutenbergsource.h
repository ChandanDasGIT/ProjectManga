#pragma once

#include "common/platform/httpclient.h"
#include "common/platform/ratelimiter.h"
#include "common/sources/booktext.h"
#include "common/sources/gutenberg/gutenbergcatalog.h"
#include "common/sources/source.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

// Project Gutenberg's public-domain books in English. Search and book details come from Gutenberg's
// own catalogue (fetched weekly into `catalogFolder`); the most downloaded books from Gutendex, or
// the most recently added ones when Gutendex does not answer; the text from the book's HTML edition
// (gutenberg.org, else its official mirror), split into chapters.
namespace gutenberg {

class GutenbergSource : public sources::Source {
public:
    // catalogFolder: where the catalogue is kept between runs; empty: no catalogue (Gutendex only).
    GutenbergSource(platform::HttpClient& http, platform::Scheduler& scheduler, std::string catalogFolder = {});
    ~GutenbergSource() override;

    std::string id() const override { return "gutenberg"; }
    std::string name() const override { return "Project Gutenberg"; }
    std::string kind() const override { return "novel"; }
    bool updates() const override { return false; }

    void search(const std::string& query, int offset, const std::vector<std::string>& contentRatings,
                std::function<void(sources::Result<sources::SearchPage>)> done) override;
    // The most downloaded books (the query's order and tags do not apply here).
    void browse(const sources::BrowseQuery& query, int offset,
                std::function<void(sources::Result<sources::SearchPage>)> done) override;
    void details(const std::string& bookId, std::function<void(sources::Result<sources::MangaDetails>)> done) override;
    void chapters(const std::string& bookId,
                  std::function<void(sources::Result<std::vector<sources::Chapter>>)> done) override;
    void pages(const sources::Chapter& chapter, std::function<void(sources::Result<sources::PageList>)> done) override;
    void chapterText(const sources::Chapter& chapter, std::function<void(sources::Result<sources::ChapterText>)> done) override;

    static constexpr int pageSize = 32;     // Gutendex's page size
    static constexpr std::int64_t catalogMaxAgeMs = 7LL * 24 * 3600 * 1000;

private:
    void fetch(const std::string& url, std::function<void(platform::HttpResponse)> done);
    void list(const std::string& query, int offset, std::function<void(sources::Result<sources::SearchPage>)> done);
    void book(const std::string& bookId, std::function<void(sources::Result<std::vector<sources::BookChapter>>)> done);
    // The book's HTML edition: gutenberg.org first, the mirror when that fails.
    void bookPage(const std::string& bookId, std::function<void(sources::Result<std::string>)> done);
    // Calls `ready` with the catalogue once it is loaded (null when it cannot be had).
    void withCatalog(std::function<void(const Catalog*)> ready);
    void loadCatalog();
    void parseCatalog(std::string gz, bool fresh);
    void catalogDone(std::shared_ptr<Catalog> catalog, std::string why);
    void gutendexDetails(const std::string& bookId, std::function<void(sources::Result<sources::MangaDetails>)> done);
    static sources::SearchPage page(const std::vector<const CatalogBook*>& books, int offset, std::string listName);

    platform::HttpClient& m_http;
    platform::Scheduler& m_scheduler;
    platform::RateLimiter m_limiter;
    std::string m_catalogFolder;
    enum class CatalogState { Unloaded, Loading, Ready, Failed } m_catalogState = CatalogState::Unloaded;
    std::shared_ptr<Catalog> m_catalog;
    std::string m_catalogError;
    std::vector<std::function<void(const Catalog*)>> m_waiting;
    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);
    std::string m_bookId;
    std::vector<sources::BookChapter> m_book;
    std::string m_pageId;                   // the last HTML edition fetched, kept for details + chapters
    std::string m_page;
};

} // namespace gutenberg
