#pragma once

#include "common/sources/mangatypes.h"
#include "common/sources/webLink/htmltokenizer.h"

#include <string>
#include <string_view>
#include <vector>

// Rules that find a manga's parts on a page the app has never seen: which links are chapters and
// which images are pages. They are guesses: they work on pages that list plain links and images,
// and miss pages built by JavaScript.
namespace weblink {

// "http(s)://host/..." with no spaces.
bool isWebAddress(std::string_view text);

// `reference` (absolute, "//host/x", "/x", "x", "../x") made absolute against `base`;
// empty for data:, javascript:, mailto: and "#..." links. Drops the "#..." part.
std::string resolveUrl(std::string_view base, std::string_view reference);

// The host in lower case without "www." and port: "https://www.Site.com:8080/a" -> "site.com".
std::string hostOf(std::string_view url);

struct PageInfo {
    std::string title;
    std::string coverUrl;
    std::string description;
};
// From og:title / <title> and og:description / description. The cover, first found wins:
// og:image, twitter:image, <link rel="image_src">, an image called or marked "cover", "poster"
// or "thumb". Empty when none of these is on the page.
PageInfo guessInfo(const std::vector<Token>& tokens, std::string_view pageUrl);

// The chapter number in a link's text ("Chapter 12.5", "Ch. 7", "Episode 3") or, failing that, in
// its address (".../chapter-261/", "...-ch-12-5"); empty when there is none.
std::string chapterNumber(std::string_view linkText, std::string_view url);

// Links on the same site that carry a chapter number, one per number, oldest first.
std::vector<sources::Chapter> guessChapters(const std::vector<Token>& tokens, std::string_view pageUrl);

// The page images of a chapter page, in reading order: lazy-load attributes first, small and
// decoration images left out, then the largest set of images from one folder.
std::vector<std::string> guessImages(const std::vector<Token>& tokens, std::string_view pageUrl);

} // namespace weblink
