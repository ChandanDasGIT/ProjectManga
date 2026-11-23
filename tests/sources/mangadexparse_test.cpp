#include "testkit.h"
#include "fakes.h"

#include "common/sources/mangaDex/mangadexparse.h"
#include "common/sources/mangaDex/mangadexsource.h"

// Small answers written after the MangaDex API documentation (api.mangadex.org/docs).

namespace {

const char* searchAnswer = R"({
  "result": "ok", "response": "collection", "limit": 2, "offset": 0, "total": 5,
  "data": [
    { "id": "a1b2c3d4-0000-0000-0000-000000000001", "type": "manga",
      "attributes": { "title": {"ja-ro": "Mokushiroku no Yonkishi"},
                      "altTitles": [{"ja": "黙示録の四騎士"}, {"en": "Four Knights of the Apocalypse"}],
                      "status": "ongoing", "year": 2022 },
      "relationships": [ {"id": "x", "type": "author"},
                         {"id": "c", "type": "cover_art", "attributes": {"fileName": "cover.jpg"}} ] },
    { "id": "a1b2c3d4-0000-0000-0000-000000000002", "type": "manga",
      "attributes": { "title": {"en": "Second"}, "status": "completed", "year": null },
      "relationships": [] }
  ]
})";

const char* mangaAnswer = R"({
  "result": "ok", "response": "entity",
  "data": { "id": "a1b2c3d4-0000-0000-0000-000000000001", "type": "manga",
    "attributes": { "title": {"en": "Four Knights"}, "status": "ongoing", "year": 2022,
                    "description": {"ja": "日本語", "en": "Percival leaves home.\n\n---\n**Links:** somewhere"} },
    "relationships": [ {"id": "p", "type": "author", "attributes": {"name": "Nakaba Suzuki"}},
                       {"id": "c", "type": "cover_art", "attributes": {"fileName": "f.png"}} ] }
})";

const char* feedAnswer = R"({
  "result": "ok", "response": "collection", "limit": 500, "offset": 0, "total": 3,
  "data": [
    { "id": "c0000000-0000-0000-0000-000000000001", "type": "chapter",
      "attributes": { "chapter": "1", "title": "Start", "publishAt": "2022-01-12T15:00:00+00:00",
                      "externalUrl": null, "pages": 50 },
      "relationships": [ {"id": "g", "type": "scanlation_group", "attributes": {"name": "Group A"}} ] },
    { "id": "c0000000-0000-0000-0000-000000000002", "type": "chapter",
      "attributes": { "chapter": "1.5", "title": null, "publishAt": "2022-01-19T15:00:00Z",
                      "externalUrl": "https://mangaplus.example/viewer/1", "pages": 0 },
      "relationships": [] },
    { "id": "c0000000-0000-0000-0000-000000000003", "type": "chapter",
      "attributes": { "chapter": null, "title": "Special", "publishAt": "bad date" },
      "relationships": [] }
  ]
})";

const char* atHomeAnswer = R"({
  "result": "ok", "baseUrl": "https://node.example.org",
  "chapter": { "hash": "abc123", "data": ["1-x.png", "2-y.png"], "dataSaver": ["1-x.jpg"] }
})";

const char* errorAnswer = R"({"result":"error","errors":[{"id":"e","status":404,"title":"not_found_http_exception","detail":"Manga could not be found"}]})";

} // namespace

TEST(mangadexparse, search_results)
{
    const auto r = mangadex::parseSearch(searchAnswer);
    CHECK(r.ok());
    CHECK_EQ(r.value.items.size(), std::size_t(2));
    CHECK_EQ(r.value.items[0].title, std::string("Four Knights of the Apocalypse"));   // English alt title wins
    CHECK_EQ(r.value.items[0].coverUrl,
             std::string("https://uploads.mangadex.org/covers/a1b2c3d4-0000-0000-0000-000000000001/cover.jpg.256.jpg"));
    CHECK_EQ(r.value.items[0].subtitle, std::string("Ongoing · 2022"));
    CHECK_EQ(r.value.items[1].coverUrl, std::string());
    CHECK_EQ(r.value.items[1].subtitle, std::string("Completed"));
    CHECK_EQ(r.value.total, 5);
    CHECK_EQ(r.value.nextOffset, 2);
}

TEST(mangadexparse, manga_details)
{
    const auto r = mangadex::parseManga(mangaAnswer);
    CHECK(r.ok());
    CHECK_EQ(r.value.title, std::string("Four Knights"));
    CHECK_EQ(r.value.description, std::string("Percival leaves home."));            // the links part is cut
    CHECK_EQ(r.value.subtitle, std::string("Nakaba Suzuki · Ongoing · 2022"));
    CHECK(r.value.coverUrl.find("/f.png.256.jpg") != std::string::npos);
}

