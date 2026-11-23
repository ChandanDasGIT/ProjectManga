#include "common/sources/gutenberg/gutenbergcatalog.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace gutenberg {

namespace {

std::string lowered(std::string_view s)
{
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string trimmed(std::string_view s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return std::string(s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1));
}

// One CSV record (RFC 4180: quoted fields may hold commas, quotes as "" and line breaks).
bool readRecord(std::string_view text, std::size_t& pos, std::vector<std::string>& fields)
{
    fields.clear();
    if (pos >= text.size()) return false;
    std::string field;
    bool quoted = false;
    while (pos < text.size()) {
        const char c = text[pos++];
        if (quoted) {
            if (c == '"') {
                if (pos < text.size() && text[pos] == '"') {
                    field += '"';
                    ++pos;
                } else {
                    quoted = false;
                }
            } else {
                field += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(std::move(field));
            field.clear();
        } else if (c == '\n') {
            break;
        } else if (c != '\r') {
            field += c;
        }
    }
    fields.push_back(std::move(field));
    return true;
}

std::vector<std::string> words(std::string_view query)
{
    std::vector<std::string> out;
    std::string current;
    for (char c : lowered(query)) {
        if (std::isalnum(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) >= 0x80) {
            current += c;
        } else if (!current.empty()) {
            out.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) out.push_back(std::move(current));
    return out;
}

} // namespace

std::string readableAuthors(std::string_view field)
{
    std::string out;
    std::size_t start = 0;
    while (start <= field.size()) {
        const std::size_t end = field.find(';', start);
        std::string one = trimmed(field.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (const auto bracket = one.find(" ["); bracket != std::string::npos) one.resize(bracket);   // "[Editor]"
        // "Melville, Herman, 1819-1891": drop the years, then "Last, First" -> "First Last".
        std::vector<std::string> parts;
        std::size_t p = 0;
        while (p <= one.size()) {
            const std::size_t comma = one.find(',', p);
            parts.push_back(trimmed(std::string_view(one).substr(p, comma == std::string::npos ? std::string::npos : comma - p)));
            if (comma == std::string::npos) break;
            p = comma + 1;
        }
        std::erase_if(parts, [](const std::string& s) {
            return s.empty() || std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '?' || c == ' ' || c == '.'; }) ||
                   s.find("BCE") != std::string::npos || s.find("cent") != std::string::npos;
        });
        std::string name = parts.size() >= 2 ? parts[1] + " " + parts[0] : parts.empty() ? std::string() : parts[0];
        if (!name.empty()) out += (out.empty() ? "" : ", ") + name;
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return out;
}

std::string coverOnMirror(int id)
{
    const std::string n = std::to_string(id);
    return "https://gutenberg.pglaf.org/cache/epub/" + n + "/pg" + n + ".cover.medium.jpg";
}

bool Catalog::load(std::string_view csv)
{
    m_books.clear();
    std::size_t pos = 0;
    std::vector<std::string> fields;
    if (!readRecord(csv, pos, fields) || fields.size() < 6 || fields[0] != "Text#") return false;
    while (readRecord(csv, pos, fields)) {
        if (fields.size() < 6 || fields[1] != "Text") continue;
        const std::string languages = fields[4];
        if (languages != "en" && languages.rfind("en;", 0) != 0 && languages.find("; en") == std::string::npos) continue;
        CatalogBook b;
        std::from_chars(fields[0].data(), fields[0].data() + fields[0].size(), b.id);
        if (b.id <= 0) continue;
        b.title = trimmed(std::string_view(fields[3]).substr(0, fields[3].find('\n')));
        b.authors = readableAuthors(fields[5]);
        b.issued = fields[2];
        b.subjects = fields.size() > 6 ? fields[6] : std::string();
        if (!b.title.empty()) m_books.push_back(std::move(b));
    }
    std::sort(m_books.begin(), m_books.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return !m_books.empty();
}

std::vector<const CatalogBook*> Catalog::search(std::string_view query) const
{
    const auto wanted = words(query);
    std::vector<std::pair<int, const CatalogBook*>> hits;            // (words in the title, book)
    if (wanted.empty()) return {};
    for (const auto& b : m_books) {
        const std::string title = lowered(b.title);
        const std::string authors = lowered(b.authors);
        int inTitle = 0;
        bool all = true;
        for (const auto& w : wanted) {
            if (title.find(w) != std::string::npos) ++inTitle;
            else if (authors.find(w) == std::string::npos) { all = false; break; }
        }
        if (all) hits.push_back({inTitle, &b});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second->title.size() < b.second->title.size();
    });
    std::vector<const CatalogBook*> out;
    for (const auto& h : hits) out.push_back(h.second);
    return out;
}

std::vector<const CatalogBook*> Catalog::recent(std::size_t count) const
{
    std::vector<const CatalogBook*> out;
    for (const auto& b : m_books) out.push_back(&b);
    std::stable_sort(out.begin(), out.end(), [](const auto* a, const auto* b) {
        return a->issued != b->issued ? a->issued > b->issued : a->id > b->id;
    });
    if (out.size() > count) out.resize(count);
    return out;
}

const CatalogBook* Catalog::find(int id) const
{
    const auto it = std::lower_bound(m_books.begin(), m_books.end(), id, [](const auto& b, int i) { return b.id < i; });
    return it != m_books.end() && it->id == id ? &*it : nullptr;
}

sources::MangaSummary Catalog::summary(const CatalogBook& b)
{
    return {std::to_string(b.id), b.title, coverOnMirror(b.id), b.authors};
}

sources::MangaDetails Catalog::details(const CatalogBook& b)
{
    sources::MangaDetails d;
    d.mangaId = std::to_string(b.id);
    d.title = b.title;
    d.coverUrl = coverOnMirror(b.id);
    d.subtitle = b.authors;
    if (!b.subjects.empty()) d.description = "Subjects: " + b.subjects;
    return d;
}

} // namespace gutenberg
