#include "common/library/siteadder.h"

#include "common/sources/webLink/pageguess.h"

#include <cctype>

namespace library {

namespace {
std::string trimmed(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
} // namespace

SiteAdder::SiteAdder(Library& library, const sources::SourceRegistry& sources, platform::Scheduler& scheduler)
    : m_library(library), m_sources(sources), m_scheduler(scheduler)
{
}

std::string SiteAdder::check(const std::string& sourceId, const std::string& name, const std::string& address) const
{
    if (trimmed(name).empty()) return "Give the site a name.";
    const std::string url = trimmed(address);
    if (!weblink::isWebAddress(url)) return "Enter the full address, starting with http:// or https://";
    if (m_library.contains(sourceId, url)) return "This address is already in your library.";
    if (!m_sources.find(sourceId)) return "Unknown source \"" + sourceId + "\".";
    return {};
}

void SiteAdder::add(const std::string& sourceId, const std::string& name, const std::string& address,
                    std::function<void(std::string error, std::vector<sources::Chapter> chapters)> done)
{
    if (const std::string why = check(sourceId, name, address); !why.empty()) return done(why, {});
    const std::string title = trimmed(name);
    const std::string url = trimmed(address);
    sources::Source* source = m_sources.find(sourceId);
    auto alive = m_lifetime.watch();
    source->chapters(url, [this, alive, source, sourceId, title, url, done = std::move(done)](
                              sources::Result<std::vector<sources::Chapter>> chapters) mutable {
        if (alive.expired()) return;
        if (!chapters.ok()) return done(chapters.error + ".", {});
        source->details(url, [this, alive, sourceId, title, url, list = std::move(chapters.value),
                              done = std::move(done)](sources::Result<sources::MangaDetails> details) mutable {
            if (alive.expired()) return;
            Entry e;
            e.sourceId = sourceId;
            e.mangaId = url;
            e.title = title;
            e.coverUrl = details.ok() ? details.value.coverUrl : std::string();
            e.addedMs = m_scheduler.nowMs();
            m_library.add(std::move(e));
            m_library.applyChapters(sourceId, url, list, m_scheduler.nowMs());
            done({}, std::move(list));
        });
    });
}

} // namespace library
