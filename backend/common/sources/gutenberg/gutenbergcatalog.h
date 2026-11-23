#pragma once

#include "common/sources/mangatypes.h"

#include <string>
#include <string_view>
#include <vector>

// Project Gutenberg's own catalogue (pg_catalog.csv, published weekly on gutenberg.org and its
// mirrors): every book with its title, authors, language and subjects. The app searches it itself,
// so search and book details need neither Gutendex nor gutenberg.org's search.
namespace gutenberg {

struct CatalogBook {
    int id = 0;
    std::string title;          // the first line of the title
    std::string authors;        // "Herman Melville, Someone Else"
    std::string issued;         // "2001-07-01"
    std::string subjects;       // "Whaling -- Fiction; Sea stories"
};

class Catalog {
public:
    // Reads the CSV text; keeps English text books only. False when it is not the catalogue.
    bool load(std::string_view csv);
    std::size_t size() const { return m_books.size(); }

    // Books whose title or authors hold every word of `query` (ignoring case), the best matches
    // first: more of the words in the title, then shorter titles, then older (lower) numbers.
    std::vector<const CatalogBook*> search(std::string_view query) const;
    // The most recently added books, newest first.
    std::vector<const CatalogBook*> recent(std::size_t count) const;
    const CatalogBook* find(int id) const;

    static sources::MangaSummary summary(const CatalogBook& book);
    static sources::MangaDetails details(const CatalogBook& book);

private:
    std::vector<CatalogBook> m_books;    // by number
};

// "Melville, Herman, 1819-1891; Smith, John [Editor]" -> "Herman Melville, John Smith"
std::string readableAuthors(std::string_view field);
// The cover Gutenberg's mirror keeps for a book.
std::string coverOnMirror(int id);

} // namespace gutenberg
