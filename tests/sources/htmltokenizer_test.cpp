#include "testkit.h"

#include "common/sources/webLink/htmltokenizer.h"

using weblink::Token;

TEST(htmltokenizer, tags_attributes_and_text)
{
    const auto t = weblink::tokenize(R"(<A HREF="/x?a=1&amp;b=2" class=big data-id='7'>Hi &amp; bye</a>)");
    CHECK_EQ(t.size(), std::size_t(3));
    CHECK(t[0].kind == Token::Kind::StartTag);
    CHECK_EQ(t[0].name, std::string("a"));
    CHECK_EQ(t[0].attribute("href"), std::string("/x?a=1&b=2"));
    CHECK_EQ(t[0].attribute("class"), std::string("big"));
    CHECK_EQ(t[0].attribute("data-id"), std::string("7"));
    CHECK_EQ(t[1].text, std::string("Hi & bye"));
    CHECK(t[2].kind == Token::Kind::EndTag);
    CHECK_EQ(t[2].name, std::string("a"));
}

TEST(htmltokenizer, self_closing_and_boolean_attributes)
{
    const auto t = weblink::tokenize(R"(<img src="a.jpg" loading=lazy alt = "x y"/><input disabled>)");
    CHECK_EQ(t.size(), std::size_t(2));
    CHECK_EQ(t[0].attribute("src"), std::string("a.jpg"));
    CHECK_EQ(t[0].attribute("alt"), std::string("x y"));
    CHECK(t[1].hasAttribute("disabled"));
    CHECK(!t[1].hasAttribute("src"));
}

TEST(htmltokenizer, drops_comments_scripts_and_styles)
{
    const auto t = weblink::tokenize(
        "<!DOCTYPE html><!-- <a href='no'> --><script>var s = '<a href=\"no\">';</SCRIPT>"
        "<style>a{}</style ><p>yes</p>");
    std::string names;
    for (const auto& token : t) names += (token.kind == Token::Kind::Text ? "#" + token.text : token.name) + ",";
    CHECK_EQ(names, std::string("script,script,style,style,p,#yes,p,"));
}

TEST(htmltokenizer, forgives_broken_markup)
{
    const auto t = weblink::tokenize("a < b <li>one<li>two <a href=\"unterminated>end");
    CHECK_EQ(t[0].text, std::string("a < b "));
    CHECK_EQ(t[1].name, std::string("li"));
    CHECK_EQ(t[4].text, std::string("two "));
    CHECK_EQ(t[5].name, std::string("a"));
    CHECK_EQ(t[5].attribute("href"), std::string("unterminated>end"));
}

TEST(htmltokenizer, entities)
{
    CHECK_EQ(weblink::decodeEntities("&lt;&#39;&#x27;&quot;&gt; &hellip; &unknown; & alone"),
             std::string("<''\"> \xE2\x80\xA6 &unknown; & alone"));
    CHECK_EQ(weblink::decodeEntities("&#0;&#xD800;"), std::string("\xEF\xBF\xBD\xEF\xBF\xBD"));
}
