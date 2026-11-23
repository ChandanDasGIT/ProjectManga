#include "testkit.h"
#include "fakes.h"

#include "discoverView/searchView/searchsession.h"

namespace {

// A source whose answers the test hands out by hand.
class ManualSource : public sources::Source {
public:
    std::vector<std::pair<std::string, std::function<void(sources::Result<sources::SearchPage>)>>> asked;

    std::string id() const override { return "manual"; }
    std::string name() const override { return "Manual"; }
    std::vector<std::string> lastRatings;
    void search(const std::string& query, int offset, const std::vector<std::string>& ratings,
                std::function<void(sources::Result<sources::SearchPage>)> done) override
    {
        lastRatings = ratings;
        asked.emplace_back(query + "@" + std::to_string(offset), std::move(done));
    }
    void details(const std::string&, std::function<void(sources::Result<sources::MangaDetails>)>) override {}
    void chapters(const std::string&, std::function<void(sources::Result<std::vector<sources::Chapter>>)>) override {}
    void pages(const sources::Chapter&, std::function<void(sources::Result<sources::PageList>)>) override {}
};

sources::Result<sources::SearchPage> page(std::vector<std::string> ids, int total, int nextOffset)
{
    sources::Result<sources::SearchPage> r;
    for (auto& id : ids) r.value.items.push_back({id, "Title " + id, "", ""});
    r.value.total = total;
    r.value.nextOffset = nextOffset;
    return r;
}

} // namespace

TEST(searchsession, search_and_load_more)
{
    ManualSource source;
    searchview::SearchSession session(source);
    int changes = 0;
    session.onChanged = [&] { ++changes; };

    session.search("  moon  ");
    CHECK_EQ(session.query(), std::string("moon"));
    CHECK(session.loading());
    CHECK_EQ(source.asked[0].first, std::string("moon@0"));
    source.asked[0].second(page({"a", "b"}, 3, 2));
    CHECK(!session.loading());
    CHECK_EQ(session.results().size(), std::size_t(2));
    CHECK(session.canLoadMore());

    session.loadMore();
    CHECK_EQ(source.asked[1].first, std::string("moon@2"));
    source.asked[1].second(page({"b", "c"}, 3, 4));          // "b" again: not doubled
    CHECK_EQ(session.results().size(), std::size_t(3));
    CHECK(!session.canLoadMore());
    CHECK(changes >= 4);
}

TEST(searchsession, a_new_search_drops_old_answers)
{
    ManualSource source;
    searchview::SearchSession session(source);
    session.search("old");
    session.search("new");
    source.asked[0].second(page({"stale"}, 1, 1));
    CHECK(session.results().empty());
    CHECK(session.loading());
    source.asked[1].second(page({"fresh"}, 1, 1));
    CHECK_EQ(session.results().size(), std::size_t(1));
    CHECK_EQ(session.results()[0].mangaId, std::string("fresh"));
}

TEST(searchsession, errors_and_empty_queries)
{
    ManualSource source;
    searchview::SearchSession session(source);
    session.search("x");
    source.asked[0].second(sources::Result<sources::SearchPage>::failure("offline"));
    CHECK_EQ(session.error(), std::string("offline"));
    CHECK(!session.canLoadMore());

    session.search("   ");
    CHECK_EQ(source.asked.size(), std::size_t(1));
    CHECK(session.query().empty());
    CHECK(session.error().empty());
}

TEST(searchsession, answers_after_the_session_is_gone_are_ignored)
{
    ManualSource source;
    {
        searchview::SearchSession session(source);
        session.search("x");
    }
    source.asked[0].second(page({"a"}, 1, 1));                 // must not touch the dead session
    CHECK(true);
}

TEST(searchsession, content_ratings_rerun_the_search)
{
    ManualSource source;
    searchview::SearchSession session(source);
    session.search("x");
    CHECK(source.lastRatings == std::vector<std::string>({"safe"}));
    session.setContentRatings({"safe", "erotica"});
    CHECK_EQ(source.asked.size(), std::size_t(2));
    CHECK(source.lastRatings == std::vector<std::string>({"safe", "erotica"}));
    session.setContentRatings({"safe", "erotica"});            // no change: no new request
    CHECK_EQ(source.asked.size(), std::size_t(2));
}
