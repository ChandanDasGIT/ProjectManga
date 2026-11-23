#include "testkit.h"
#include "fakes.h"

#include "common/sources/gutenberg/gutenbergsource.h"
#include "common/sources/gutenberg/gutendexparse.h"
#include "common/sources/standardEbooks/standardebooksparse.h"
#include "common/sources/standardEbooks/standardebookssource.h"
#include "common/sources/webLink/weblinksource.h"

// Sample answers shaped like Standard Ebooks' list and ebook pages and Gutendex's JSON.

namespace {

const char* seList = R"(<ol class="ebooks-list grid">
<li typeof="schema:Book" about="/ebooks/jane-austen/northanger-abbey"><div class="thumbnail-container"><a href="/ebooks/jane-austen/northanger-abbey">
<picture><source srcset="/images/covers/a/cover.avif 1x" type="image/avif"/><img src="/images/covers/jane-austen_northanger-abbey/1/cover@2x.jpg" alt="" property="schema:image"/></picture></a></div>
<p><a href="/ebooks/jane-austen/northanger-abbey" property="schema:url"><span property="schema:name">Northanger Abbey</span></a></p>
<p class="author" typeof="schema:Person" property="schema:author"><a href="https://standardebooks.org/ebooks/jane-austen" property="schema:url"><span property="schema:name">Jane Austen</span></a></p></li>
<li typeof="schema:Book" about="/ebooks/h-g-wells/the-time-machine"><p><a href="/ebooks/h-g-wells/the-time-machine"><span property="schema:name">The Time Machine</span></a></p>
<p class="author"><a><span property="schema:name">H. G. Wells</span></a></p></li></ol>
<nav class="pagination"><a aria-disabled="true">Back</a><ol><li><a aria-current="page" href="#">1</a></li><li><a href="/ebooks?page=2">2</a></li></ol><a href="/ebooks?page=2" rel="next">Next</a></nav>)";

const char* seBook = R"(<html><head><title>Pride and Prejudice, by Jane Austen - Free ebook download - Standard Ebooks: Free and liberated ebooks</title></head>
<meta content="https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/downloads/cover.jpg" property="og:image"/>
<body><img src="/images/covers/jane-austen_pride-and-prejudice/9/hero@2x.jpg" alt=""/>
<section id="description"><h2>Description</h2><div property="schema:description"><p><i>Pride and Prejudice</i> may be one of the most enduring novels.</p><p>Second part.</p></div></section></body></html>)";

const char* gutendexList = R"({"count":2,"next":null,"previous":null,"results":[
{"id":1342,"title":"Pride and Prejudice","authors":[{"name":"Austen, Jane","birth_year":1775}],"summaries":["A novel."],
 "formats":{"text/html":"https://www.gutenberg.org/ebooks/1342.html.images","image/jpeg":"https://www.gutenberg.org/cache/epub/1342/pg1342.cover.medium.jpg"},"download_count":100},
{"id":84,"title":"Frankenstein","authors":[{"name":"Shelley, Mary Wollstonecraft"}],"formats":{}}]})";

} // namespace

TEST(novelsources, standard_ebooks_list)
{
    const auto r = standardebooks::parseList(seList, 24);
    CHECK(r.ok());
    CHECK_EQ(r.value.items.size(), std::size_t(2));
    CHECK_EQ(r.value.items[0].mangaId, std::string("/ebooks/jane-austen/northanger-abbey"));
    CHECK_EQ(r.value.items[0].title, std::string("Northanger Abbey"));
    CHECK_EQ(r.value.items[0].subtitle, std::string("Jane Austen"));
    CHECK_EQ(r.value.items[0].coverUrl, std::string("https://standardebooks.org/images/covers/jane-austen_northanger-abbey/1/cover@2x.jpg"));
    CHECK_EQ(r.value.items[1].subtitle, std::string("H. G. Wells"));
    CHECK_EQ(r.value.nextOffset, 26);
    CHECK(r.value.total > r.value.nextOffset);              // a "Next" link: there is more
}

TEST(novelsources, standard_ebooks_book)
{
    const auto r = standardebooks::parseBook(seBook, "/ebooks/jane-austen/pride-and-prejudice");
    CHECK(r.ok());
    CHECK_EQ(r.value.title, std::string("Pride and Prejudice"));
    CHECK_EQ(r.value.subtitle, std::string("Jane Austen"));
    CHECK_EQ(r.value.description, std::string("Pride and Prejudice may be one of the most enduring novels.\n\nSecond part."));
    CHECK_EQ(r.value.coverUrl, std::string("https://standardebooks.org/ebooks/jane-austen/pride-and-prejudice/downloads/cover.jpg"));
    CHECK(standardebooks::isBookId("/ebooks/jane-austen/pride-and-prejudice"));
    CHECK(!standardebooks::isBookId("/ebooks/../etc"));
    CHECK(!standardebooks::isBookId("https://evil.example/ebooks/a/b"));
}

TEST(novelsources, standard_ebooks_source_reads_a_book)
{
    fakes::Http http;
    fakes::Scheduler clock;
    standardebooks::StandardEbooksSource source(http, clock);
    http.set("https://standardebooks.org/ebooks/a/b/text/single-page",
             R"(<section id="chapter-1" epub:type="chapter"><h2>I</h2><p>One.</p></section>
                <section id="chapter-2" epub:type="chapter"><h2>II</h2><p>Two.</p><p>Three.</p></section>)");
    std::vector<sources::Chapter> chapters;
    source.chapters("/ebooks/a/b", [&](auto r) { chapters = r.value; });
    http.answerAll();
    CHECK_EQ(chapters.size(), std::size_t(2));
    CHECK_EQ(sources::chapterLabel(chapters[1]), std::string("Chapter II"));
    CHECK_EQ(chapters[1].numberValue, 2.0);

    sources::ChapterText text;
    source.chapterText(chapters[1], [&](auto r) { text = r.value; });
    CHECK_EQ(text.paragraphs.size(), std::size_t(2));              // from the book kept in memory
    CHECK_EQ(http.requests.size(), std::size_t(1));
    CHECK(!source.updates());

    source.search("jane austen", 24, {}, [](auto) {});
    clock.advance(1000);
    CHECK_EQ(http.requests.back().url, std::string("https://standardebooks.org/ebooks?query=jane%20austen&sort=relevance&per-page=24&page=2"));
}

