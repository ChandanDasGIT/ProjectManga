#include "testkit.h"

#include "common/sources/booktext.h"

// Sample pages shaped like a Standard Ebooks single-page book, a Project Gutenberg HTML book and
// a novel site's chapter page.

namespace {

const char* sectionBook = R"(<html><body><header><nav><a href="/">Standard Ebooks</a></nav></header><main>
<section id="titlepage" epub:type="frontmatter titlepage"><h1>Pride and Prejudice</h1><p>By Jane Austen.</p></section>
<section id="chapter-1" epub:type="chapter"><h2 epub:type="num">I</h2>
<p>It is a truth universally acknowledged, that a single man in possession of a good fortune, must be in want of a wife.</p>
<p>“My dear Mr. Bennet,” said his lady to him one day, “have you heard?”</p></section>
<section id="chapter-2" epub:type="chapter"><hgroup><h2>II</h2><p>The Visit</p></hgroup><p>Mr. Bennet was among the earliest.</p>
<blockquote><p>A quoted letter.</p></blockquote></section>
<section id="colophon" epub:type="backmatter colophon"><p>Produced by volunteers.</p></section>
</main></body></html>)";

const char* headingBook = R"(<html><body>
<section id="pg-header"><h2>The Project Gutenberg eBook</h2><p>This ebook is for the use of anyone.</p></section>
<h1>Moby Dick</h1>
<div class="chapter"><h2><a id="link1"></a>CHAPTER 1. Loomings.</h2>
<p>Call me Ishmael. Some years ago—never mind how long precisely—</p>
<p>having little or no money in my purse,<br/>I thought I would sail about.</p></div>
<div class="chapter"><h2>CHAPTER 2. The Carpet-Bag.</h2><p>I stuffed a shirt or two.</p></div>
<section id="pg-footer"><h2>THE FULL PROJECT GUTENBERG LICENSE</h2><p>Licence text.</p></section>
</body></html>)";

} // namespace

TEST(booktext, chapter_sections)
{
    const auto chapters = sources::splitBook(weblink::tokenize(sectionBook));
    CHECK_EQ(chapters.size(), std::size_t(2));
    CHECK_EQ(chapters[0].title, std::string("Chapter I"));
    CHECK_EQ(chapters[0].paragraphs.size(), std::size_t(2));
    CHECK(chapters[0].paragraphs[1].rfind("“My dear Mr. Bennet,”", 0) == 0);
    CHECK_EQ(chapters[1].title, std::string("II The Visit"));
    CHECK_EQ(chapters[1].paragraphs.size(), std::size_t(2));
    CHECK_EQ(chapters[1].paragraphs[1], std::string("A quoted letter."));
}

TEST(booktext, heading_chapters_skip_gutenberg_parts)
{
    const auto chapters = sources::splitBook(weblink::tokenize(headingBook));
    CHECK_EQ(chapters.size(), std::size_t(2));
    CHECK_EQ(chapters[0].title, std::string("CHAPTER 1. Loomings."));
    CHECK_EQ(chapters[0].paragraphs.size(), std::size_t(2));
    CHECK_EQ(chapters[0].paragraphs[1], std::string("having little or no money in my purse,\nI thought I would sail about."));
    CHECK_EQ(chapters[1].paragraphs[0], std::string("I stuffed a shirt or two."));
}

TEST(booktext, no_structure_is_one_part)
{
    const auto chapters = sources::splitBook(weblink::tokenize("<p>One.</p><p>Two.</p>"));
    CHECK_EQ(chapters.size(), std::size_t(1));
    CHECK_EQ(chapters[0].title, std::string("Part 1"));
}

TEST(booktext, story_text_on_a_chapter_page)
{
    const std::string page = R"(<html><head><title>Chapter 5 | Some Site</title>
<style>.cjKq2{display: none; speak: never;} .x{color:red}</style></head><body>
<nav><p>Home</p><p>Browse</p></nav>
<div class="sidebar"><p>Popular this week: lots of other stories you could read instead.</p></div>
<h1>Chapter 5: The Fall</h1>
<div class="chapter-content">
<p>The wind howled across the broken plain as Mira climbed the last ridge, her breath short and her hands raw from the stone.</p>
<p class="cjKq2">This story was taken from somewhere else; report it.</p>
<p>Below her, the city burned. She had come too late, and she knew it before she saw the smoke rising from the gates.</p>
<p style="display:none">hidden too</p>
</div><footer><p>© Site</p></footer></body></html>)";
    const auto text = sources::pageText(page, weblink::tokenize(page));
    CHECK_EQ(text.title, std::string("Chapter 5: The Fall"));
    CHECK_EQ(text.paragraphs.size(), std::size_t(2));
    CHECK(text.paragraphs[0].rfind("The wind howled", 0) == 0);
    CHECK(text.paragraphs[1].rfind("Below her", 0) == 0);
}

TEST(booktext, story_text_without_paragraphs)
{
    const std::string page = "<title>T</title><div id='menu'>Home</div><div id='text'>First line of the story goes here and runs on."
                             "<br><br>Second line of the story, also long enough to count.<br>Third.</div>";
    const auto text = sources::pageText(page, weblink::tokenize(page));
    CHECK_EQ(text.paragraphs.size(), std::size_t(3));
    CHECK_EQ(text.paragraphs[2], std::string("Third."));
}

TEST(booktext, bare_numbers)
{
    CHECK(sources::isBareNumber("XIV"));
    CHECK(sources::isBareNumber(" 12. "));
    CHECK(!sources::isBareNumber("Chapter 1"));
    CHECK(!sources::isBareNumber(""));
}

TEST(booktext, gutenberg_page_numbers_are_left_out)
{
    const auto chapters = sources::splitBook(weblink::tokenize(
        "<h2><span class='pagenum'><a id='page_ix'>{ix}</a></span>PREFACE.</h2><p>Walt Whitman has<span class='pagenum'>{x}</span> somewhere.</p>"
        "<h2>CHAPTER I.</h2><p>It is a truth.</p>"));
    CHECK_EQ(chapters.size(), std::size_t(2));
    CHECK_EQ(chapters[0].title, std::string("PREFACE."));
    CHECK_EQ(chapters[0].paragraphs[0], std::string("Walt Whitman has somewhere."));
}