TEST(mangadexparse, chapter_feed)
{
    const auto r = mangadex::parseFeed(feedAnswer);
    CHECK(r.ok());
    CHECK_EQ(r.value.total, 3);
    CHECK_EQ(r.value.chapters.size(), std::size_t(3));
    const auto& first = r.value.chapters[0];
    CHECK_EQ(first.number, std::string("1"));
    CHECK_EQ(first.numberValue, 1.0);
    CHECK_EQ(first.title, std::string("Start"));
    CHECK_EQ(first.group, std::string("Group A"));
    CHECK_EQ(first.publishedMs, std::int64_t(1641999600000));
    CHECK_EQ(r.value.chapters[1].numberValue, 1.5);
    CHECK_EQ(r.value.chapters[1].externalUrl, std::string("https://mangaplus.example/viewer/1"));
    CHECK_EQ(r.value.chapters[2].number, std::string());
    CHECK_EQ(r.value.chapters[2].numberValue, -1.0);
    CHECK_EQ(r.value.chapters[2].publishedMs, std::int64_t(0));
}

TEST(mangadexparse, image_server_pages)
{
    const auto r = mangadex::parseAtHome(atHomeAnswer);
    CHECK(r.ok());
    CHECK_EQ(r.value.urls.size(), std::size_t(2));
    CHECK_EQ(r.value.urls[1], std::string("https://node.example.org/data/abc123/2-y.png"));
    CHECK(!mangadex::parseAtHome(R"({"result":"ok","baseUrl":"https://n","chapter":{"hash":"h","data":[]}})").ok());
}

TEST(mangadexparse, errors_and_garbage)
{
    const auto r = mangadex::parseManga(errorAnswer);
    CHECK(!r.ok());
    CHECK_EQ(r.error, std::string("MangaDex: Manga could not be found"));
    CHECK(!mangadex::parseSearch("<html>busy</html>").ok());
    CHECK_EQ(mangadex::errorMessage("{}"), std::string());
}

TEST(mangadexparse, iso_times)
{
    CHECK_EQ(mangadex::parseIsoTime("1970-01-01T00:00:00+00:00"), std::int64_t(0));
    CHECK_EQ(mangadex::parseIsoTime("2022-01-12T15:00:00+00:00"), std::int64_t(1641999600000));
    CHECK_EQ(mangadex::parseIsoTime("2022-01-12T17:00:00+02:00"), std::int64_t(1641999600000));
    CHECK_EQ(mangadex::parseIsoTime("2022-01-12T15:00:00.123Z"), std::int64_t(1641999600000));
    CHECK_EQ(mangadex::parseIsoTime("2024-02-29T00:00:00Z"), std::int64_t(1709164800000));
    CHECK_EQ(mangadex::parseIsoTime("yesterday"), std::int64_t(0));
    CHECK_EQ(mangadex::parseIsoTime("2022-13-01T00:00:00Z"), std::int64_t(0));
}

TEST(mangadexparse, source_builds_search_request)
{
    fakes::Http http;
    fakes::Scheduler clock;
    mangadex::MangaDexSource source(http, clock);
    std::string error = "not called";
    source.search("Four Knights & co", 24, {"safe", "erotica"}, [&](auto r) { error = r.error; });
    CHECK_EQ(http.requests.size(), std::size_t(1));
    const std::string& url = http.requests[0].url;
    CHECK(url.rfind("https://api.mangadex.org/manga?title=Four%20Knights%20%26%20co&limit=24&offset=24", 0) == 0);
    CHECK(url.find("includes%5B%5D=cover_art") != std::string::npos);
    CHECK(url.find("&contentRating%5B%5D=safe&contentRating%5B%5D=erotica") != std::string::npos);
    CHECK(url.find("suggestive") == std::string::npos);
    http.answerAll();
    CHECK(error.find("404") != std::string::npos);
}

TEST(mangadexparse, source_pages_through_the_whole_feed)
{
    fakes::Http http;
    fakes::Scheduler clock;
    mangadex::MangaDexSource source(http, clock);
    const std::string id = "a1b2c3d4-0000-0000-0000-000000000001";
    auto feedPage = [&](int offset, int count, int total) {
        std::string data;
        for (int i = 0; i < count; ++i) {
            if (i) data += ",";
            data += R"({"id":"c-)" + std::to_string(offset + i) + R"(","attributes":{"chapter":")" +
                    std::to_string(offset + i + 1) + R"("}})";
        }
        return R"({"result":"ok","limit":500,"offset":)" + std::to_string(offset) + R"(,"total":)" +
               std::to_string(total) + R"(,"data":[)" + data + "]}";
    };
    const std::string base = "https://api.mangadex.org/manga/" + id + "/feed?translatedLanguage%5B%5D=en&limit=500&offset=";
    const std::string rest = "&order%5Bchapter%5D=asc&includes%5B%5D=scanlation_group&contentRating%5B%5D=safe"
                             "&contentRating%5B%5D=suggestive&contentRating%5B%5D=erotica&contentRating%5B%5D=pornographic";
    http.set(base + "0" + rest, feedPage(0, 500, 620));
    http.set(base + "500" + rest, feedPage(500, 120, 620));

    std::size_t got = 0;
    source.chapters(id, [&](auto r) { got = r.ok() ? r.value.size() : 0; });
    http.answerAll();
    CHECK_EQ(got, std::size_t(620));
    CHECK_EQ(http.requests.size(), std::size_t(2));
}

