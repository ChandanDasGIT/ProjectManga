#pragma once

#include "common/sources/mangatypes.h"

#include <string>
#include <string_view>

// Gutendex (gutendex.com), a JSON catalogue of Project Gutenberg's books. A book's id is its
// Gutenberg number, "1342".
namespace gutenberg {

// A /books/ list: up to 32 books, the total count, and the offset after them.
sources::Result<sources::SearchPage> parseList(std::string_view json, int offset);
// One /books/<id>/ answer.
sources::Result<sources::MangaDetails> parseBook(std::string_view json);

bool isBookId(std::string_view id);
// Where Project Gutenberg keeps a book's HTML edition, on gutenberg.org or on its official mirror
// (gutenberg.pglaf.org, used when gutenberg.org does not answer).
std::string htmlUrl(std::string_view bookId);
std::string mirrorHtmlUrl(std::string_view bookId);
// A gutenberg.org file link moved to the mirror (covers load from there); other links unchanged.
std::string onMirror(std::string_view url);
// A book's details from its HTML edition, for when Gutendex does not answer: the title and author
// from "<title>Moby Dick; or The Whale | Project Gutenberg</title>" and the "Author:" line.
sources::MangaDetails detailsFromBook(std::string_view html, std::string_view bookId);

} // namespace gutenberg
