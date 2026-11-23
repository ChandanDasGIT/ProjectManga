#include "common/sources/standardEbooks/standardebookssource.h"

#include "common/sources/mangaDex/mangadexsource.h"
#include "common/sources/standardEbooks/standardebooksparse.h"

#include <algorithm>

namespace standardebooks {

namespace {

const std::string site = "https://standardebooks.org";

std::string problem(const platform::HttpResponse& r)
{
    return "Could not reach Standard Ebooks (" + r.problem() + ")";
}

// "/ebooks/a/b#3" -> ("/ebooks/a/b", 3); -1 when there is no number.
std::pair<std::string, int> split(const std::string& chapterId)
{
    const std::size_t hash = chapterId.rfind('#');
    if (hash == std::string::npos) return {chapterId, -1};
    int n = -1;
    try {
        n = std::stoi(chapterId.substr(hash + 1));
    } catch (...) {
        n = -1;
    }
    return {chapterId.substr(0, hash), n};
}

} // namespace

StandardEbooksSource::StandardEbooksSource(platform::HttpClient& http, platform::Scheduler& scheduler)
    : m_http(http), m_limiter(scheduler, 2, 1000)
{
}

void StandardEbooksSource::fetch(const std::string& url, std::function<void(platform::HttpResponse)> done)
{
    m_limiter.run([this, url, done = std::move(done)]() mutable {
        m_http.get({url, {{"Accept", "text/html,application/xhtml+xml"}}}, std::move(done));
    });
}

void StandardEbooksSource::list(const std::string& query, int offset,
                                std::function<void(sources::Result<sources::SearchPage>)> done)
{
    const int page = std::max(0, offset) / pageSize + 1;
    const std::string url = site + "/ebooks?" + query + "&per-page=" + std::to_string(pageSize) + "&page=" + std::to_string(page);
    const int start = (page - 1) * pageSize;
    fetch(url, [start, done = std::move(done)](platform::HttpResponse r) {
        if (!r.ok()) return done(sources::Result<sources::SearchPage>::failure(problem(r)));
        done(parseList(r.body, start));
    });
}

void StandardEbooksSource::search(const std::string& query, int offset, const std::vector<std::string>&,
                                  std::function<void(sources::Result<sources::SearchPage>)> done)
{
    list("query=" + mangadex::percentEncode(query) + "&sort=relevance", offset, std::move(done));
}

void StandardEbooksSource::browse(const sources::BrowseQuery&, int offset,
                                  std::function<void(sources::Result<sources::SearchPage>)> done)
{
    list("sort=newest", offset, std::move(done));
}

void StandardEbooksSource::details(const std::string& bookId, std::function<void(sources::Result<sources::MangaDetails>)> done)
{
    if (!isBookId(bookId)) return done(sources::Result<sources::MangaDetails>::failure("Not a Standard Ebooks book"));
    fetch(site + bookId, [bookId, done = std::move(done)](platform::HttpResponse r) {
        if (!r.ok()) return done(sources::Result<sources::MangaDetails>::failure(problem(r)));
        done(parseBook(r.body, bookId));
    });
}

void StandardEbooksSource::book(const std::string& bookId,
                                std::function<void(sources::Result<std::vector<sources::BookChapter>>)> done)
{
    using Book = sources::Result<std::vector<sources::BookChapter>>;
    if (!isBookId(bookId)) return done(Book::failure("Not a Standard Ebooks book"));
    if (bookId == m_bookId) {
        Book cached;
        cached.value = m_book;
        return done(std::move(cached));
    }
    fetch(site + bookId + "/text/single-page", [this, bookId, done = std::move(done)](platform::HttpResponse r) {
        if (!r.ok()) return done(Book::failure(problem(r)));
        Book b;
        b.value = sources::splitBook(weblink::tokenize(r.body));
        if (b.value.empty()) return done(Book::failure("No chapters found in this book"));
        m_bookId = bookId;
        m_book = b.value;
        done(std::move(b));
    });
}

void StandardEbooksSource::chapters(const std::string& bookId,
                                    std::function<void(sources::Result<std::vector<sources::Chapter>>)> done)
{
    book(bookId, [bookId, done = std::move(done)](sources::Result<std::vector<sources::BookChapter>> b) {
        using Chapters = sources::Result<std::vector<sources::Chapter>>;
        if (!b.ok()) return done(Chapters::failure(b.error));
        Chapters r;
        for (std::size_t i = 0; i < b.value.size(); ++i) {
            sources::Chapter c;
            c.id = bookId + "#" + std::to_string(i);
            c.number = std::to_string(i + 1);
            c.numberValue = static_cast<double>(i + 1);
            c.title = b.value[i].title;
            c.titleOnly = true;
            r.value.push_back(std::move(c));
        }
        done(std::move(r));
    });
}

void StandardEbooksSource::pages(const sources::Chapter&, std::function<void(sources::Result<sources::PageList>)> done)
{
    done(sources::Result<sources::PageList>::failure("Standard Ebooks books are text"));
}

void StandardEbooksSource::chapterText(const sources::Chapter& chapter,
                                       std::function<void(sources::Result<sources::ChapterText>)> done)
{
    const auto [bookId, index] = split(chapter.id);
    book(bookId, [index, done = std::move(done)](sources::Result<std::vector<sources::BookChapter>> b) {
        using Text = sources::Result<sources::ChapterText>;
        if (!b.ok()) return done(Text::failure(b.error));
        if (index < 0 || index >= static_cast<int>(b.value.size())) return done(Text::failure("Chapter not found in this book"));
        Text r;
        r.value.title = b.value[static_cast<std::size_t>(index)].title;
        r.value.paragraphs = b.value[static_cast<std::size_t>(index)].paragraphs;
        done(std::move(r));
    });
}

} // namespace standardebooks