TEST(mangadexparse, source_refuses_odd_ids_and_external_chapters)
{
    fakes::Http http;
    fakes::Scheduler clock;
    mangadex::MangaDexSource source(http, clock);
    std::string error;
    source.details("../../etc", [&](auto r) { error = r.error; });
    CHECK_EQ(error, std::string("Not a MangaDex id"));
    sources::Chapter external;
    external.id = "c0000000-0000-0000-0000-000000000002";
    external.externalUrl = "https://mangaplus.example/viewer/1";
    source.pages(external, [&](auto r) { error = r.error; });
    CHECK_EQ(error, std::string("This chapter is published on an official site"));
    CHECK(http.requests.empty());
}

TEST(mangadexparse, source_keeps_to_five_requests_a_second)
{
    fakes::Http http;
    fakes::Scheduler clock;
    mangadex::MangaDexSource source(http, clock);
    for (int i = 0; i < 8; ++i) source.search("x", 0, {}, [](auto) {});
    CHECK_EQ(http.requests.size(), std::size_t(5));
    clock.advance(1000);
    CHECK_EQ(http.requests.size(), std::size_t(8));
}

TEST(mangadexparse, tags_and_manga_tags)
{
    const auto r = mangadex::parseTags(R"({"result":"ok","data":[
        {"id":"t-3","attributes":{"name":{"en":"Isekai"},"group":"theme"}},
        {"id":"t-1","attributes":{"name":{"en":"Romance"},"group":"genre"}},
        {"id":"t-2","attributes":{"name":{"en":"Action"},"group":"genre"}},
        {"id":"","attributes":{"name":{"en":"Broken"},"group":"genre"}}]})");
    CHECK(r.ok());
    CHECK_EQ(r.value.size(), std::size_t(3));
    CHECK_EQ(r.value[0].name, std::string("Action"));
    CHECK_EQ(r.value[2].group, std::string("theme"));

    const auto m = mangadex::parseManga(R"({"result":"ok","data":{"id":"m1","attributes":{"title":{"en":"T"},
        "tags":[{"id":"t-2","attributes":{"name":{"en":"Action"},"group":"genre"}}]}}})");
    CHECK_EQ(m.value.tags.size(), std::size_t(1));
    CHECK_EQ(m.value.tags[0].id, std::string("t-2"));
}

TEST(mangadexparse, source_builds_browse_request)
{
    fakes::Http http;
    fakes::Scheduler clock;
    mangadex::MangaDexSource source(http, clock);
    sources::BrowseQuery q;
    q.order = "rating";
    q.includedTags = {"aaaa-1", "not an id!"};
    q.anyTag = true;
    q.demographics = {"seinen", "kids"};
    q.contentRatings = {"safe", "suggestive"};
    source.browse(q, 48, [](auto) {});
    const std::string& url = http.requests.at(0).url;
    CHECK(url.rfind("https://api.mangadex.org/manga?limit=24&offset=48", 0) == 0);
    CHECK(url.find("&order%5Brating%5D=desc") != std::string::npos);
    CHECK(url.find("&includedTags%5B%5D=aaaa-1&includedTagsMode=OR") != std::string::npos);
    CHECK(url.find("not") == std::string::npos);
    CHECK(url.find("&publicationDemographic%5B%5D=seinen") != std::string::npos);
    CHECK(url.find("kids") == std::string::npos);
    CHECK(url.find("&contentRating%5B%5D=safe&contentRating%5B%5D=suggestive") != std::string::npos);

    q.order = "nonsense";
    source.browse(q, 0, [](auto) {});
    CHECK(http.requests.at(1).url.find("&order%5BfollowedCount%5D=desc") != std::string::npos);
    source.tags([](auto) {});
    CHECK_EQ(http.requests.at(2).url, std::string("https://api.mangadex.org/manga/tag"));
}
