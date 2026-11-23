#include "testkit.h"
#include "fakes.h"

#include "common/sources/webLink/weblinksource.h"
#include "readerView/readersession.h"

namespace {

sources::Chapter chapter(const std::string& url, double number, std::string external = "")
{
    sources::Chapter c;
    c.id = url;
    c.number = std::to_string(static_cast<int>(number));
    c.numberValue = number;
    c.externalUrl = std::move(external);
    return c;
}

struct Setup {
    fakes::Http http;
    fakes::Scheduler clock;
    sources::SourceRegistry registry;
    readerview::ReaderSession reader{registry};
    std::vector<sources::Chapter> list = {chapter("https://a.example/c1/", 1), chapter("https://a.example/c2/", 2),
                                          chapter("https://a.example/c3/", 3, "https://official.example/3")};
    Setup()
    {
        registry.add(std::make_unique<weblink::WebLinkSource>(http, clock));
        http.set("https://a.example/c1/", "<img src='/i/1a.jpg'><img src='/i/1b.jpg'>");
        http.set("https://a.example/c2/", "<img src='/i/2a.jpg'>");
    }
};

} // namespace

TEST(readersession, opens_a_chapter_and_moves_on)
{
    Setup s;
    s.reader.open("web", s.list, "https://a.example/c1/");
    CHECK(s.reader.loading());
    s.http.answerAll();
    CHECK_EQ(s.reader.pages().urls.size(), std::size_t(2));
    CHECK_EQ(s.reader.pages().referer, std::string("https://a.example/c1/"));
    CHECK(!s.reader.hasPrevious());
    CHECK(s.reader.hasNext());

    s.reader.next();
    s.http.answerAll();
    CHECK_EQ(s.reader.chapter().number, std::string("2"));
    CHECK_EQ(s.reader.pages().urls.size(), std::size_t(1));
    s.reader.previous();
    s.http.answerAll();
    CHECK_EQ(s.reader.chapter().number, std::string("1"));
}

TEST(readersession, external_chapter_loads_nothing)
{
    Setup s;
    s.reader.open("web", s.list, "https://a.example/c3/");
    CHECK(s.reader.external());
    CHECK(!s.reader.loading());
    CHECK(s.http.requests.empty());
    CHECK(!s.reader.hasNext());
}

TEST(readersession, a_quick_skip_drops_the_old_answer)
{
    Setup s;
    s.reader.open("web", s.list, "https://a.example/c1/");
    s.reader.next();
    s.http.answerAll();
    CHECK_EQ(s.reader.chapter().number, std::string("2"));
    CHECK_EQ(s.reader.pages().urls.size(), std::size_t(1));
}

TEST(readersession, missing_chapter_and_failed_page)
{
    Setup s;
    s.reader.open("web", s.list, "https://a.example/none/");
    CHECK_EQ(s.reader.error(), std::string("Chapter not found"));
    std::vector<sources::Chapter> broken = {chapter("https://a.example/gone/", 1)};
    s.reader.open("web", broken, "https://a.example/gone/");
    s.http.answerAll();
    CHECK(s.reader.error().find("404") != std::string::npos);
    s.reader.reload();
    CHECK(s.reader.loading());
}
