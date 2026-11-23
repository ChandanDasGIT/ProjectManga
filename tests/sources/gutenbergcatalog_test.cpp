#include "testkit.h"
#include "fakes.h"

#include "common/sources/gutenberg/gutenbergcatalog.h"
#include "common/sources/gutenberg/gutenbergsource.h"
#include "common/ziparchive.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

// A catalogue sample in the shape of pg_catalog.csv.

namespace {

const char* csv =
    "Text#,Type,Issued,Title,Language,Authors,Subjects,LoCC,Bookshelves\n"
    "2701,Text,2001-07-01,\"Moby Dick; Or, The Whale\",en,\"Melville, Herman, 1819-1891\",\"Whaling -- Fiction; Sea stories\",PS,\n"
    "1342,Text,1998-06-01,Pride and Prejudice,en,\"Austen, Jane, 1775-1817\",\"England -- Fiction\",PR,\n"
    "15,Text,1991-01-01,\"Moby Word Lists\",en,\"Ward, Grady\",,PE,\n"
    "84,Text,1993-10-01,\"Frankenstein; Or, The Modern Prometheus\",en,\"Shelley, Mary Wollstonecraft, 1797-1851\",,PR,\n"
    "5000,Text,2026-11-01,\"A Long Title\nWith a Second Line\",en,\"Doe, John [Editor]; Roe, Richard, 1900-\",,,\n"
    "6000,Sound,2026-11-02,Moby Dick (audio),en,\"Melville, Herman\",,,\n"
    "7000,Text,2026-11-03,Moby Dick,de,\"Melville, Herman\",,,\n";

void put32(std::string& s, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i) s += static_cast<char>((v >> (8 * i)) & 0xff);
}

// A gzip file holding `text` in stored (uncompressed) Deflate blocks.
std::string gzipStored(const std::string& text)
{
    std::string gz = std::string("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff", 10);
    std::size_t at = 0;
    do {
        const std::size_t len = std::min<std::size_t>(65535, text.size() - at);
        const bool last = at + len >= text.size();
        gz += static_cast<char>(last ? 1 : 0);
        gz += static_cast<char>(len & 0xff);
        gz += static_cast<char>(len >> 8);
        gz += static_cast<char>(~len & 0xff);
        gz += static_cast<char>((~len >> 8) & 0xff);
        gz += text.substr(at, len);
        at += len;
    } while (at < text.size());
    put32(gz, archive::crc32(reinterpret_cast<const unsigned char*>(text.data()), text.size()));
    put32(gz, static_cast<std::uint32_t>(text.size()));
    return gz;
}

struct TempFolder {
    std::filesystem::path path;
    TempFolder()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "projectmanga-test-XXXXXX").string();
        path = mkdtemp(pattern.data());
    }
    ~TempFolder() { std::filesystem::remove_all(path); }
};

template <typename Pred>
bool settle(fakes::Http& http, fakes::Scheduler& clock, Pred done)
{
    for (int i = 0; i < 400 && !done(); ++i) {
        http.answerAll();
        clock.advance(1000);
        clock.runPosted();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return done();
}

} // namespace

TEST(gutenbergcatalog, gunzip)
{
    std::string out;
    CHECK_EQ(archive::gunzip(gzipStored(csv), out), std::string());
    CHECK_EQ(out, std::string(csv));
    std::string bad = gzipStored("hello");
    bad[bad.size() - 9] ^= 1;
    CHECK(archive::gunzip(bad, out).find("checksum") != std::string::npos);
    CHECK_EQ(archive::gunzip("not gzip at all, really", out), std::string("not a gzip file"));
}

