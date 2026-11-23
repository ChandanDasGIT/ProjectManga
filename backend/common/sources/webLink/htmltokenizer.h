#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A forgiving HTML reader: turns a page into start tags (with attributes), end tags and text, the
// way it comes, without building a tree. Unclosed tags, missing quotes and stray '<' are fine.
// Comments are dropped, and so is what is inside <script> and <style>.
namespace weblink {

struct Token {
    enum class Kind { StartTag, EndTag, Text };
    Kind kind = Kind::Text;
    std::string name;                                          // tags: lower case
    std::vector<std::pair<std::string, std::string>> attributes;   // names lower case, values decoded
    std::string text;                                          // text: decoded

    // The attribute's value; empty when missing.
    std::string attribute(std::string_view name) const;
    bool hasAttribute(std::string_view name) const;
};

std::vector<Token> tokenize(std::string_view html);

// &amp; &lt; &#39; &#x27; &nbsp; and the common named ones; unknown ones stay as written.
std::string decodeEntities(std::string_view text);

} // namespace weblink
