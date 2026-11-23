#include "common/sources/mangaDex/mangadexsource.h"

#include "common/sources/mangaDex/mangadexparse.h"

#include <algorithm>

namespace mangadex {

namespace {

const std::string api = "https://api.mangadex.org";

// MangaDex ids are UUIDs; anything else must not end up in a URL path.
bool isId(const std::string& id)
{
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-';
    });
}

// "&contentRating%5B%5D=safe&..." for the ratings MangaDex knows; empty input: safe only.
std::string ratingParams(const std::vector<std::string>& ratings)
{
    std::string out;
    for (const auto& r : ratings)
        if (r == "safe" || r == "suggestive" || r == "erotica") out += "&contentRating%5B%5D=" + r;
    return out.empty() ? "&contentRating%5B%5D=safe" : out;
}

template <typename T>
sources::Result<T> badId()
{
    return sources::Result<T>::failure("Not a MangaDex id");
}

} // namespace

std::string percentEncode(std::string_view text)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
            c == '_' || c == '~') {
            out += ch;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string failureText(const platform::HttpResponse& response)
{
    const std::string why = errorMessage(response.body);
    if (!why.empty()) return "MangaDex: " + why;
    if (response.status == 429) return "MangaDex is limiting requests right now; try again in a minute";
    return "Could not reach MangaDex (" + response.problem() + ")";
}

MangaDexSource::MangaDexSource(platform::HttpClient& http, platform::Scheduler& scheduler)
    : m_http(http), m_api(scheduler, 5, 1000), m_atHome(scheduler, 40, 60000)
{
}

void MangaDexSource::request(platform::RateLimiter& limiter, const std::string& url,
                             std::function<void(platform::HttpResponse)> done)
{
    limiter.run([this, url, done = std::move(done)]() mutable { m_http.get({url, {}}, std::move(done)); });
}

void MangaDexSource::search(const std::string& query, int offset, const std::vector<std::string>& contentRatings,
                            std::function<void(sources::Result<sources::SearchPage>)> done)
{
    const std::string url = api + "/manga?title=" + percentEncode(query) + "&limit=" + std::to_string(searchPageSize) +
                            "&offset=" + std::to_string(std::max(0, offset)) +
                            "&includes%5B%5D=cover_art&availableTranslatedLanguage%5B%5D=en&order%5Brelevance%5D=desc" +
                            ratingParams(contentRatings);
    request(m_api, url, [done = std::move(done)](platform::HttpResponse response) {
        if (!response.ok()) return done(sources::Result<sources::SearchPage>::failure(failureText(response)));
        done(parseSearch(response.body));
    });
}

void MangaDexSource::browse(const sources::BrowseQuery& query, int offset,
                            std::function<void(sources::Result<sources::SearchPage>)> done)
{
    static const char* orders[] = {"followedCount", "latestUploadedChapter", "rating", "createdAt"};
    const bool knownOrder = std::any_of(std::begin(orders), std::end(orders), [&](const char* o) { return query.order == o; });
    std::string url = api + "/manga?limit=" + std::to_string(std::clamp(query.limit, 1, 100)) +
                      "&offset=" + std::to_string(std::max(0, offset)) +
                      "&includes%5B%5D=cover_art&availableTranslatedLanguage%5B%5D=en&hasAvailableChapters=true" +
                      "&order%5B" + (knownOrder ? query.order : std::string("followedCount")) + "%5D=desc" +
                      ratingParams(query.contentRatings);
    for (const auto& tag : query.includedTags)
        if (isId(tag)) url += "&includedTags%5B%5D=" + tag;
    if (!query.includedTags.empty()) url += query.anyTag ? "&includedTagsMode=OR" : "&includedTagsMode=AND";
    for (const auto& d : query.demographics)
        if (d == "shounen" || d == "shoujo" || d == "seinen" || d == "josei") url += "&publicationDemographic%5B%5D=" + d;
    request(m_api, url, [done = std::move(done)](platform::HttpResponse response) {
        if (!response.ok()) return done(sources::Result<sources::SearchPage>::failure(failureText(response)));
        done(parseSearch(response.body));
    });
}

void MangaDexSource::tags(std::function<void(sources::Result<std::vector<sources::Tag>>)> done)
{
    request(m_api, api + "/manga/tag", [done = std::move(done)](platform::HttpResponse response) {
        if (!response.ok()) return done(sources::Result<std::vector<sources::Tag>>::failure(failureText(response)));
        done(parseTags(response.body));
    });
}

void MangaDexSource::details(const std::string& mangaId,
                             std::function<void(sources::Result<sources::MangaDetails>)> done)
{
    if (!isId(mangaId)) return done(badId<sources::MangaDetails>());
    const std::string url = api + "/manga/" + mangaId + "?includes%5B%5D=cover_art&includes%5B%5D=author";
    request(m_api, url, [done = std::move(done)](platform::HttpResponse response) {
        if (!response.ok()) return done(sources::Result<sources::MangaDetails>::failure(failureText(response)));
        done(parseManga(response.body));
    });
}

void MangaDexSource::chapters(const std::string& mangaId,
                              std::function<void(sources::Result<std::vector<sources::Chapter>>)> done)
{
    if (!isId(mangaId)) return done(badId<std::vector<sources::Chapter>>());
    loadFeed(mangaId, 0, std::make_shared<std::vector<sources::Chapter>>(), std::move(done));
}

void MangaDexSource::loadFeed(const std::string& mangaId, int offset,
                              std::shared_ptr<std::vector<sources::Chapter>> collected,
                              std::function<void(sources::Result<std::vector<sources::Chapter>>)> done)
{
    const std::string url = api + "/manga/" + mangaId + "/feed?translatedLanguage%5B%5D=en&limit=" +
                            std::to_string(feedPageSize) + "&offset=" + std::to_string(offset) +
                            "&order%5Bchapter%5D=asc&includes%5B%5D=scanlation_group"
                            "&contentRating%5B%5D=safe&contentRating%5B%5D=suggestive"
                            "&contentRating%5B%5D=erotica&contentRating%5B%5D=pornographic";
    request(m_api, url, [this, mangaId, offset, collected, done = std::move(done)](platform::HttpResponse response) mutable {
        using ChapterList = sources::Result<std::vector<sources::Chapter>>;
        if (!response.ok()) return done(ChapterList::failure(failureText(response)));
        auto page = parseFeed(response.body);
        if (!page.ok()) return done(ChapterList::failure(page.error));
        const std::size_t got = page.value.chapters.size();
        for (auto& c : page.value.chapters) collected->push_back(std::move(c));

        const int next = offset + static_cast<int>(got);
        // MangaDex serves at most 10000 results of a list; stop there as well.
        if (got > 0 && next < page.value.total && next < 10000) {
            loadFeed(mangaId, next, collected, std::move(done));
            return;
        }
        ChapterList r;
        r.value = std::move(*collected);
        done(std::move(r));
    });
}

void MangaDexSource::pages(const sources::Chapter& chapter, std::function<void(sources::Result<sources::PageList>)> done)
{
    if (!chapter.externalUrl.empty())
        return done(sources::Result<sources::PageList>::failure("This chapter is published on an official site"));
    if (!isId(chapter.id)) return done(badId<sources::PageList>());
    request(m_atHome, api + "/at-home/server/" + chapter.id, [done = std::move(done)](platform::HttpResponse response) {
        if (!response.ok()) return done(sources::Result<sources::PageList>::failure(failureText(response)));
        done(parseAtHome(response.body));
    });
}

} // namespace mangadex
