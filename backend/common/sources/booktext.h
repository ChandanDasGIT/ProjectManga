#pragma once

#include "common/sources/mangatypes.h"
#include "common/sources/webLink/htmltokenizer.h"

#include <string>
#include <string_view>
#include <vector>

// Readable text out of HTML: a whole book split into chapters, or the story part of one chapter
// page on a novel site.
namespace sources {

struct BookChapter {
    std::string title;
    std::vector<std::string> paragraphs;
};

// A book on one page. Chapters are the <section>s marked as chapters (epub:type "chapter", or an
// id starting "chapter-"); without those, each <h2> (else <h3>) starts one. Project Gutenberg's
// header and licence parts are left out, and so are chapters without text.
std::vector<BookChapter> splitBook(const std::vector<weblink::Token>& tokens);

// The story text on a chapter page: the block holding the most paragraph text (or, without
// paragraphs, the most <br>-separated text), skipping what the page hides with CSS. `html` is the
// raw page, for its <style> rules. Title: the page's first <h1>, else <h2>, else <title>.
ChapterText pageText(std::string_view html, const std::vector<weblink::Token>& tokens);

// "I", "XII", "12": a heading that is only a chapter number.
bool isBareNumber(std::string_view heading);

} // namespace sources
