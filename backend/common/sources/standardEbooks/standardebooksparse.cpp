#include "common/sources/standardEbooks/standardebooksparse.h"

#include "common/sources/webLink/htmltokenizer.h"
#include "common/sources/webLink/pageguess.h"

#include <algorithm>

namespace standardebooks {

namespace {

using weblink::Token;
const std::string site = "https://standardebooks.org";

std::string collapsed(const std::string& s)
{
    std::string out;
    bool space = false;
    for (char c : s) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
            space = !out.empty();
        } else {
            if (space) out += ' ';
            out += c;
            space = false;
        }
    }
    return out;
}

// The text inside the element starting at tokens[i], up to its end tag.
std::string textOf(const std::vector<Token>& tokens, std::size_t i)
{
    const std::string name = tokens[i].name;
    std::string out;
    int depth = 0;
    for (std::size_t k = i; k < tokens.size(); ++k) {
        const Token& t = tokens[k];
        if (t.kind == Token::Kind::StartTag && t.name == name) ++depth;
        else if (t.kind == Token::Kind::EndTag && t.name == name && --depth == 0) break;
        else if (t.kind == Token::Kind::Text) out += t.text;
    }
    return collapsed(out);
}

} // namespace

bool isBookId(std::string_view id)
{
    if (id.rfind("/ebooks/", 0) != 0 || id.size() > 300) return false;
    const auto rest = id.substr(8);
    if (rest.empty() || rest.find('/') == std::string_view::npos) return false;
    return std::all_of(rest.begin(), rest.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '/' || c == '_';
    });
}

sources::Result<sources::SearchPage> parseList(std::string_view html, int offset)
{
    const auto tokens = weblink::tokenize(html);
    sources::Result<sources::SearchPage> r;
    sources::MangaSummary* current = nullptr;
    bool inAuthor = false;
    bool hasNext = false;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        if (t.kind == Token::Kind::StartTag && t.name == "li" && t.attribute("typeof") == "schema:Book" &&
            isBookId(t.attribute("about"))) {
            r.value.items.push_back({t.attribute("about"), "", "", ""});
            current = &r.value.items.back();
            inAuthor = false;
            continue;
        }
        if (t.kind == Token::Kind::StartTag && t.name == "a" && t.attribute("rel") == "next") hasNext = true;
        if (t.kind == Token::Kind::StartTag && t.name == "a" && collapsed(t.attribute("aria-disabled")) != "true" &&
            i + 1 < tokens.size() && tokens[i + 1].kind == Token::Kind::Text && collapsed(tokens[i + 1].text) == "Next" &&
            t.hasAttribute("href"))
            hasNext = true;
        if (!current || t.kind != Token::Kind::StartTag) continue;
        if (t.name == "img" && current->coverUrl.empty() && !t.attribute("src").empty())
            current->coverUrl = weblink::resolveUrl(site + "/ebooks", t.attribute("src"));
        if (t.name == "p" && t.attribute("class") == "author") inAuthor = true;
        if (t.name == "span" && t.attribute("property") == "schema:name") {
            const std::string text = textOf(tokens, i);
            if (inAuthor) {
                if (current->subtitle.empty()) current->subtitle = text;
                else current->subtitle += ", " + text;
            } else if (current->title.empty()) {
                current->title = text;
            }
        }
    }
    std::erase_if(r.value.items, [](const auto& m) { return m.title.empty(); });
    const int shown = static_cast<int>(r.value.items.size());
    r.value.nextOffset = offset + shown;
    r.value.total = offset + shown + (hasNext && shown > 0 ? 1 : 0);
    return r;
}

sources::Result<sources::MangaDetails> parseBook(std::string_view html, const std::string& bookId)
{
    const auto tokens = weblink::tokenize(html);
    sources::Result<sources::MangaDetails> r;
    r.value.mangaId = bookId;
    std::string titleTag;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        if (t.kind != Token::Kind::StartTag) continue;
        if (t.name == "title" && titleTag.empty()) titleTag = textOf(tokens, i);
        if (t.name == "meta" && r.value.coverUrl.empty() && t.attribute("property") == "og:image")
            r.value.coverUrl = weblink::resolveUrl(site + bookId, t.attribute("content"));
        if (t.attribute("property") == "schema:description" && r.value.description.empty()) {
            // Its paragraphs, one per line.
            std::string text;
            int depth = 0;
            for (std::size_t k = i; k < tokens.size(); ++k) {
                const Token& u = tokens[k];
                if (u.kind == Token::Kind::StartTag && u.name == t.name) ++depth;
                else if (u.kind == Token::Kind::EndTag && u.name == t.name && --depth == 0) break;
                else if (u.kind == Token::Kind::EndTag && u.name == "p") text += "\n\n";
                else if (u.kind == Token::Kind::Text) text += u.text;
            }
            std::string tidy;
            for (std::size_t a = 0; a < text.size();) {
                const std::size_t b = text.find("\n\n", a);
                const std::string part = collapsed(text.substr(a, b == std::string::npos ? std::string::npos : b - a));
                if (!part.empty()) tidy += (tidy.empty() ? "" : "\n\n") + part;
                if (b == std::string::npos) break;
                a = b + 2;
            }
            r.value.description = tidy;
        }
    }
    // "Pride and Prejudice, by Jane Austen - Free ebook download - Standard Ebooks: ..."
    std::string head = titleTag.substr(0, titleTag.find(" - Free ebook"));
    const std::size_t by = head.rfind(", by ");
    if (by != std::string::npos) {
        r.value.title = head.substr(0, by);
        r.value.subtitle = head.substr(by + 5);
    } else {
        r.value.title = head;
    }
    if (r.value.title.empty()) return sources::Result<sources::MangaDetails>::failure("Standard Ebooks sent a page the app cannot read");
    return r;
}

} // namespace standardebooks
