#include "common/sources/gutenberg/gutenbergsource.h"

#include "common/sources/gutenberg/gutendexparse.h"
#include "common/sources/mangaDex/mangadexsource.h"
#include "common/ziparchive.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include <algorithm>

namespace gutenberg {

namespace {

const std::string gutendex = "https://gutendex.com/books/";

std::string problem(const std::string& who, const platform::HttpResponse& r)
{
    return "Could not reach " + who + " (" + r.problem() + ")";
}

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

const char* const catalogUrls[] = {"https://www.gutenberg.org/cache/epub/feeds/pg_catalog.csv.gz",
                                   "https://gutenberg.pglaf.org/cache/epub/feeds/pg_catalog.csv.gz"};

GutenbergSource::GutenbergSource(platform::HttpClient& http, platform::Scheduler& scheduler, std::string catalogFolder)
    : m_http(http), m_scheduler(scheduler), m_limiter(scheduler, 1, 1000), m_catalogFolder(std::move(catalogFolder))
{
}

GutenbergSource::~GutenbergSource()
{
    m_alive->store(false);
}

void GutenbergSource::withCatalog(std::function<void(const Catalog*)> ready)
{
    if (m_catalogState == CatalogState::Ready) return ready(m_catalog.get());
    if (m_catalogState == CatalogState::Failed || m_catalogFolder.empty()) return ready(nullptr);
    m_waiting.push_back(std::move(ready));
    if (m_catalogState == CatalogState::Unloaded) loadCatalog();
}

void GutenbergSource::loadCatalog()
{
    namespace fs = std::filesystem;
    m_catalogState = CatalogState::Loading;
    const std::string path = m_catalogFolder + "/pg_catalog.csv.gz";
    std::error_code ec;
    const bool have = fs::exists(path, ec);
    std::string saved;
    if (have) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream text;
        text << in.rdbuf();
        saved = text.str();
    }
    const auto age = have ? std::chrono::duration_cast<std::chrono::milliseconds>(
                                fs::file_time_type::clock::now() - fs::last_write_time(path, ec)).count()
                          : catalogMaxAgeMs + 1;
    if (have && age < catalogMaxAgeMs) return parseCatalog(std::move(saved), false);

    // Fetch it (gutenberg.org, else the mirror); a failed fetch falls back to the copy on disk.
    auto attempt = std::make_shared<std::function<void(std::size_t)>>();
    *attempt = [this, attempt, saved = std::move(saved)](std::size_t index) mutable {
        if (index >= std::size(catalogUrls)) {
            if (!saved.empty()) return parseCatalog(std::move(saved), false);
            return catalogDone(nullptr, "Could not fetch Project Gutenberg's catalogue");
        }
        fetch(catalogUrls[index], [this, attempt, index, alive = m_alive](platform::HttpResponse r) {
            if (!alive->load()) return;
            // A gzip file, not some error page that happened to say 200.
            if (r.ok() && r.body.size() > 2 && r.body[0] == '\x1f' && r.body[1] == '\x8b')
                return parseCatalog(std::move(r.body), true);
            (*attempt)(index + 1);
        });
    };
    (*attempt)(0);
}

void GutenbergSource::parseCatalog(std::string gz, bool fresh)
{
    // About 20 MB of text: unpacked and read on a worker thread.
    std::thread([this, gz = std::move(gz), fresh, folder = m_catalogFolder, alive = m_alive] {
        std::string csv;
        std::string why = archive::gunzip(gz, csv);
        auto catalog = std::make_shared<Catalog>();
        if (why.empty() && !catalog->load(csv)) why = "the catalogue file is not readable";
        if (why.empty() && fresh) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::create_directories(folder, ec);
            std::ofstream out(folder + "/pg_catalog.csv.gz.tmp", std::ios::binary | std::ios::trunc);
            out.write(gz.data(), static_cast<std::streamsize>(gz.size()));
            out.close();
            fs::rename(folder + "/pg_catalog.csv.gz.tmp", folder + "/pg_catalog.csv.gz", ec);
        }
        if (!alive->load()) return;
        m_scheduler.post([this, alive, catalog = why.empty() ? catalog : nullptr, why] {
            if (alive->load()) catalogDone(catalog, why);
        });
    }).detach();
}

void GutenbergSource::catalogDone(std::shared_ptr<Catalog> catalog, std::string why)
{
    m_catalog = std::move(catalog);
    m_catalogState = m_catalog ? CatalogState::Ready : CatalogState::Failed;
    m_catalogError = why;
    auto waiting = std::move(m_waiting);
    m_waiting.clear();
    for (auto& ready : waiting) ready(m_catalog.get());
}

sources::SearchPage GutenbergSource::page(const std::vector<const CatalogBook*>& books, int offset, std::string listName)
{
    sources::SearchPage p;
    offset = std::max(0, offset);
    for (std::size_t i = static_cast<std::size_t>(offset); i < books.size() && p.items.size() < static_cast<std::size_t>(pageSize); ++i)
        p.items.push_back(Catalog::summary(*books[i]));
    p.total = static_cast<int>(books.size());
    p.nextOffset = offset + static_cast<int>(p.items.size());
    p.listName = std::move(listName);
    return p;
}

void GutenbergSource::fetch(const std::string& url, std::function<void(platform::HttpResponse)> done)
{
    m_limiter.run([this, url, done = std::move(done)]() mutable { m_http.get({url, {}}, std::move(done)); });
}

