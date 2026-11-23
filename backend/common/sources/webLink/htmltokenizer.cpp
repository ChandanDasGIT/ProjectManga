#include "common/sources/webLink/htmltokenizer.h"

#include <cctype>
#include <cstdint>

namespace weblink {

namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool isLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

char lower(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

void appendUtf8(std::string& out, std::uint32_t cp)
{
    if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

struct Named {
    const char* name;
    std::uint32_t cp;
};

constexpr Named namedEntities[] = {
    {"amp", '&'},       {"lt", '<'},         {"gt", '>'},         {"quot", '"'},      {"apos", '\''},
    {"nbsp", ' '},      {"hellip", 0x2026},  {"ndash", 0x2013},   {"mdash", 0x2014},  {"lsquo", 0x2018},
    {"rsquo", 0x2019},  {"ldquo", 0x201C},   {"rdquo", 0x201D},   {"laquo", 0x00AB},  {"raquo", 0x00BB},
    {"middot", 0x00B7}, {"bull", 0x2022},    {"copy", 0x00A9},    {"times", 0x00D7},
};

// Ends a tag that holds raw text (script, style): everything up to </name> is skipped.
std::size_t skipRawText(std::string_view html, std::size_t pos, const std::string& name)
{
    while (true) {
        const std::size_t open = html.find("</", pos);
        if (open == std::string_view::npos) return html.size();
        std::size_t i = open + 2;
        std::size_t k = 0;
        while (k < name.size() && i < html.size() && lower(html[i]) == name[k]) { ++i; ++k; }
        if (k == name.size() && (i >= html.size() || !isLetter(html[i]))) return open;
        pos = open + 2;
    }
}

} // namespace

std::string Token::attribute(std::string_view wanted) const
{
    for (const auto& [n, v] : attributes)
        if (n == wanted) return v;
    return {};
}

bool Token::hasAttribute(std::string_view wanted) const
{
    for (const auto& a : attributes)
        if (a.first == wanted) return true;
    return false;
}

std::string decodeEntities(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '&') {
            out += text[i++];
            continue;
        }
        const std::size_t semi = text.find(';', i + 1);
        if (semi == std::string_view::npos || semi - i > 12) {
            out += text[i++];
            continue;
        }
        const std::string_view body = text.substr(i + 1, semi - i - 1);
        bool done = false;
        if (body.size() >= 2 && body[0] == '#') {
            std::uint32_t cp = 0;
            const bool hex = body[1] == 'x' || body[1] == 'X';
            bool valid = body.size() > (hex ? 2u : 1u);
            for (std::size_t k = hex ? 2 : 1; k < body.size() && valid; ++k) {
                const char c = body[k];
                int d = -1;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (hex && c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (hex && c >= 'A' && c <= 'F') d = c - 'A' + 10;
                if (d < 0 || cp > 0x10FFFF) valid = false;
                else cp = cp * (hex ? 16 : 10) + static_cast<std::uint32_t>(d);
            }
            if (valid) {
                appendUtf8(out, cp);
                done = true;
            }
        } else {
            for (const auto& e : namedEntities)
                if (body == e.name) {
                    appendUtf8(out, e.cp);
                    done = true;
                    break;
                }
        }
        if (done) {
            i = semi + 1;
        } else {
            out += text[i++];
        }
    }
    return out;
}

std::vector<Token> tokenize(std::string_view html)
{
    std::vector<Token> tokens;
    std::string text;
    auto flushText = [&] {
        if (text.empty()) return;
        Token t;
        t.kind = Token::Kind::Text;
        t.text = decodeEntities(text);
        tokens.push_back(std::move(t));
        text.clear();
    };

    std::size_t i = 0;
    const std::size_t n = html.size();
    while (i < n) {
        const char c = html[i];
        if (c != '<' || i + 1 >= n) {
            text += c;
            ++i;
            continue;
        }
        const char next = html[i + 1];

        if (html.substr(i, 4) == "<!--") {
            flushText();
            const std::size_t end = html.find("-->", i + 4);
            i = end == std::string_view::npos ? n : end + 3;
            continue;
        }
        if (next == '!' || next == '?') {
            flushText();
            const std::size_t end = html.find('>', i + 2);
            i = end == std::string_view::npos ? n : end + 1;
            continue;
        }
        if (next == '/' && i + 2 < n && isLetter(html[i + 2])) {
            flushText();
            std::size_t k = i + 2;
            Token t;
            t.kind = Token::Kind::EndTag;
            while (k < n && !isSpace(html[k]) && html[k] != '>' && html[k] != '/') t.name += lower(html[k++]);
            const std::size_t end = html.find('>', k);
            i = end == std::string_view::npos ? n : end + 1;
            tokens.push_back(std::move(t));
            continue;
        }
        if (!isLetter(next)) {                                   // a stray '<' is text
            text += c;
            ++i;
            continue;
        }

        flushText();
        Token t;
        t.kind = Token::Kind::StartTag;
        std::size_t k = i + 1;
        while (k < n && !isSpace(html[k]) && html[k] != '>' && html[k] != '/') t.name += lower(html[k++]);

        while (k < n) {
            while (k < n && (isSpace(html[k]) || html[k] == '/')) ++k;
            if (k >= n || html[k] == '>') break;
            std::string name;
            while (k < n && !isSpace(html[k]) && html[k] != '=' && html[k] != '>' && html[k] != '/')
                name += lower(html[k++]);
            while (k < n && isSpace(html[k])) ++k;
            std::string value;
            if (k < n && html[k] == '=') {
                ++k;
                while (k < n && isSpace(html[k])) ++k;
                if (k < n && (html[k] == '"' || html[k] == '\'')) {
                    const char quote = html[k++];
                    const std::size_t end = html.find(quote, k);
                    const std::size_t stop = end == std::string_view::npos ? n : end;
                    value = std::string(html.substr(k, stop - k));
                    k = stop == n ? n : stop + 1;
                } else {
                    while (k < n && !isSpace(html[k]) && html[k] != '>') value += html[k++];
                }
            }
            if (!name.empty()) t.attributes.emplace_back(std::move(name), decodeEntities(value));
        }
        i = k < n ? k + 1 : n;

        const std::string name = t.name;
        tokens.push_back(std::move(t));
        if (name == "script" || name == "style") i = skipRawText(html, i, name);
    }
    flushText();
    return tokens;
}

} // namespace weblink
