#include "common/sources/gutenberg/gutendexparse.h"

#include "common/json.h"
#include "common/sources/webLink/htmltokenizer.h"

#include <algorithm>

namespace gutenberg {

namespace {

const std::string unreadable = "Gutendex sent an answer the app cannot read";

std::string authors(const json::Value& book)
{
    std::string out;
    for (const auto& a : book["authors"].asArray()) {
        std::string name = a["name"].asString();
        // "Austen, Jane" -> "Jane Austen"
        if (const std::size_t comma = name.find(", "); comma != std::string::npos && name.find(", ", comma + 2) == std::string::npos)
            name = name.substr(comma + 2) + " " + name.substr(0, comma);
        if (!name.empty()) out += (out.empty() ? "" : ", ") + name;
    }
    return out;
}

sources::MangaSummary summary(const json::Value& book)
{
    sources::MangaSummary m;
    const auto& id = book["id"];
    m.mangaId = id.isNumber() ? std::to_string(id.asInt()) : std::string();
    m.title = book["title"].asString();
    m.coverUrl = onMirror(book["formats"]["image/jpeg"].asString());
    m.subtitle = authors(book);
    return m;
}

} // namespace

bool isBookId(std::string_view id)
{
    return !id.empty() && id.size() <= 9 && std::all_of(id.begin(), id.end(), [](char c) { return c >= '0' && c <= '9'; });
}

std::string htmlUrl(std::string_view bookId)
{
    const std::string id(bookId);
    return "https://www.gutenberg.org/cache/epub/" + id + "/pg" + id + "-images.html";
}

std::string mirrorHtmlUrl(std::string_view bookId)
{
    const std::string id(bookId);
    return "https://gutenberg.pglaf.org/cache/epub/" + id + "/pg" + id + "-images.html";
}

std::string onMirror(std::string_view url)
{
    const std::string_view from = "https://www.gutenberg.org/cache/";
    if (url.substr(0, from.size()) != from) return std::string(url);
    return "https://gutenberg.pglaf.org/cache/" + std::string(url.substr(from.size()));
}

sources::MangaDetails detailsFromBook(std::string_view html, std::string_view bookId)
{
    sources::MangaDetails d;
    d.mangaId = std::string(bookId);
    const auto tokens = weblink::tokenize(html);
    bool inTitle = false;
    std::string title;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        if (t.kind == weblink::Token::Kind::StartTag && t.name == "title") inTitle = true;
        else if (t.kind == weblink::Token::Kind::EndTag && t.name == "title") inTitle = false;
        else if (t.kind == weblink::Token::Kind::Text && inTitle) title += t.text;
        else if (t.kind == weblink::Token::Kind::Text && d.subtitle.empty()) {
            // The front matter's "Author: Herman Melville" line.
            const std::size_t at = t.text.find("Author:");
            if (at != std::string::npos) {
                std::string name = t.text.substr(at + 7);
                if (name.find_first_not_of(" \t\r\n") == std::string::npos && i + 1 < tokens.size() &&
                    tokens[i + 1].kind == weblink::Token::Kind::Text)
                    name = tokens[i + 1].text;
                const auto first = name.find_first_not_of(" \t\r\n");
                if (first != std::string::npos) d.subtitle = name.substr(first, name.find_first_of("\r\n", first) - first);
            }
        }
    }
    if (const std::size_t bar = title.rfind(" | Project Gutenberg"); bar != std::string::npos) title.resize(bar);
    const auto first = title.find_first_not_of(" \t\r\n");
    d.title = first == std::string::npos ? "Book " + std::string(bookId) : title.substr(first, title.find_last_not_of(" \t\r\n") - first + 1);
    d.coverUrl = "https://gutenberg.pglaf.org/cache/epub/" + std::string(bookId) + "/pg" + std::string(bookId) + ".cover.medium.jpg";
    return d;
}

sources::Result<sources::SearchPage> parseList(std::string_view text, int offset)
{
    const auto parsed = json::parse(text);
    if (!parsed.ok() || !parsed.value["results"].isArray()) return sources::Result<sources::SearchPage>::failure(unreadable);
    sources::Result<sources::SearchPage> r;
    for (const auto& book : parsed.value["results"].asArray())
        if (auto m = summary(book); !m.mangaId.empty() && !m.title.empty()) r.value.items.push_back(std::move(m));
    r.value.total = static_cast<int>(parsed.value["count"].asInt());
    r.value.nextOffset = offset + static_cast<int>(parsed.value["results"].size());
    if (parsed.value["next"].isNull()) r.value.total = std::min(r.value.total, r.value.nextOffset);
    return r;
}

sources::Result<sources::MangaDetails> parseBook(std::string_view text)
{
    const auto parsed = json::parse(text);
    if (!parsed.ok()) return sources::Result<sources::MangaDetails>::failure(unreadable);
    if (parsed.value.contains("detail")) return sources::Result<sources::MangaDetails>::failure("Gutendex: " + parsed.value["detail"].asString());
    const auto m = summary(parsed.value);
    if (m.mangaId.empty()) return sources::Result<sources::MangaDetails>::failure(unreadable);
    sources::Result<sources::MangaDetails> r;
    r.value.mangaId = m.mangaId;
    r.value.title = m.title;
    r.value.coverUrl = m.coverUrl;
    r.value.subtitle = m.subtitle;
    r.value.description = parsed.value["summaries"][0].asString();
    return r;
}

} // namespace gutenberg
