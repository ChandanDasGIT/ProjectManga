#include "common/sources/webLink/weblinksource.h"

#include "common/sources/webLink/htmltokenizer.h"
#include "common/sources/webLink/pageguess.h"
#include "common/sources/booktext.h"

namespace weblink {

WebLinkSource::WebLinkSource(platform::HttpClient& http, platform::Scheduler& scheduler)
    : m_http(http), m_scheduler(scheduler), m_limiter(scheduler, 2, 1000)
{
}

void WebLinkSource::fetch(const std::string& url, std::function<void(sources::Result<std::string>)> done)
{
    using Page = sources::Result<std::string>;
    if (!isWebAddress(url)) return done(Page::failure("Not a web address (it must start with http:// or https://)"));
    if (url == m_lastUrl && m_scheduler.nowMs() - m_lastAt < 60'000) {
        Page cached;
        cached.value = std::move(m_lastBody);
        m_lastUrl.clear();
        m_lastBody.clear();
        return done(std::move(cached));
    }
    m_limiter.run([this, url, done = std::move(done)]() mutable {
        platform::HttpRequest request{url, {{"Accept", "text/html,application/xhtml+xml"}}};
        m_http.get(request, [this, url, done = std::move(done)](platform::HttpResponse response) {
            if (!response.ok()) return done(Page::failure("Could not load the page (" + response.problem() + ")"));
            m_lastUrl = url;
            m_lastBody = response.body;
            m_lastAt = m_scheduler.nowMs();
            Page page;
            page.value = std::move(response.body);
            done(std::move(page));
        });
    });
}

void WebLinkSource::search(const std::string&, int, const std::vector<std::string>&,
                           std::function<void(sources::Result<sources::SearchPage>)> done)
{
    done(sources::Result<sources::SearchPage>::failure("Web links cannot be searched; add the site in the library"));
}

void WebLinkSource::details(const std::string& address, std::function<void(sources::Result<sources::MangaDetails>)> done)
{
    fetch(address, [this, address, done = std::move(done)](sources::Result<std::string> page) mutable {
        using Details = sources::Result<sources::MangaDetails>;
        if (!page.ok()) return done(Details::failure(page.error));
        const auto tokens = tokenize(page.value);
        const PageInfo info = guessInfo(tokens, address);
        Details r;
        r.value.mangaId = address;
        r.value.title = info.title.empty() ? hostOf(address) : info.title;
        r.value.coverUrl = info.coverUrl;
        r.value.description = info.description;
        r.value.subtitle = hostOf(address);
        if (!r.value.coverUrl.empty()) return done(std::move(r));

        // No cover on the page: use the first page of the first chapter.
        const auto chapters = guessChapters(tokens, address);
        if (chapters.empty()) return done(std::move(r));
        const std::string firstChapter = chapters.front().id;
        fetch(firstChapter, [firstChapter, r = std::move(r), done = std::move(done)](sources::Result<std::string> chapter) mutable {
            if (chapter.ok()) {
                const auto images = guessImages(tokenize(chapter.value), firstChapter);
                if (!images.empty()) r.value.coverUrl = images.front();
            }
            done(std::move(r));
        });
    });
}

void WebLinkSource::chapters(const std::string& address,
                             std::function<void(sources::Result<std::vector<sources::Chapter>>)> done)
{
    fetch(address, [address, done = std::move(done)](sources::Result<std::string> page) {
        using Chapters = sources::Result<std::vector<sources::Chapter>>;
        if (!page.ok()) return done(Chapters::failure(page.error));
        Chapters r;
        r.value = guessChapters(tokenize(page.value), address);
        if (r.value.empty()) return done(Chapters::failure("No chapter links found on this page"));
        done(std::move(r));
    });
}

void WebLinkSource::pages(const sources::Chapter& chapter, std::function<void(sources::Result<sources::PageList>)> done)
{
    const std::string address = chapter.id;
    fetch(address, [address, done = std::move(done)](sources::Result<std::string> page) {
        using Pages = sources::Result<sources::PageList>;
        if (!page.ok()) return done(Pages::failure(page.error));
        Pages r;
        r.value.urls = guessImages(tokenize(page.value), address);
        r.value.referer = address;
        if (r.value.urls.empty()) return done(Pages::failure("No page images found on this chapter page"));
        done(std::move(r));
    });
}

void WebNovelSource::pages(const sources::Chapter&, std::function<void(sources::Result<sources::PageList>)> done)
{
    done(sources::Result<sources::PageList>::failure("Novel chapters are text"));
}

void WebNovelSource::chapterText(const sources::Chapter& chapter, std::function<void(sources::Result<sources::ChapterText>)> done)
{
    fetch(chapter.id, [done = std::move(done)](sources::Result<std::string> page) {
        using Text = sources::Result<sources::ChapterText>;
        if (!page.ok()) return done(Text::failure(page.error));
        Text r;
        r.value = sources::pageText(page.value, tokenize(page.value));
        if (r.value.paragraphs.empty()) return done(Text::failure("No story text found on this chapter page"));
        done(std::move(r));
    });
}

} // namespace weblink
