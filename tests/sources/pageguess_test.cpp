#include "testkit.h"
#include "fakes.h"

#include "common/sources/webLink/pageguess.h"
#include "common/sources/webLink/weblinksource.h"

// Sample pages written for these tests, shaped like common manga site layouts.

namespace {

const char* seriesPage = R"(<html><head>
<title>Moon Knights Manga Online</title>
<meta property="og:title" content="Moon Knights">
<meta property="og:image" content="/wp-content/uploads/cover.jpg">
<meta name="description" content="A story about knights.">
</head><body>
<nav><a href="/">Home</a> <a href="https://other.example/chapter-99/">Ad</a></nav>
<ul class="chapters">
  <li><a href="https://www.moon.example/manga/moon-knights-chapter-3/">Moon Knights, Chapter 3</a></li>
  <li><a href="/manga/moon-knights-chapter-2/">Moon Knights, Chapter 2</a></li>
  <li><a href="/manga/moon-knights-chapter-1-5/">Extra</a></li>
  <li><a href="manga/moon-knights-chapter-1/"><span>Ch.</span> <b>1</b></a></li>
</ul>
<a href="/manga/moon-knights-chapter-3/">Read latest chapter</a>
<a href="/about/">About</a>
</body></html>)";

const char* chapterPage = R"(<html><body>
<img src="/wp-content/themes/x/logo.png">
<img src="https://cdn.example/avatar/u1.jpg" width="48" height="48">
<div class="reader">
  <img src="data:image/gif;base64,R0lG" data-src="https://img.example/images/Moon Knights/p01.jpg" alt="page 1">
  <img data-lazy-src="https://img.example/images/Moon Knights/p02.jpg">
  <img src="https://img.example/images/Moon Knights/p03.webp">
  <img src="https://img.example/images/Moon Knights/p02.jpg">
</div>
<aside><img src="https://moon.example/wp-content/uploads/related-1.jpg"></aside>
<aside><img src="https://moon.example/wp-content/uploads/other-150x150.jpg"></aside>
</body></html>)";

} // namespace

TEST(pageguess, web_addresses)
{
    CHECK(weblink::isWebAddress("https://moon.example/manga/"));
    CHECK(weblink::isWebAddress("http://moon.example"));
    CHECK(!weblink::isWebAddress("moon.example"));
    CHECK(!weblink::isWebAddress("ftp://moon.example"));
    CHECK(!weblink::isWebAddress("https://"));
    CHECK(!weblink::isWebAddress("https://moon example.com"));
    CHECK(!weblink::isWebAddress("https://localhost/"));
}

TEST(pageguess, resolving_links)
{
    const std::string base = "https://moon.example/manga/series/index.html?x=1";
    CHECK_EQ(weblink::resolveUrl(base, "https://a.example/b"), std::string("https://a.example/b"));
    CHECK_EQ(weblink::resolveUrl(base, "//cdn.example/i.jpg"), std::string("https://cdn.example/i.jpg"));
    CHECK_EQ(weblink::resolveUrl(base, "/top/"), std::string("https://moon.example/top/"));
    CHECK_EQ(weblink::resolveUrl(base, "ch-2/"), std::string("https://moon.example/manga/series/ch-2/"));
    CHECK_EQ(weblink::resolveUrl(base, "../other/./x.jpg"), std::string("https://moon.example/manga/other/x.jpg"));
    CHECK_EQ(weblink::resolveUrl(base, "?page=2"), std::string("https://moon.example/manga/series/index.html?page=2"));
    CHECK_EQ(weblink::resolveUrl(base, "x.html#top"), std::string("https://moon.example/manga/series/x.html"));
    CHECK_EQ(weblink::resolveUrl(base, "#top"), std::string());
    CHECK_EQ(weblink::resolveUrl(base, "javascript:void(0)"), std::string());
    CHECK_EQ(weblink::resolveUrl("https://moon.example", "a"), std::string("https://moon.example/a"));
    CHECK_EQ(weblink::hostOf("https://www.Moon.Example:8080/a"), std::string("moon.example"));
}

TEST(pageguess, chapter_numbers)
{
    CHECK_EQ(weblink::chapterNumber("Chapter 12", ""), std::string("12"));
    CHECK_EQ(weblink::chapterNumber("Ch. 7.5 - The end", ""), std::string("7.5"));
    CHECK_EQ(weblink::chapterNumber("Episode 003", ""), std::string("3"));
    CHECK_EQ(weblink::chapterNumber("Read now", "https://s.example/manga/x-chapter-261/"), std::string("261"));
    CHECK_EQ(weblink::chapterNumber("Read now", "https://s.example/x/chapter-12-5/"), std::string("12.5"));
    CHECK_EQ(weblink::chapterNumber("Read now", "https://s.example/x/ch_40.html"), std::string("40"));
    CHECK_EQ(weblink::chapterNumber("Read now", "https://s.example/x/chapter-12-the-return/"), std::string());
    CHECK_EQ(weblink::chapterNumber("Watch 12", "https://s.example/watch-12/"), std::string());
    CHECK_EQ(weblink::chapterNumber("About", "https://s.example/about/"), std::string());
}

TEST(pageguess, page_info)
{
    const auto info = weblink::guessInfo(weblink::tokenize(seriesPage), "https://moon.example/manga/");
    CHECK_EQ(info.title, std::string("Moon Knights"));
    CHECK_EQ(info.coverUrl, std::string("https://moon.example/wp-content/uploads/cover.jpg"));
    CHECK_EQ(info.description, std::string("A story about knights."));
    const auto bare = weblink::guessInfo(weblink::tokenize("<title> Only\n a  title </title>"), "https://a.example/");
    CHECK_EQ(bare.title, std::string("Only a title"));
}

