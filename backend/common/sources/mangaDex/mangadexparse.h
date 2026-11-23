#pragma once

#include "common/sources/mangatypes.h"

#include <cstdint>
#include <string>
#include <string_view>

// Turns MangaDex API answers (api.mangadex.org, JSON) into the source types. No network here.
namespace mangadex {

sources::Result<sources::SearchPage> parseSearch(std::string_view body);
sources::Result<sources::MangaDetails> parseManga(std::string_view body);

struct FeedPage {
    std::vector<sources::Chapter> chapters;
    int total = 0;
    int offset = 0;
    int limit = 0;
};
sources::Result<FeedPage> parseFeed(std::string_view body);

// The tag list (/manga/tag), sorted by group and name.
sources::Result<std::vector<sources::Tag>> parseTags(std::string_view body);

// The image server's answer for one chapter: the full-quality page links, in order.
sources::Result<sources::PageList> parseAtHome(std::string_view body);

// MangaDex's error text from an error answer; empty when the body is not one.
std::string errorMessage(std::string_view body);

// "2023-05-01T12:34:56+00:00" (or with Z) to ms since 1970; 0 when it cannot be read.
std::int64_t parseIsoTime(std::string_view text);

// The 256-pixel-wide thumbnail of a cover file.
std::string coverUrl(std::string_view mangaId, std::string_view fileName);

} // namespace mangadex
