#pragma once

#include "common/platform/httpclient.h"
#include "common/platform/ratelimiter.h"
#include "common/sources/source.h"

#include <memory>

// MangaDex through its public API: search, a manga, its English chapters, and the image server
// links of a chapter. Stays inside MangaDex's limits (5 requests a second; 40 image-server
// lookups a minute).
namespace mangadex {

class MangaDexSource : public sources::Source {
public:
    MangaDexSource(platform::HttpClient& http, platform::Scheduler& scheduler);

    std::string id() const override { return "mangadex"; }
    std::string name() const override { return "MangaDex"; }

    void search(const std::string& query, int offset, const std::vector<std::string>& contentRatings,
                std::function<void(sources::Result<sources::SearchPage>)> done) override;
    void details(const std::string& mangaId,
                 std::function<void(sources::Result<sources::MangaDetails>)> done) override;
    void chapters(const std::string& mangaId,
                  std::function<void(sources::Result<std::vector<sources::Chapter>>)> done) override;
    void pages(const sources::Chapter& chapter,
               std::function<void(sources::Result<sources::PageList>)> done) override;
    void browse(const sources::BrowseQuery& query, int offset,
                std::function<void(sources::Result<sources::SearchPage>)> done) override;
    void tags(std::function<void(sources::Result<std::vector<sources::Tag>>)> done) override;

    static constexpr int searchPageSize = 24;
    static constexpr int feedPageSize = 500;

private:
    void request(platform::RateLimiter& limiter, const std::string& url,
                 std::function<void(platform::HttpResponse)> done);
    void loadFeed(const std::string& mangaId, int offset, std::shared_ptr<std::vector<sources::Chapter>> collected,
                  std::function<void(sources::Result<std::vector<sources::Chapter>>)> done);

    platform::HttpClient& m_http;
    platform::RateLimiter m_api;
    platform::RateLimiter m_atHome;
};

// The text of a request that failed: MangaDex's own error message when it sent one.
std::string failureText(const platform::HttpResponse& response);
// Percent-encodes everything but letters, digits and -._~
std::string percentEncode(std::string_view text);

} // namespace mangadex
