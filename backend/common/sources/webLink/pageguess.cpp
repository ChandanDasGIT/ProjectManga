#include "common/sources/webLink/pageguess.h"

#include "common/json.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <regex>
#include <set>

namespace weblink {

namespace {

std::string lowered(std::string_view s)
{
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string collapsed(std::string_view s)
{
    std::string out;
    bool space = false;
    for (const char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            space = !out.empty();
        } else {
            if (space) out += ' ';
            out += c;
            space = false;
        }
    }
    return out;
}

// "https://a.com/x/y?q" -> "https://a.com"
std::string originOf(std::string_view url)
{
    const std::size_t scheme = url.find("://");
    if (scheme == std::string_view::npos) return {};
    const std::size_t end = url.find_first_of("/?#", scheme + 3);
    return std::string(url.substr(0, end));
}

// Removes "." and ".." segments from an absolute URL's path.
std::string normalizedPath(const std::string& url)
{
    const std::string origin = originOf(url);
    std::string rest = url.substr(origin.size());
    std::string tail;
    if (const std::size_t q = rest.find_first_of("?#"); q != std::string::npos) {
        tail = rest.substr(q);
        rest.resize(q);
    }
    std::vector<std::string> parts;
    std::size_t pos = 0;
    while (pos <= rest.size()) {
        const std::size_t slash = rest.find('/', pos);
        const std::string part = rest.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else if (part != "." && !(part.empty() && pos != 0 && slash != std::string::npos)) {
            parts.push_back(part);
        }
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    std::string path;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0 || !parts[i].empty()) path += '/';
        path += parts[i];
    }
    if (path.empty()) path = "/";
    if (rest.size() > 1 && rest.back() == '/' && path.back() != '/') path += '/';
    return origin + path + tail;
}

bool sameSite(const std::string& a, const std::string& b)
{
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    auto endsWith = [](const std::string& s, const std::string& end) {
        return s.size() > end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0 &&
               s[s.size() - end.size() - 1] == '.';
    };
    return endsWith(a, b) || endsWith(b, a);
}

std::string withoutTrailingSlash(std::string s)
{
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

std::string cleanNumber(const std::string& whole, const std::string& fraction)
{
    std::size_t start = 0;
    while (start + 1 < whole.size() && whole[start] == '0') ++start;
    std::string out = whole.substr(start);
    if (!fraction.empty() && std::any_of(fraction.begin(), fraction.end(), [](char c) { return c != '0'; }))
        out += "." + fraction;
    return out;
}

double toNumber(const std::string& s)
{
    double v = -1.0;
    return json::parseDouble(s, v) == std::errc() ? v : -1.0;
}

int sizeAttribute(const Token& t, std::string_view name)
{
    const std::string v = t.attribute(name);
    int n = -1;
    std::from_chars(v.data(), v.data() + v.size(), n);
    return n;
}

std::string imageSource(const Token& img)
{
    for (const char* name : {"data-src", "data-lazy-src", "data-original", "data-cfsrc", "data-url", "src"}) {
        std::string v = img.attribute(name);
        const auto first = v.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        v = v.substr(first);
        v.erase(v.find_last_not_of(" \t\r\n") + 1);
        if (v.rfind("data:", 0) == 0) continue;
        return v;
    }
    const std::string srcset = img.attribute("data-srcset").empty() ? img.attribute("srcset") : img.attribute("data-srcset");
    const auto first = srcset.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return srcset.substr(first, srcset.find_first_of(" \t\r\n,", first) - first);
}

bool looksLikeDecoration(const std::string& url)
{
    const std::string u = lowered(url.substr(0, url.find_first_of("?#")));
    static const char* words[] = {"logo", "avatar", "icon", "banner", "emoji", "gravatar", "spinner", "loading",
                                  "placeholder", "/ads/", "advert", "pixel", "1x1", "badge", "sprite", "button"};
    for (const char* w : words)
        if (u.find(w) != std::string::npos) return true;
    for (const char* ext : {".svg", ".gif", ".ico"})
        if (u.size() >= 4 && u.compare(u.size() - 4, 4, ext) == 0) return true;
    static const std::regex wordpressThumb(R"(-\d{2,4}x\d{2,4}\.(jpe?g|png|webp)$)");
    return std::regex_search(u, wordpressThumb);
}

std::string folderOf(const std::string& url)
{
    const std::string path = url.substr(0, url.find_first_of("?#"));
    const std::size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(0, slash);
}

} // namespace

bool isWebAddress(std::string_view text)
{
    const std::string t = lowered(text.substr(0, 8));
    std::size_t start = 0;
    if (t.rfind("https://", 0) == 0) start = 8;
    else if (t.rfind("http://", 0) == 0) start = 7;
    else return false;
    if (text.find_first_of(" \t\r\n") != std::string_view::npos) return false;
    const std::size_t end = text.find_first_of("/?#", start);
    const std::string_view host = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
    return !host.empty() && host.find('.') != std::string_view::npos && host.front() != '.';
}

std::string hostOf(std::string_view url)
{
    const std::size_t scheme = url.find("://");
    if (scheme == std::string_view::npos) return {};
    const std::size_t start = scheme + 3;
    const std::size_t end = url.find_first_of("/?#", start);
    std::string host = lowered(url.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
    if (const std::size_t at = host.rfind('@'); at != std::string::npos) host = host.substr(at + 1);
    if (const std::size_t colon = host.find(':'); colon != std::string::npos) host.resize(colon);
    if (host.rfind("www.", 0) == 0) host = host.substr(4);
    return host;
}

std::string resolveUrl(std::string_view base, std::string_view reference)
{
    std::string ref = collapsed(reference);
    if (ref.empty() || ref[0] == '#') return {};
    const std::string lower = lowered(ref.substr(0, 11));
    if (lower.rfind("data:", 0) == 0 || lower.rfind("javascript:", 0) == 0 || lower.rfind("mailto:", 0) == 0 ||
        lower.rfind("tel:", 0) == 0)
        return {};
    if (const std::size_t hash = ref.find('#'); hash != std::string::npos) ref.resize(hash);

    if (const std::size_t scheme = ref.find("://"); scheme != std::string::npos && scheme < 10)
        return normalizedPath(ref);
    const std::string origin = originOf(base);
    if (origin.empty()) return {};
    if (ref.rfind("//", 0) == 0) return normalizedPath(origin.substr(0, origin.find("://") + 1) + ref);
    if (ref[0] == '/') return normalizedPath(origin + ref);
    if (ref[0] == '?') {
        const std::string b(base.substr(0, base.find_first_of("?#")));
        return normalizedPath(b + ref);
    }
    std::string dir(base.substr(0, base.find_first_of("?#")));
    const std::size_t lastSlash = dir.rfind('/');
    if (lastSlash == std::string::npos || lastSlash < origin.size()) dir = origin + "/";
    else dir.resize(lastSlash + 1);
    return normalizedPath(dir + ref);
}

PageInfo guessInfo(const std::vector<Token>& tokens, std::string_view pageUrl)
{
    PageInfo info;
    std::string titleTag;
    std::string metaDescription;
    std::string twitterImage;
    std::string linkImage;
    std::string namedImage;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        if (t.kind != Token::Kind::StartTag) continue;
        if (t.name == "meta") {
            const std::string key = lowered(t.attribute("property").empty() ? t.attribute("name") : t.attribute("property"));
            const std::string content = collapsed(t.attribute("content"));
            if (content.empty()) continue;
            if (key == "og:title" && info.title.empty()) info.title = content;
            else if ((key == "og:image" || key == "og:image:url") && info.coverUrl.empty()) info.coverUrl = resolveUrl(pageUrl, content);
            else if ((key == "twitter:image" || key == "twitter:image:src") && twitterImage.empty()) twitterImage = resolveUrl(pageUrl, content);
            else if (key == "og:description" && info.description.empty()) info.description = content;
            else if (key == "description" && metaDescription.empty()) metaDescription = content;
        } else if (t.name == "link" && linkImage.empty() && lowered(t.attribute("rel")) == "image_src") {
            linkImage = resolveUrl(pageUrl, t.attribute("href"));
        } else if (t.name == "img" && namedImage.empty()) {
            const std::string url = resolveUrl(pageUrl, imageSource(t));
            const std::string hints = lowered(t.attribute("class") + " " + t.attribute("id") + " " + t.attribute("alt") + " " +
                                              url.substr(0, url.find_first_of("?#")));
            const bool named = hints.find("cover") != std::string::npos || hints.find("poster") != std::string::npos ||
                               hints.find("thumb") != std::string::npos;
            const int width = sizeAttribute(t, "width");
            const int height = sizeAttribute(t, "height");
            const bool tiny = (width >= 0 && width < 100) || (height >= 0 && height < 100);
            if (named && !url.empty() && !tiny && !looksLikeDecoration(url)) namedImage = url;
        } else if (t.name == "title" && titleTag.empty()) {
            for (std::size_t k = i + 1; k < tokens.size() && tokens[k].kind == Token::Kind::Text; ++k)
                titleTag += tokens[k].text;
            titleTag = collapsed(titleTag);
        }
    }
    if (info.title.empty()) info.title = titleTag;
    if (info.description.empty()) info.description = metaDescription;
    for (const std::string* candidate : {&twitterImage, &linkImage, &namedImage})
        if (info.coverUrl.empty()) info.coverUrl = *candidate;
    return info;
}

std::string chapterNumber(std::string_view linkText, std::string_view url)
{
    static const std::regex inText(R"((?:^|[^a-z])(?:chapter|chap|ch|episode|ep)\.?\s*[-_#:]?\s*(\d+)(?:[.,](\d+))?)",
                                   std::regex::icase);
    static const std::regex inUrl(R"((?:^|[/_.\-])(?:chapter|chap|ch|episode|ep)[-_/.]*(\d+)(?:[-_.](\d{1,2}))?(?=[/?#]|\.html?|$))",
                                  std::regex::icase);
    std::smatch m;
    const std::string text(linkText);
    if (std::regex_search(text, m, inText)) return cleanNumber(m[1].str(), m[2].str());

    std::string path(url);
    const std::size_t scheme = path.find("://");
    if (scheme != std::string::npos) {
        const std::size_t slash = path.find('/', scheme + 3);
        path = slash == std::string::npos ? std::string() : path.substr(slash);
    }
    if (std::regex_search(path, m, inUrl)) return cleanNumber(m[1].str(), m[2].str());
    return {};
}

std::vector<sources::Chapter> guessChapters(const std::vector<Token>& tokens, std::string_view pageUrl)
{
    const std::string pageHost = hostOf(pageUrl);
    const std::string self = withoutTrailingSlash(std::string(pageUrl.substr(0, pageUrl.find('#'))));
    std::map<double, sources::Chapter> byNumber;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        if (t.kind != Token::Kind::StartTag || t.name != "a" || !t.hasAttribute("href")) continue;
        const std::string url = resolveUrl(pageUrl, t.attribute("href"));
        if (url.empty() || !sameSite(hostOf(url), pageHost) || withoutTrailingSlash(url) == self) continue;

        std::string text;
        for (std::size_t k = i + 1; k < tokens.size(); ++k) {
            const Token& inner = tokens[k];
            if ((inner.kind == Token::Kind::EndTag && inner.name == "a") ||
                (inner.kind == Token::Kind::StartTag && inner.name == "a"))
                break;
            if (inner.kind == Token::Kind::Text) text += inner.text + " ";
        }
        text = collapsed(text);

        const std::string number = chapterNumber(text, url);
        const double value = toNumber(number);
        if (number.empty() || value < 0 || byNumber.count(value)) continue;

        sources::Chapter c;
        c.id = url;
        c.number = number;
        c.numberValue = value;
        byNumber.emplace(value, std::move(c));
    }

