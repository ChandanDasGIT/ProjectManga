#pragma once

#include "common/library/library.h"
#include "common/platform/httpclient.h"
#include "common/platform/scheduler.h"
#include "common/sources/sourceregistry.h"

#include <functional>
#include <string>
#include <vector>

// Adding a site the user names by its address ("web" for manga, "webnovel" for novels): the page
// must list chapters; it is then saved with the user's name, the page's cover and its chapters.
namespace library {

class SiteAdder {
public:
    SiteAdder(Library& library, const sources::SourceRegistry& sources, platform::Scheduler& scheduler);

    // Why it cannot be added before asking the site (empty: fine to try).
    std::string check(const std::string& sourceId, const std::string& name, const std::string& address) const;
    // `done` gets an empty error and the chapters when the site was added, else why not.
    void add(const std::string& sourceId, const std::string& name, const std::string& address,
             std::function<void(std::string error, std::vector<sources::Chapter> chapters)> done);

private:
    Library& m_library;
    const sources::SourceRegistry& m_sources;
    platform::Scheduler& m_scheduler;
    platform::Lifetime m_lifetime;
};

} // namespace library