TEST(gutenbergcatalog, reads_english_text_books)
{
    gutenberg::Catalog c;
    CHECK(c.load(csv));
    CHECK_EQ(c.size(), std::size_t(5));                       // no audio, no German
    const auto* moby = c.find(2701);
    CHECK(moby != nullptr);
    CHECK_EQ(moby->title, std::string("Moby Dick; Or, The Whale"));
    CHECK_EQ(moby->authors, std::string("Herman Melville"));
    CHECK_EQ(c.find(5000)->title, std::string("A Long Title"));
    CHECK_EQ(c.find(5000)->authors, std::string("John Doe, Richard Roe"));
    CHECK(c.find(6000) == nullptr);
    CHECK(!c.load("not,the,catalogue\n1,2,3\n"));
}

TEST(gutenbergcatalog, search_and_recent)
{
    gutenberg::Catalog c;
    c.load(csv);
    auto hits = c.search("moby dick");
    CHECK_EQ(hits.size(), std::size_t(1));
    CHECK_EQ(hits[0]->id, 2701);
    hits = c.search("MOBY");
    CHECK_EQ(hits.size(), std::size_t(2));
    CHECK_EQ(hits[0]->id, 15);                                // the shorter title first
    CHECK_EQ(c.search("melville whale").size(), std::size_t(1));   // author and title words together
    CHECK(c.search("nothing like this").empty());
    const auto recent = c.recent(2);
    CHECK_EQ(recent.size(), std::size_t(2));
    CHECK_EQ(recent[0]->id, 5000);
    const auto d = gutenberg::Catalog::details(*c.find(2701));
    CHECK_EQ(d.description, std::string("Subjects: Whaling -- Fiction; Sea stories"));
    CHECK_EQ(d.coverUrl, std::string("https://gutenberg.pglaf.org/cache/epub/2701/pg2701.cover.medium.jpg"));
}

TEST(gutenbergcatalog, source_searches_the_catalogue_from_the_mirror)
{
    TempFolder dir;
    fakes::Http http;
    fakes::Scheduler clock;
    gutenberg::GutenbergSource source(http, clock, (dir.path / "gutenberg").string());
    // gutenberg.org is down (not set: 404); the mirror has the catalogue.
    http.set("https://gutenberg.pglaf.org/cache/epub/feeds/pg_catalog.csv.gz", gzipStored(csv));
    std::vector<sources::MangaSummary> found;
    source.search("moby dick", 0, {}, [&](auto r) { found = r.value.items; });
    CHECK(settle(http, clock, [&] { return !found.empty(); }));
    CHECK(!found.empty() && found[0].mangaId == "2701");
    CHECK(std::filesystem::exists(dir.path / "gutenberg/pg_catalog.csv.gz"));   // kept for next time

    sources::MangaDetails details;
    const std::size_t asked = http.requests.size();
    source.details("1342", [&](auto r) { details = r.value; });
    CHECK_EQ(details.title, std::string("Pride and Prejudice"));               // at once, from the catalogue
    CHECK_EQ(http.requests.size(), asked);

    // Gutendex down: the list falls back to the newest books.
    sources::SearchPage list;
    source.browse({}, 0, [&](auto r) { list = r.value; });
    CHECK(settle(http, clock, [&] { return !list.items.empty(); }));
    CHECK(!list.items.empty() && list.items[0].mangaId == "5000");
    CHECK(list.listName.rfind("Recently added", 0) == 0);
}

TEST(gutenbergcatalog, a_fresh_copy_on_disk_is_used_without_asking)
{
    TempFolder dir;
    std::filesystem::create_directories(dir.path / "gutenberg");
    { std::ofstream(dir.path / "gutenberg/pg_catalog.csv.gz", std::ios::binary) << gzipStored(csv); }
    fakes::Http http;
    fakes::Scheduler clock;
    gutenberg::GutenbergSource source(http, clock, (dir.path / "gutenberg").string());
    std::vector<sources::MangaSummary> found;
    source.search("frankenstein", 0, {}, [&](auto r) { found = r.value.items; });
    CHECK(settle(http, clock, [&] { return !found.empty(); }));
    CHECK(!found.empty() && found[0].mangaId == "84");
    CHECK(http.requests.empty());
}