TEST(pageguess, chapters_on_a_series_page)
{
    const auto chapters = weblink::guessChapters(weblink::tokenize(seriesPage), "https://moon.example/manga/");
    CHECK_EQ(chapters.size(), std::size_t(4));
    CHECK_EQ(chapters[0].number, std::string("1"));
    CHECK_EQ(chapters[0].id, std::string("https://moon.example/manga/manga/moon-knights-chapter-1/"));
    CHECK_EQ(chapters[1].number, std::string("1.5"));
    CHECK_EQ(chapters[2].id, std::string("https://moon.example/manga/moon-knights-chapter-2/"));
    CHECK_EQ(chapters[3].number, std::string("3"));
    CHECK_EQ(chapters[3].id, std::string("https://www.moon.example/manga/moon-knights-chapter-3/"));
}

TEST(pageguess, images_on_a_chapter_page)
{
    const auto pages = weblink::guessImages(weblink::tokenize(chapterPage), "https://moon.example/manga/moon-knights-chapter-3/");
    CHECK_EQ(pages.size(), std::size_t(3));
    CHECK_EQ(pages[0], std::string("https://img.example/images/Moon Knights/p01.jpg"));
    CHECK_EQ(pages[1], std::string("https://img.example/images/Moon Knights/p02.jpg"));
    CHECK_EQ(pages[2], std::string("https://img.example/images/Moon Knights/p03.webp"));
}

TEST(pageguess, a_single_image_is_kept)
{
    const auto pages = weblink::guessImages(weblink::tokenize(R"(<img src="/strip/long.png">)"), "https://s.example/c/1/");
    CHECK_EQ(pages.size(), std::size_t(1));
}

TEST(pageguess, web_link_source_reads_series_and_chapter)
{
    fakes::Http http;
    fakes::Scheduler clock;
    weblink::WebLinkSource source(http, clock);
    const std::string series = "https://moon.example/manga/";
    http.set(series, seriesPage);
    http.set("https://www.moon.example/manga/moon-knights-chapter-3/", chapterPage);

    std::string title;
    std::vector<sources::Chapter> chapters;
    source.details(series, [&](auto r) { title = r.value.title; });
    http.answerAll();
    source.chapters(series, [&](auto r) { chapters = r.value; });
    http.answerAll();
    CHECK_EQ(title, std::string("Moon Knights"));
    CHECK_EQ(chapters.size(), std::size_t(4));
    CHECK_EQ(http.requests.size(), std::size_t(1));              // the second read came from the kept page

    sources::PageList pages;
    source.pages(chapters.back(), [&](auto r) { pages = r.value; });
    http.answerAll();
    CHECK_EQ(pages.urls.size(), std::size_t(3));
    CHECK_EQ(pages.referer, chapters.back().id);
}

TEST(pageguess, web_link_source_explains_failures)
{
    fakes::Http http;
    fakes::Scheduler clock;
    weblink::WebLinkSource source(http, clock);
    http.set("https://plain.example/", "<p>No links here</p>");
    std::string error;
    source.chapters("https://plain.example/", [&](auto r) { error = r.error; });
    http.answerAll();
    CHECK_EQ(error, std::string("No chapter links found on this page"));
    source.chapters("https://gone.example/", [&](auto r) { error = r.error; });
    http.answerAll();
    CHECK_EQ(error, std::string("Could not load the page (the site answered 404)"));
    source.chapters("not a link", [&](auto r) { error = r.error; });
    CHECK(error.rfind("Not a web address", 0) == 0);
}

TEST(pageguess, cover_fallbacks_in_order)
{
    const std::string base = "https://moon.example/manga/";
    auto cover = [&](const std::string& html) { return weblink::guessInfo(weblink::tokenize(html), base).coverUrl; };
    CHECK_EQ(cover(R"(<meta name="twitter:image" content="/t.jpg"><meta property="og:image" content="/og.jpg">)"),
             std::string("https://moon.example/og.jpg"));
    CHECK_EQ(cover(R"(<link rel="image_src" href="/l.jpg"><meta name="twitter:image" content="/t.jpg">)"),
             std::string("https://moon.example/t.jpg"));
    CHECK_EQ(cover(R"(<img src="/logo.png" class="cover"><img class="summary_image" src="/x.jpg"><link rel="image_src" href="/l.jpg">)"),
             std::string("https://moon.example/l.jpg"));
    CHECK_EQ(cover(R"(<img src="/a.jpg"><img alt="Moon Knights Cover" data-src="/covers/moon.webp" src="data:,">)"),
             std::string("https://moon.example/covers/moon.webp"));
    CHECK_EQ(cover(R"(<img src="/thumbs/tiny.jpg" width="40" height="40"><img src="/a.jpg">)"), std::string());
}

TEST(pageguess, web_link_source_takes_first_page_as_cover)
{
    fakes::Http http;
    fakes::Scheduler clock;
    weblink::WebLinkSource source(http, clock);
    http.set("https://bare.example/s/", "<title>Bare</title><a href='/s/chapter-2/'>Chapter 2</a><a href='/s/chapter-1/'>Chapter 1</a>");
    http.set("https://bare.example/s/chapter-1/", "<img src='/img/logo.png'><img src='/img/s/001.jpg'><img src='/img/s/002.jpg'>");
    std::string cover = "not called";
    source.details("https://bare.example/s/", [&](auto r) { cover = r.value.coverUrl; });
    http.answerAll();
    CHECK_EQ(cover, std::string("https://bare.example/img/s/001.jpg"));

    http.set("https://bare.example/none/", "<title>Nothing</title>");
    clock.advance(1000);
    source.details("https://bare.example/none/", [&](auto r) { cover = r.value.coverUrl; });
    http.answerAll();
    CHECK_EQ(cover, std::string());
}
