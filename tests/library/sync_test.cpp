#include "testkit.h"

#include "common/library/sync.h"

namespace {

library::Entry manga(const std::string& id, std::int64_t addedMs)
{
    library::Entry e;
    e.sourceId = "mangadex";
    e.mangaId = id;
    e.title = "Title " + id;
    e.addedMs = addedMs;
    return e;
}

} // namespace

TEST(sync, read_chapters_add_up_and_the_newest_place_wins)
{
    library::Entry mine = manga("a", 100);
    mine.readNumbers = {1, 2};
    mine.lastReadNumber = 2;
    mine.positionChapterId = "c3";
    mine.positionFraction = 0.5;
    mine.positionMs = 1000;
    library::Entry theirs = manga("a", 50);
    theirs.readNumbers = {3};
    theirs.lastReadNumber = 3;
    theirs.positionChapterId = "c4";
    theirs.positionFraction = 0.25;
    theirs.positionMs = 2000;
    theirs.seenLatest = 7;

    library::Snapshot a{{mine}, {}};
    CHECK(library::mergeInto(a, library::Snapshot{{theirs}, {}}));
    const library::Entry& m = a.entries.at(0);
    CHECK(m.readNumbers == std::vector<double>({1, 2, 3}));
    CHECK_EQ(m.lastReadNumber, 3.0);
    CHECK_EQ(m.positionChapterId, std::string("c4"));
    CHECK_EQ(m.positionFraction, 0.25);
    CHECK_EQ(m.seenLatest, 7.0);
    CHECK_EQ(m.addedMs, std::int64_t(50));                     // added first on the other device

    // An older place does not win, and the same copy again changes nothing.
    library::Entry older = theirs;
    older.positionChapterId = "c1";
    older.positionMs = 10;
    CHECK(!library::mergeInto(a, library::Snapshot{{theirs}, {}}));
    library::mergeInto(a, library::Snapshot{{older}, {}});
    CHECK_EQ(a.entries.at(0).positionChapterId, std::string("c4"));
}

TEST(sync, added_and_removed_on_another_device)
{
    library::Snapshot mine{{manga("kept", 100), manga("dropped", 100)}, {}};
    const library::Snapshot theirs{{manga("kept", 100), manga("new", 300)}, {{"mangadex", "dropped", 200}}};
    CHECK(library::mergeInto(mine, theirs));
    CHECK_EQ(mine.entries.size(), std::size_t(2));
    CHECK_EQ(mine.entries.at(0).mangaId, std::string("new"));  // newest first
    CHECK_EQ(mine.entries.at(1).mangaId, std::string("kept"));
    CHECK_EQ(mine.removals.size(), std::size_t(1));

    // Added again after the removal: it stays.
    library::Snapshot again{{manga("dropped", 500)}, {}};
    library::mergeInto(again, theirs);
    CHECK_EQ(again.entries.size(), std::size_t(3));

    // Removed here; an older copy elsewhere does not bring it back.
    library::Snapshot here{{}, {{"mangadex", "new", 400}}};
    library::mergeInto(here, theirs);
    CHECK_EQ(here.entries.size(), std::size_t(1));
    CHECK_EQ(here.entries.at(0).mangaId, std::string("kept"));
}
