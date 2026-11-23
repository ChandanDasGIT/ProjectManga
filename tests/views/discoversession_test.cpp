#include "testkit.h"

#include "discoverView/discoversession.h"

namespace {

// A source that records browse requests; the test answers them by hand.
class BrowseSource : public sources::Source {
public:
    struct Asked {
        sources::BrowseQuery query;
        int offset;
        std::function<void(sources::Result<sources::SearchPage>)> done;
    };
    std::vector<Asked> asked;
    std::function<void(sources::Result<std::vector<sources::Tag>>)> tagsAnswer;

    std::string id() const override { return "browse"; }
    std::string name() const override { return "Browse"; }
    void search(const std::string&, int, const std::vector<std::string>&,
                std::function<void(sources::Result<sources::SearchPage>)>) override {}
    void details(const std::string&, std::function<void(sources::Result<sources::MangaDetails>)>) override {}
    void chapters(const std::string&, std::function<void(sources::Result<std::vector<sources::Chapter>>)>) override {}
    void pages(const sources::Chapter&, std::function<void(sources::Result<sources::PageList>)>) override {}
    void browse(const sources::BrowseQuery& q, int offset, std::function<void(sources::Result<sources::SearchPage>)> done) override
    {
        asked.push_back({q, offset, std::move(done)});
    }
    void tags(std::function<void(sources::Result<std::vector<sources::Tag>>)> done) override { tagsAnswer = std::move(done); }

    // The latest request with this order.
    Asked* last(const std::string& order)
    {
        for (auto it = asked.rbegin(); it != asked.rend(); ++it)
            if (it->query.order == order && it->query.includedTags.empty() && it->query.demographics.empty()) return &*it;
        return nullptr;
    }
};

sources::Result<sources::SearchPage> page(std::vector<std::string> ids, int total = 100, int next = 24)
{
    sources::Result<sources::SearchPage> r;
    for (auto& id : ids) r.value.items.push_back({id, "Title " + id, "", ""});
    r.value.total = total;
    r.value.nextOffset = next;
    return r;
}

std::string keys(const std::vector<discoverview::Shelf>& shelves)
{
    std::string out;
    for (const auto& s : shelves) out += (out.empty() ? "" : ",") + s.key;
    return out;
}

} // namespace

TEST(discoversession, loads_the_shelves_and_tags)
{
    BrowseSource source;
    discoverview::DiscoverSession session(source);
    session.load();
    CHECK_EQ(keys(session.shelves()), std::string("popular,updated,rated,new"));
    CHECK_EQ(source.asked.size(), std::size_t(4));
    CHECK(session.shelves()[0].loading);
    CHECK(source.asked[0].query.contentRatings == std::vector<std::string>({"safe"}));

    source.last("followedCount")->done(page({"a", "b"}));
    source.last("rating")->done(sources::Result<sources::SearchPage>::failure("busy"));
    CHECK_EQ(session.shelves()[0].items.size(), std::size_t(2));
    CHECK(!session.shelves()[0].loading);
    CHECK_EQ(session.shelves()[2].error, std::string("busy"));

    CHECK(session.tagsLoading());
    sources::Result<std::vector<sources::Tag>> tags;
    tags.value = {{"t1", "Action", "genre"}};
    source.tagsAnswer(tags);
    CHECK(!session.tagsLoading());
    CHECK_EQ(session.tags().size(), std::size_t(1));
}

TEST(discoversession, content_ratings_reload_everything)
{
    BrowseSource source;
    discoverview::DiscoverSession session(source);
    session.load();
    session.toggleTag("t1");
    const std::size_t before = source.asked.size();
    session.setContentRatings({"safe", "erotica"});
    CHECK_EQ(source.asked.size(), before + 5);                  // four shelves and the browse grid
    CHECK(source.asked.back().query.contentRatings == std::vector<std::string>({"safe", "erotica"}));
    session.setContentRatings({"safe", "erotica"});
    CHECK_EQ(source.asked.size(), before + 5);

    source.asked[0].done(page({"stale"}));                      // an answer for the old ratings
    CHECK(session.shelves()[0].items.empty());
}

TEST(discoversession, for_you_shelf)
{
    BrowseSource source;
    discoverview::DiscoverSession session(source);
    session.load();
    session.setForYou({"t1", "t2"}, {"owned"});
    CHECK_EQ(keys(session.shelves()), std::string("foryou,popular,updated,rated,new"));
    const auto& q = source.asked.back().query;
    CHECK(q.includedTags == std::vector<std::string>({"t1", "t2"}));
    CHECK(!q.anyTag);
    source.asked.back().done(page({"owned", "fresh"}));
    CHECK_EQ(session.shelves()[0].items.size(), std::size_t(1));
    CHECK_EQ(session.shelves()[0].items[0].mangaId, std::string("fresh"));

    const std::size_t before = source.asked.size();
    session.setForYou({"t1", "t2"}, {"owned", "fresh"});        // added to the library: dropped, no request
    CHECK(session.shelves()[0].items.empty());
    CHECK_EQ(source.asked.size(), before);

    session.setForYou({}, {});
    CHECK_EQ(keys(session.shelves()), std::string("popular,updated,rated,new"));
}

TEST(discoversession, browsing_by_category)
{
    BrowseSource source;
    discoverview::DiscoverSession session(source);
    CHECK(!session.browsing());
    session.toggleTag("t1");
    session.toggleDemographic("seinen");
    session.toggleDemographic("not-a-demographic");
    CHECK(session.browsing());
    CHECK(session.tagSelected("t1"));
    CHECK(session.demographicSelected("seinen"));
    auto& latest = source.asked.back();
    CHECK(latest.query.includedTags == std::vector<std::string>({"t1"}));
    CHECK(latest.query.demographics == std::vector<std::string>({"seinen"}));
    CHECK(!latest.query.anyTag);
    latest.done(page({"a", "b"}, 30, 24));
    CHECK_EQ(session.results().size(), std::size_t(2));
    CHECK(session.canLoadMore());

    session.loadMore();
    CHECK_EQ(source.asked.back().offset, 24);
    source.asked.back().done(page({"b", "c"}, 30, 30));
    CHECK_EQ(session.results().size(), std::size_t(3));
    CHECK(!session.canLoadMore());

    session.toggleTag("t1");
    session.clearBrowse();
    CHECK(!session.browsing());
    CHECK(session.results().empty());
}