    std::vector<sources::Chapter> out;
    out.reserve(byNumber.size());
    for (auto& [n, c] : byNumber) out.push_back(std::move(c));
    return out;
}

std::vector<std::string> guessImages(const std::vector<Token>& tokens, std::string_view pageUrl)
{
    std::vector<std::string> candidates;
    std::set<std::string> seen;
    for (const Token& t : tokens) {
        if (t.kind != Token::Kind::StartTag || t.name != "img") continue;
        const int width = sizeAttribute(t, "width");
        const int height = sizeAttribute(t, "height");
        if ((width >= 0 && width < 200) || (height >= 0 && height < 200)) continue;
        const std::string url = resolveUrl(pageUrl, imageSource(t));
        if (url.empty() || looksLikeDecoration(url) || !seen.insert(url).second) continue;
        candidates.push_back(url);
    }

    std::map<std::string, int> perFolder;
    for (const auto& url : candidates) ++perFolder[folderOf(url)];
    std::string bestFolder;
    int best = 0;
    for (const auto& url : candidates) {                     // ties: the folder that comes first
        const int count = perFolder[folderOf(url)];
        if (count > best) {
            best = count;
            bestFolder = folderOf(url);
        }
    }
    if (best < 2) return candidates;

    std::vector<std::string> pages;
    for (const auto& url : candidates)
        if (folderOf(url) == bestFolder) pages.push_back(url);
    return pages;
}

} // namespace weblink
