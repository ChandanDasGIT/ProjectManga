#pragma once

#include "common/sources/mangatypes.h"

#include <string_view>

// Standard Ebooks' public web pages (standardebooks.org): the ebook lists and an ebook's page.
// A book's id is its path, "/ebooks/jane-austen/pride-and-prejudice".
namespace standardebooks {

// A list page (/ebooks?query=... or ?sort=newest): its books; `offset` is the number of books on
// the pages before. The total is known only as "more than these" while a "Next" link is there.
sources::Result<sources::SearchPage> parseList(std::string_view html, int offset);

// An ebook's page: title and author (from <title>), description, and the cover (og:image).
sources::Result<sources::MangaDetails> parseBook(std::string_view html, const std::string& bookId);

// Only paths like "/ebooks/author/title" are book ids.
bool isBookId(std::string_view id);

} // namespace standardebooks