TEST(novelsources, gutendex_list_and_book)
{
    const auto r = gutenberg::parseList(gutendexList, 0);
    CHECK(r.ok());
    CHECK_EQ(r.value.items.size(), std::size_t(2));
    CHECK_EQ(r.value.items[0].mangaId, std::string("1342"));
    CHECK_EQ(r.value.items[0].subtitle, std::string("Jane Austen"));
    CHECK_EQ(r.value.items[1].subtitle, std::string("Mary Wollstonecraft Shelley"));
    CHECK_EQ(r.value.items[0].coverUrl, std::string("https://gutenberg.pglaf.org/cache/epub/1342/pg1342.cover.medium.jpg"));
    CHECK_EQ(r.value.total, 2);

    const auto b = gutenberg::parseBook(R"({"id":1342,"title":"Pride and Prejudice","authors":[],"summaries":["A novel."],"formats":{}})");
    CHECK(b.ok());
    CHECK_EQ(b.value.description, std::string("A novel."));
    CHECK(!gutenberg::parseBook(R"({"detail":"Not found."})").ok());
    CHECK_EQ(gutenberg::htmlUrl("1342"), std::string("https://www.gutenberg.org/cache/epub/1342/pg1342-images.html"));
    CHECK(!gutenberg::isBookId("13a"));
}

TEST(novelsources, gutenberg_source_requests)
{
    fakes::Http http;
    fakes::Scheduler clock;
    gutenberg::GutenbergSource source(http, clock);
    source.search("moby dick", 32, {}, [](auto) {});
    CHECK_EQ(http.requests.at(0).url, std::string("https://gutendex.com/books/?languages=en&search=moby%20dick&page=2"));
    clock.advance(1000);
    source.browse({}, 0, [](auto) {});
    clock.advance(1000);
    CHECK_EQ(http.requests.at(1).url, std::string("https://gutendex.com/books/?languages=en&page=1"));
    std::string error;
    source.chapters("../x", [&](auto r) { error = r.error; });
    CHECK_EQ(error, std::string("Not a Project Gutenberg book number"));
}

TEST(novelsources, novel_site_reads_story_text)
{
    fakes::Http http;
    fakes::Scheduler clock;
    weblink::WebNovelSource source(http, clock);
    CHECK_EQ(source.kind(), std::string("novel"));
    http.set("https://n.example/s/chapter-1/", "<h1>Chapter 1</h1><div><p>A long enough first paragraph of the story text, written out to pass the size check of the reader.</p>"
                                               "<p>And a second paragraph that follows it with more words in it than the menus.</p></div>");
    sources::Chapter c;
    c.id = "https://n.example/s/chapter-1/";
    sources::ChapterText text;
    source.chapterText(c, [&](auto r) { text = r.value; });
    http.answerAll();
    CHECK_EQ(text.title, std::string("Chapter 1"));
    CHECK_EQ(text.paragraphs.size(), std::size_t(2));
}

TEST(novelsources, gutenberg_falls_back_to_its_mirror)
{
    fakes::Http http;
    fakes::Scheduler clock;
    gutenberg::GutenbergSource source(http, clock);
    // gutenberg.org and Gutendex are down (not set: 404); the mirror answers.
    http.set("https://www.gutenberg.org/cache/epub/2701/pg2701-images.html", "Temporarily Down for Maintenance", 503);
    http.set("https://gutenberg.pglaf.org/cache/epub/2701/pg2701-images.html",
             "<title>Moby Dick; or The Whale | Project Gutenberg</title><p>Author: Herman Melville</p>"
             "<h2>CHAPTER 1. Loomings.</h2><p>Call me Ishmael.</p><h2>CHAPTER 2. The Carpet-Bag.</h2><p>I stuffed a shirt.</p>");
    sources::MangaDetails details;
    std::string error = "not called";
    source.details("2701", [&](auto r) { details = r.value; error = r.error; });
    for (int i = 0; i < 5; ++i) { http.answerAll(); clock.advance(1000); }
    CHECK_EQ(error, std::string());
    CHECK_EQ(details.title, std::string("Moby Dick; or The Whale"));
    CHECK_EQ(details.subtitle, std::string("Herman Melville"));
    CHECK_EQ(details.coverUrl, std::string("https://gutenberg.pglaf.org/cache/epub/2701/pg2701.cover.medium.jpg"));

    std::vector<sources::Chapter> chapters;
    const std::size_t asked = http.requests.size();
    source.chapters("2701", [&](auto r) { chapters = r.value; });
    for (int i = 0; i < 3; ++i) { http.answerAll(); clock.advance(1000); }
    CHECK_EQ(chapters.size(), std::size_t(2));
    CHECK_EQ(http.requests.size(), asked);                      // the page kept from the details

    http.set("https://gutenberg.pglaf.org/cache/epub/11/pg11-images.html", "down", 503);
    source.chapters("11", [&](auto r) { error = r.error; });
    for (int i = 0; i < 3; ++i) { http.answerAll(); clock.advance(1000); }
    CHECK(error.find("its mirror did not answer either") != std::string::npos);
}
