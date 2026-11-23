#include "testkit.h"
#include "fakes.h"

#include "common/library/siteadder.h"
#include "common/sources/webLink/weblinksource.h"
#include "novelReaderView/novelreadersession.h"
#include "novelsView/novelssession.h"

namespace {

// A source that answers browse and search at once with a few books.
class BookSource : public sources::Source {
public:
    explicit BookSource(std::string id) : m_id(std::move(id)) {}
    std::vector<std::string> calls;
    std::string id() const override { return m_id; }
    std::string name() const override { return m_id; }
    std::string kind() const override { return "novel"; }
    void search(const std::string& q, int offset, const std::vector<std::string>&,
                std::function<void(sources::Result<sources::SearchPage>)> done) override
    {
        calls.push_back("search:" + q + "@" + std::to_string(offset));
        done(page(offset));
    }
    void browse(const sources::BrowseQuery&, int offset, std::function<void(sources::Result<sources::SearchPage>)> done) override
    {
        calls.push_back("browse@" + std::to_string(offset));
        done(page(offset));
    }
    void details(const std::string&, std::function<void(sources::Result<sources::MangaDetails>)>) override {}
    void chapters(const std::string&, std::function<void(sources::Result<std::vector<sources::Chapter>>)>) override {}
    void pages(const sources::Chapter&, std::function<void(sources::Result<sources::PageList>)>) override {}

private:
    sources::Result<sources::SearchPage> page(int offset)
    {
        sources::Result<sources::SearchPage> r;
        r.value.items = {{m_id + std::to_string(offset), "Book", "", ""}};
        r.value.total = 2;
        r.value.nextOffset = offset + 1;
        return r;
    }
    std::string m_id;
};

} // namespace

TEST(novelssession, lists_searches_and_switches_source)
{
    sources::SourceRegistry registry;
    auto* a = new BookSource("a");
    auto* b = new BookSource("b");
    registry.add(std::unique_ptr<sources::Source>(a));
    registry.add(std::unique_ptr<sources::Source>(b));
    novelsview::NovelsSession session(registry, {"a", "b"});
    CHECK_EQ(session.sourceId(), std::string("a"));
    session.start();
    session.start();
    CHECK(a->calls == std::vector<std::string>({"browse@0"}));
    CHECK(session.canLoadMore());
    session.loadMore();
    CHECK_EQ(session.results().size(), std::size_t(2));
    CHECK(!session.canLoadMore());

    session.search("  moon ");
    CHECK_EQ(a->calls.back(), std::string("search:moon@0"));
    session.setSource("b");
    CHECK_EQ(b->calls.back(), std::string("search:moon@0"));          // the same search, other source
    session.search("");
    CHECK_EQ(b->calls.back(), std::string("browse@0"));
    session.setSource("nope");
    CHECK_EQ(session.sourceId(), std::string("b"));
}

TEST(novelssession, reader_moves_through_chapters)
{
    fakes::Http http;
    fakes::Scheduler clock;
    sources::SourceRegistry registry;
    registry.add(std::make_unique<weblink::WebNovelSource>(http, clock));
    const std::string longText = "<p>A paragraph long enough to be the story, with plenty of words in it to count as text.</p>"
                                 "<p>Another paragraph long enough to be the story, with plenty of words in it as well.</p>";
    http.set("https://n.example/c1/", "<h1>One</h1><div>" + longText + "</div>");
    http.set("https://n.example/c2/", "<h1>Two</h1><div>" + longText + "</div>");
    std::vector<sources::Chapter> list(2);
    list[0].id = "https://n.example/c1/";
    list[1].id = "https://n.example/c2/";
    novelreaderview::NovelReaderSession reader(registry);
    reader.open("webnovel", list, list[0].id);
    CHECK(reader.loading());
    http.answerAll();
    CHECK_EQ(reader.text().title, std::string("One"));
    CHECK(reader.hasNext());
    reader.next();
    http.answerAll();
    CHECK_EQ(reader.text().title, std::string("Two"));
    CHECK(!reader.hasNext());
    reader.open("webnovel", list, "missing");
    CHECK_EQ(reader.error(), std::string("Chapter not found"));
}

TEST(novelssession, site_adder_checks_and_adds)
{
    fakes::Http http;
    fakes::Scheduler clock;
    sources::SourceRegistry registry;
    registry.add(std::make_unique<weblink::WebNovelSource>(http, clock));
    library::Library lib;
    library::SiteAdder adder(lib, registry, clock);
    CHECK_EQ(adder.check("webnovel", " ", "https://n.example/"), std::string("Give the site a name."));
    CHECK(adder.check("webnovel", "N", "n.example").rfind("Enter the full address", 0) == 0);
    CHECK_EQ(adder.check("nope", "N", "https://n.example/"), std::string("Unknown source \"nope\"."));

    http.set("https://n.example/s/", "<a href='/s/chapter-1/'>Chapter 1</a><a href='/s/chapter-2/'>Chapter 2</a>");
    std::string error = "not called";
    adder.add("webnovel", " My Novel ", "https://n.example/s/", [&](std::string e, auto) { error = e; });
    http.answerAll();
    CHECK_EQ(error, std::string());
    const auto* e = lib.find("webnovel", "https://n.example/s/");
    CHECK(e != nullptr);
    CHECK_EQ(e->title, std::string("My Novel"));
    CHECK_EQ(e->chapterNumbers.size(), std::size_t(2));
    CHECK_EQ(adder.check("webnovel", "Again", "https://n.example/s/"), std::string("This address is already in your library."));
}
