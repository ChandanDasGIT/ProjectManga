#pragma once

#include "common/platform/httpclient.h"
#include "common/platform/ratelimiter.h"
#include "common/sources/source.h"

#include <cstdint>
#include <string>

// A site the user added by its address, read with the rules in pageguess.h. The manga id is the
// address of the page that lists the chapters; a chapter id is the chapter page's address.
namespace weblink {

class WebLinkSource : public sources::Source {
public:
    WebLinkSource(platform::HttpClient& http, platform::Scheduler& scheduler);

    std::string id() const override { return "web"; }
    std::string name() const override { return "Web link"; }

    void search(const std::string& query, int offset, const std::vector<std::string>& contentRatings,
                std::function<void(sources::Result<sources::SearchPage>)> done) override;
    void details(const std::string& address,
                 std::function<void(sources::Result<sources::MangaDetails>)> done) override;
    void chapters(const std::string& address,
                  std::function<void(sources::Result<std::vector<sources::Chapter>>)> done) override;
    void pages(const sources::Chapter& chapter,
               std::function<void(sources::Result<sources::PageList>)> done) override;

protected:
    // The page's HTML. The last page read serves one more read within a minute, so asking for the
    // chapters and then the details of an address costs one request; later reads fetch it fresh.
    void fetch(const std::string& url, std::function<void(sources::Result<std::string>)> done);

private:
    platform::HttpClient& m_http;
    platform::Scheduler& m_scheduler;
    platform::RateLimiter m_limiter;
    std::string m_lastUrl;
    std::string m_lastBody;
    std::int64_t m_lastAt = 0;
};

// A novel site the user added by its address: the same chapter list as WebLinkSource, but a
// chapter is the story text on its page instead of images.
class WebNovelSource : public WebLinkSource {
public:
    using WebLinkSource::WebLinkSource;

    std::string id() const override { return "webnovel"; }
    std::string name() const override { return "Novel site"; }
    std::string kind() const override { return "novel"; }

    void pages(const sources::Chapter& chapter, std::function<void(sources::Result<sources::PageList>)> done) override;
    void chapterText(const sources::Chapter& chapter, std::function<void(sources::Result<sources::ChapterText>)> done) override;
};

} // namespace weblink
