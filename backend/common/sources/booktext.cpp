#include "common/sources/booktext.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>

namespace sources {

namespace {

using weblink::Token;

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

// Collapses runs of spaces (keeping single line breaks from <br>) and trims.
std::string tidy(const std::string& s)
{
    std::string out;
    bool space = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\n') {
            while (!out.empty() && out.back() == ' ') out.pop_back();
            if (!out.empty() && out.back() != '\n') out += '\n';
            space = false;
        } else if (isSpace(c)) {
            space = !out.empty() && out.back() != '\n';
        } else {
            if (space) out += ' ';
            out += c;
            space = false;
        }
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
    // A no-break space from &nbsp; counts as text only when something else is there.
    if (out == "\xC2\xA0") out.clear();
    return out;
}

// Printed page numbers ("{ix}", "[23]") that Project Gutenberg keeps in <span class="pagenum">.
bool isPageNumber(const Token& t)
{
    return t.kind == Token::Kind::StartTag && t.attribute("class").find("pagenum") != std::string::npos;
}

// Skips an element and what is inside it: call for every token; true while inside.
struct Skipper {
    std::string tag;
    int depth = 0;
    bool skip(const Token& t)
    {
        if (tag.empty()) {
            if (!isPageNumber(t)) return false;
            tag = t.name;
            depth = 1;
            return true;
        }
        if (t.kind == Token::Kind::StartTag && t.name == tag) ++depth;
        else if (t.kind == Token::Kind::EndTag && t.name == tag && --depth == 0) tag.clear();
        return true;
    }
};

bool isHeading(const std::string& name)
{
    return name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6";
}

// Block elements that end an open (unclosed) paragraph.
bool closesParagraph(const std::string& name)
{
    static const std::set<std::string> blocks = {"p", "div", "section", "article", "header", "footer", "blockquote",
                                                 "ul", "ol", "li", "table", "hr", "hgroup", "main", "aside", "nav",
                                                 "h1", "h2", "h3", "h4", "h5", "h6"};
    return blocks.count(name) > 0;
}

// Containers whose start and end tags are tracked to know what holds what.
bool isContainer(const std::string& name)
{
    return name == "div" || name == "section" || name == "article" || name == "main" || name == "header" ||
           name == "footer" || name == "aside" || name == "nav" || name == "blockquote" || name == "td" || name == "body";
}

std::string headingTitle(const std::string& heading)
{
    const std::string t = tidy(heading);
    return isBareNumber(t) ? "Chapter " + t : t;
}

bool chapterSection(const Token& t)
{
    if (t.name != "section") return false;
    const std::string type = t.attribute("epub:type");
    const std::string id = t.attribute("id");
    return type.find("chapter") != std::string::npos || id.rfind("chapter-", 0) == 0;
}

// The CSS classes a page hides (display: none) in its <style> blocks.
std::set<std::string> hiddenClasses(std::string_view html)
{
    std::set<std::string> out;
    static const std::regex rule(R"(\.([A-Za-z_][\w-]*)\s*\{[^}]*display\s*:\s*none)", std::regex::icase);
    const std::string text(html);
    for (auto it = std::sregex_iterator(text.begin(), text.end(), rule); it != std::sregex_iterator(); ++it)
        out.insert((*it)[1].str());
    return out;
}

bool hidden(const Token& t, const std::set<std::string>& hiddenSet)
{
    std::string style = t.attribute("style");
    style.erase(std::remove_if(style.begin(), style.end(), isSpace), style.end());
    if (style.find("display:none") != std::string::npos) return true;
    const std::string cls = t.attribute("class");
    std::size_t pos = 0;
    while (pos < cls.size()) {
        const std::size_t end = cls.find(' ', pos);
        const std::string one = cls.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (!one.empty() && hiddenSet.count(one)) return true;
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return t.hasAttribute("hidden");
}

} // namespace

bool isBareNumber(std::string_view heading)
{
    std::string s;
    for (char c : heading)
        if (!isSpace(c) && c != '.') s += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (s.empty()) return false;
    const bool digits = std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
    const bool roman = std::all_of(s.begin(), s.end(), [](char c) { return std::string("IVXLCDM").find(c) != std::string::npos; });
    return digits || roman;
}

std::vector<BookChapter> splitBook(const std::vector<Token>& tokens)
{
    const bool sectionMode = std::any_of(tokens.begin(), tokens.end(), [](const Token& t) {
        return t.kind == Token::Kind::StartTag && chapterSection(t);
    });
    std::string splitHeading;                       // heading mode: the tag that starts a chapter
    if (!sectionMode) {
        int h2 = 0, h3 = 0;
        for (const auto& t : tokens)
            if (t.kind == Token::Kind::StartTag) {
                h2 += t.name == "h2";
                h3 += t.name == "h3";
            }
        splitHeading = h2 >= 2 ? "h2" : h3 >= 2 ? "h3" : "";
    }

    enum class Mark { None, Chapter, Skip };
    std::vector<std::pair<std::string, Mark>> stack;
    auto count = [&](Mark m) {
        return std::count_if(stack.begin(), stack.end(), [m](const auto& e) { return e.second == m; });
    };

    std::vector<BookChapter> chapters;
    bool headingChapterOpen = false;                // heading mode: a chapter has started
    if (!sectionMode && splitHeading.empty()) {     // no structure at all: one chapter
        chapters.push_back({});
        headingChapterOpen = true;
    }
    auto inChapter = [&] { return count(Mark::Skip) == 0 && (sectionMode ? count(Mark::Chapter) > 0 : headingChapterOpen); };

    std::string heading;
    std::string headingTag;                         // the tag being captured ("h2", "hgroup"); empty: none
    std::string paragraph;
    bool inParagraph = false;

    auto endParagraph = [&] {
        if (inParagraph && inChapter() && !chapters.empty()) {
            const std::string text = tidy(paragraph);
            if (!text.empty()) chapters.back().paragraphs.push_back(text);
        }
        paragraph.clear();
        inParagraph = false;
    };

    Skipper pageNumbers;
    for (const auto& t : tokens) {
        if (pageNumbers.skip(t)) continue;
        if (t.kind == Token::Kind::Text) {
            if (count(Mark::Skip) > 0) continue;
            if (!headingTag.empty()) heading += t.text;
            else if (inParagraph) paragraph += t.text;
            continue;
        }
        const bool start = t.kind == Token::Kind::StartTag;
        if (start && t.name == "br") {
            if (inParagraph) paragraph += "\n";
            continue;
        }
        if (start && closesParagraph(t.name)) endParagraph();
        if (!start && t.name == "p") endParagraph();

        if (isContainer(t.name)) {
            if (start) {
                const std::string id = t.attribute("id");
                const Mark mark = (id == "pg-header" || id == "pg-footer") ? Mark::Skip
                                : (sectionMode && chapterSection(t))     ? Mark::Chapter
                                                                         : Mark::None;
                if (mark == Mark::Chapter && count(Mark::Skip) == 0) chapters.push_back({});
                stack.push_back({t.name, mark});
            } else {
                for (std::size_t k = stack.size(); k-- > 0;)
                    if (stack[k].first == t.name) {
                        stack.resize(k);
                        break;
                    }
            }
            continue;
        }
        if (count(Mark::Skip) > 0) continue;
        if (start && t.name == "p" && !headingTag.empty()) heading += " ";   // a subtitle in an <hgroup>

        if (isHeading(t.name) || t.name == "hgroup") {
            if (start && headingTag.empty()) {
                headingTag = t.name;
                heading.clear();
            } else if (start) {
                heading += " ";                     // a heading inside an <hgroup>
            } else if (t.name == headingTag) {
                const std::string title = headingTitle(heading);
                if (!sectionMode && headingTag == splitHeading) {
                    chapters.push_back({title, {}});
                    headingChapterOpen = true;
                } else if (inChapter() && !chapters.empty() && chapters.back().title.empty() &&
                           chapters.back().paragraphs.empty()) {
                    chapters.back().title = title;
                }
                headingTag.clear();
            }
            continue;
        }
        if (start && t.name == "p" && headingTag.empty()) {
            inParagraph = true;
            paragraph.clear();
        }
    }
    endParagraph();

    std::erase_if(chapters, [](const BookChapter& c) { return c.paragraphs.empty(); });
    for (std::size_t i = 0; i < chapters.size(); ++i)
        if (chapters[i].title.empty()) chapters[i].title = "Part " + std::to_string(i + 1);
    return chapters;
}

ChapterText pageText(std::string_view html, const std::vector<Token>& tokens)
{
    const std::set<std::string> hiddenSet = hiddenClasses(html);

    struct Block {
        std::vector<std::string> paragraphs;
        std::size_t chars = 0;
        std::string loose;                 // text outside <p>, split at <br>
    };
    std::map<std::size_t, Block> blocks;   // by the container's token index
    std::vector<std::size_t> stack;        // open containers (token indexes)
    std::vector<std::size_t> hiddenStack;  // open containers that are hidden
    std::string paragraph;
    bool inParagraph = false;
    bool paragraphHidden = false;
    std::size_t paragraphOwner = 0;
    std::string firstH1, firstH2, titleTag;
    std::string heading;
    std::string headingName;
    bool inTitle = false;

    auto owner = [&]() -> std::size_t { return stack.empty() ? std::size_t(-1) : stack.back(); };
    auto endParagraph = [&] {
        if (inParagraph && !paragraphHidden) {
            const std::string text = tidy(paragraph);
            if (!text.empty()) {
                auto& b = blocks[paragraphOwner];
                b.paragraphs.push_back(text);
                b.chars += text.size();
            }
        }
        inParagraph = false;
        paragraph.clear();
    };

    Skipper pageNumbers;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        if (pageNumbers.skip(t)) continue;
        if (t.kind == Token::Kind::Text) {
            if (inTitle) titleTag += t.text;
            else if (!headingName.empty()) heading += t.text;
            else if (inParagraph) paragraph += t.text;
            else if (!stack.empty() && hiddenStack.empty()) blocks[owner()].loose += t.text;
            continue;
        }
        const bool start = t.kind == Token::Kind::StartTag;
        if (start && t.name == "title") { inTitle = true; continue; }
        if (!start && t.name == "title") { inTitle = false; continue; }
        if (start && t.name == "br") {
            if (inParagraph) paragraph += "\n";
            else if (!stack.empty()) blocks[owner()].loose += "\n";
            continue;
        }
        if (start && closesParagraph(t.name)) endParagraph();
        if (!start && t.name == "p") endParagraph();

        if (start && (t.name == "h1" || t.name == "h2")) {
            headingName = t.name;
            heading.clear();
            continue;
        }
        if (!start && !headingName.empty() && t.name == headingName) {
            std::string& slot = headingName == "h1" ? firstH1 : firstH2;
            if (slot.empty()) slot = tidy(heading);
            headingName.clear();
            continue;
        }
        if (start && isContainer(t.name)) {
            stack.push_back(i);
            if (hidden(t, hiddenSet)) hiddenStack.push_back(i);
        } else if (!start && isContainer(t.name)) {
            for (std::size_t k = stack.size(); k-- > 0;) {
                if (tokens[stack[k]].name != t.name) continue;
                const std::size_t closed = stack[k];
                stack.resize(k);
                while (!hiddenStack.empty() && hiddenStack.back() >= closed) hiddenStack.pop_back();
                break;
            }
        }
        if (start && t.name == "p") {
            inParagraph = true;
            paragraph.clear();
            paragraphOwner = owner();
            paragraphHidden = !hiddenStack.empty() || hidden(t, hiddenSet);
        }
    }
    endParagraph();

    ChapterText out;
    out.title = !firstH1.empty() ? firstH1 : !firstH2.empty() ? firstH2 : tidy(titleTag);

    const Block* best = nullptr;
    for (const auto& [index, b] : blocks)
        if (!b.paragraphs.empty() && (!best || b.chars > best->chars)) best = &b;
    if (best && best->chars >= 200) {
        out.paragraphs = best->paragraphs;
        return out;
    }
    // No paragraph markup: the block with the most text between <br>s.
    std::size_t bestLoose = 0;
    const Block* looseBlock = nullptr;
    for (const auto& [index, b] : blocks) {
        const std::size_t size = tidy(b.loose).size();
        if (size > bestLoose) {
            bestLoose = size;
            looseBlock = &b;
        }
    }
    if (looseBlock && bestLoose >= (best ? best->chars : 0)) {
        const std::string text = tidy(looseBlock->loose);
        std::size_t pos = 0;
        while (pos <= text.size()) {
            const std::size_t end = text.find('\n', pos);
            const std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            if (!line.empty()) out.paragraphs.push_back(line);
            if (end == std::string::npos) break;
            pos = end + 1;
        }
    } else if (best) {
        out.paragraphs = best->paragraphs;
    }
    return out;
}

} // namespace sources
