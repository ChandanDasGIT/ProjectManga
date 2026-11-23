#include "testkit.h"

#include "libraryView/resume.h"

namespace {

library::Entry tenChapters()
{
    library::Entry e;
    e.sourceId = "web";
    e.mangaId = "a";
    e.chapterNumbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    return e;
}

} // namespace

TEST(resume, started)
{
    auto e = tenChapters();
    CHECK(!libraryview::started(e));
    CHECK_EQ(libraryview::bookProgress(e), -1.0);
    e.readIds = {"prologue"};
    CHECK(libraryview::started(e));
}

TEST(resume, progress_through_the_book)
{
    auto e = tenChapters();
    e.positionChapterId = "c6";
    e.positionNumber = 6;
    e.positionFraction = 0.5;
    e.lastReadNumber = 5;
    CHECK_EQ(libraryview::bookProgress(e), 0.55);              // five chapters and half of the sixth

    e.positionFraction = 1.0;                                  // the sixth finished
    e.lastReadNumber = 6;
    CHECK_EQ(libraryview::bookProgress(e), 0.6);

    auto old = tenChapters();                                  // read before places were kept
    old.lastReadNumber = 3;
    CHECK_EQ(libraryview::bookProgress(old), 0.3);

    old.chapterNumbers.push_back(11);                          // a new chapter came out
    old.chapterNumbers.push_back(12);
    CHECK_EQ(libraryview::bookProgress(old), 0.25);

    auto unnumbered = tenChapters();
    unnumbered.chapterNumbers.clear();
    unnumbered.readIds = {"x"};
    CHECK_EQ(libraryview::bookProgress(unnumbered), -1.0);
}