void GutenbergSource::list(const std::string& query, int offset, std::function<void(sources::Result<sources::SearchPage>)> done)
{
    const int page = std::max(0, offset) / pageSize + 1;
    const int start = (page - 1) * pageSize;
    fetch(gutendex + "?languages=en" + query + "&page=" + std::to_string(page), [start, done = std::move(done)](platform::HttpResponse r) {
        if (!r.ok()) return done(sources::Result<sources::SearchPage>::failure(problem("Gutendex", r)));
        done(parseList(r.body, start));
    });
}

void GutenbergSource::search(const std::string& query, int offset, const std::vector<std::string>&,
                             std::function<void(sources::Result<sources::SearchPage>)> done)
{
    withCatalog([this, query, offset, done = std::move(done)](const Catalog* catalog) mutable {
        if (!catalog) return list("&search=" + mangadex::percentEncode(query), offset, std::move(done));
        sources::Result<sources::SearchPage> r;
        r.value = page(catalog->search(query), offset, {});
        done(std::move(r));
    });
}

void GutenbergSource::browse(const sources::BrowseQuery&, int offset, std::function<void(sources::Result<sources::SearchPage>)> done)
{
    list("", offset, [this, offset, done = std::move(done)](sources::Result<sources::SearchPage> popular) mutable {
        if (popular.ok()) return done(std::move(popular));
        // Gutendex is down: the newest books from the catalogue instead.
        withCatalog([offset, why = popular.error, done = std::move(done)](const Catalog* catalog) {
            if (!catalog) return done(sources::Result<sources::SearchPage>::failure(why));
            sources::Result<sources::SearchPage> r;
            r.value = page(catalog->recent(400), offset, "Recently added (the most-downloaded list is not answering)");
            done(std::move(r));
        });
    });
}

void GutenbergSource::details(const std::string& bookId, std::function<void(sources::Result<sources::MangaDetails>)> done)
{
    using Details = sources::Result<sources::MangaDetails>;
    if (!isBookId(bookId)) return done(Details::failure("Not a Project Gutenberg book number"));
    withCatalog([this, bookId, done = std::move(done)](const Catalog* catalog) mutable {
        const CatalogBook* book = catalog ? catalog->find(std::stoi(bookId)) : nullptr;
        if (!book) return gutendexDetails(bookId, std::move(done));
        Details d;
        d.value = Catalog::details(*book);
        done(std::move(d));
    });
}

void GutenbergSource::gutendexDetails(const std::string& bookId, std::function<void(sources::Result<sources::MangaDetails>)> done)
{
    using Details = sources::Result<sources::MangaDetails>;
    fetch(gutendex + bookId + "/", [this, bookId, done = std::move(done)](platform::HttpResponse r) mutable {
        if (r.ok()) {
            auto parsed = parseBook(r.body);
            if (parsed.ok()) return done(std::move(parsed));
        }
        // Gutendex is down or answered oddly: read what the book itself says.
        const std::string why = problem("Gutendex", r);
        bookPage(bookId, [bookId, why, done = std::move(done)](sources::Result<std::string> page) {
            if (!page.ok()) return done(Details::failure(why));
            Details d;
            d.value = detailsFromBook(page.value, bookId);
            done(std::move(d));
        });
    });
}

void GutenbergSource::bookPage(const std::string& bookId, std::function<void(sources::Result<std::string>)> done)
{
    using Page = sources::Result<std::string>;
    if (bookId == m_pageId) {
        Page cached;
        cached.value = m_page;
        return done(std::move(cached));
    }
    fetch(htmlUrl(bookId), [this, bookId, done = std::move(done)](platform::HttpResponse r) mutable {
        auto keep = [this, bookId](std::string body) {
            m_pageId = bookId;
            m_page = body;
            Page p;
            p.value = std::move(body);
            return p;
        };
        if (r.ok()) return done(keep(std::move(r.body)));
        const std::string why = problem("Project Gutenberg", r);
        fetch(mirrorHtmlUrl(bookId), [why, keep, done = std::move(done)](platform::HttpResponse m) mutable {
            if (!m.ok()) return done(Page::failure(why + "; its mirror did not answer either"));
            done(keep(std::move(m.body)));
        });
    });
}

void GutenbergSource::book(const std::string& bookId, std::function<void(sources::Result<std::vector<sources::BookChapter>>)> done)
{
    using Book = sources::Result<std::vector<sources::BookChapter>>;
    if (!isBookId(bookId)) return done(Book::failure("Not a Project Gutenberg book number"));
    if (bookId == m_bookId) {
        Book cached;
        cached.value = m_book;
        return done(std::move(cached));
    }
    bookPage(bookId, [this, bookId, done = std::move(done)](sources::Result<std::string> page) {
        if (!page.ok()) return done(Book::failure(page.error));
        Book b;
        b.value = sources::splitBook(weblink::tokenize(page.value));
        if (b.value.empty()) return done(Book::failure("No chapters found in this book"));
        m_bookId = bookId;
        m_book = b.value;
        done(std::move(b));
    });
}

void GutenbergSource::chapters(const std::string& bookId, std::function<void(sources::Result<std::vector<sources::Chapter>>)> done)
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

void GutenbergSource::pages(const sources::Chapter&, std::function<void(sources::Result<sources::PageList>)> done)
{
    done(sources::Result<sources::PageList>::failure("Project Gutenberg books are text"));
}

void GutenbergSource::chapterText(const sources::Chapter& chapter, std::function<void(sources::Result<sources::ChapterText>)> done)
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

} // namespace gutenberg
