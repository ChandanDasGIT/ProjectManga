#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The user's choices that outlive a run: which content ratings Discover and search show, and how
// the library is sorted. Kept as one small JSON file next to the library.
namespace settings {

struct Settings {
    bool safe = true;
    bool suggestive = false;
    bool erotica = false;
    std::string librarySort = "added";
    std::string libraryKind = "all";      // "all", "manga", "novel"

    // The novel reader.
    int readerFontSize = 18;            // 12 .. 40
    double readerLineSpacing = 1.5;     // 1.0 .. 2.5
    int readerWidth = 720;              // 480 .. 1400, the text column in pixels
    std::string readerTheme = "dark";   // "dark", "sepia", "light"

    std::string mangaReaderMode = "scroll";   // "scroll", "page" (one page fitted), "flip" (two-page book)
    std::string chapterOrder = "auto";        // "auto" (the source's own), "newest", "oldest"
    std::string flipSpeed = "normal";         // how fast a page turns: "slow", "normal", "fast", "veryFast"

    // This device's name among the devices the library syncs with, made once and never shared
    // between devices.
    std::string deviceId;
    // Off after this device's data was deleted from the sync server, until turned on again.
    bool syncEnabled = true;
    // When the sync server's data was last deleted, as this device last saw it (-1: not yet).
    std::int64_t serverWipedAt = -1;

    // Each keeps the value inside its range (an unknown theme: dark).
    void setReaderFontSize(int size);
    void setReaderLineSpacing(double spacing);
    void setReaderWidth(int width);
    void setReaderTheme(const std::string& theme);
    // Unknown values: "scroll" and "auto".
    void setMangaReaderMode(const std::string& mode);
    void setChapterOrder(const std::string& order);
    // Unknown: "normal".
    void setFlipSpeed(const std::string& speed);

    // The MangaDex ratings to ask for, e.g. {"safe", "erotica"}; never empty (then: safe).
    std::vector<std::string> contentRatings() const;
    // Turns one rating on or off; the last one left on stays on. False for an unknown name.
    bool setRating(const std::string& name, bool on);
    bool rating(const std::string& name) const;
};

// 16 random hex digits.
std::string newDeviceId();

// A missing or unreadable file gives the defaults.
Settings load(const std::string& path);
// Returns an empty string or why it failed.
std::string save(const Settings& settings, const std::string& path);

} // namespace settings
