#include "appCoreWrap.h"

#include "common/library/librarystore.h"
#include "common/sources/gutenberg/gutenbergsource.h"
#include "common/sources/mangaDex/mangadexsource.h"
#include "common/sources/standardEbooks/standardebookssource.h"
#include "common/sources/webLink/weblinksource.h"

#include <QDateTime>
#include <QFile>
#include <QJSEngine>
#include <QQmlEngine>

namespace {
AppCoreWrap* s_instance = nullptr;
}

AppCoreWrap::AppCoreWrap(const QString& dataFolder, const QString& cacheFolder, QObject* parent)
    : QObject(parent),
      m_checker(m_library, m_sources, m_scheduler),
      m_siteAdder(m_library, m_sources, m_scheduler),
      m_serverSync(m_library, m_http, m_scheduler),
      m_lanSync(m_library, m_lanNetwork, m_scheduler),
      m_libraryPath(dataFolder + "/library.json"),
      m_settingsPath(dataFolder + "/settings.json"),
      m_settings(settings::load(m_settingsPath.toStdString()))
{
    s_instance = this;
    m_sources.add(std::make_unique<mangadex::MangaDexSource>(m_http, m_scheduler));
    m_sources.add(std::make_unique<weblink::WebLinkSource>(m_http, m_scheduler));
    m_sources.add(std::make_unique<standardebooks::StandardEbooksSource>(m_http, m_scheduler));
    m_sources.add(std::make_unique<gutenberg::GutenbergSource>(
        m_http, m_scheduler, ((cacheFolder.isEmpty() ? dataFolder + "/cache" : cacheFolder) + "/gutenberg").toStdString()));
    m_sources.add(std::make_unique<weblink::WebNovelSource>(m_http, m_scheduler));

    loadLibrary();
    m_library.onChanged = [this] {
        if (m_loading) return;
        saveLibrary();
        m_checker.reschedule();
        m_serverSync.libraryChanged();
        emit libraryChanged();
    };

    if (m_settings.deviceId.empty()) {
        m_settings.deviceId = settings::newDeviceId();
        saveSettings();
    }

    m_serverSync.onStatusChanged = [this] { emit syncChanged(); };
    m_serverSync.onKnownWipedAtChanged = [this](std::int64_t wipedAt) {
        m_settings.serverWipedAt = wipedAt;
        saveSettings();
    };
    m_lanSync.onStatusChanged = [this] { emit syncChanged(); };
#if defined(PROJECTMANGA_SYNC_SERVER) && defined(PROJECTMANGA_SYNC_KEY)
    setSyncServer(QStringLiteral(PROJECTMANGA_SYNC_SERVER), QStringLiteral(PROJECTMANGA_SYNC_KEY));
#endif
}

AppCoreWrap::~AppCoreWrap()
{
    m_library.onChanged = nullptr;
    m_serverSync.onStatusChanged = nullptr;
    m_serverSync.onKnownWipedAtChanged = nullptr;
    m_lanSync.onStatusChanged = nullptr;
    if (s_instance == this) s_instance = nullptr;
}

AppCoreWrap* AppCoreWrap::instance()
{
    return s_instance;
}

AppCoreWrap* AppCoreWrap::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void AppCoreWrap::start()
{
    m_checker.start();
}

void AppCoreWrap::loadLibrary()
{
    auto loaded = library::load(m_libraryPath.toStdString());
    if (!loaded.ok()) {
        // Keep the damaged file next to the new one instead of overwriting it.
        const QString kept = m_libraryPath + ".damaged-" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QFile::rename(m_libraryPath, kept);
        m_libraryProblem = QString::fromStdString(loaded.error) + ". It was kept as " + kept + ".";
    }
    m_loading = true;
    m_library.replaceAll(std::move(loaded.entries), std::move(loaded.removals));
    m_loading = false;
}

void AppCoreWrap::saveLibrary()
{
    const std::string error = library::save(m_library.entries(), m_library.removals(), m_libraryPath.toStdString());
    m_libraryProblem = error.empty() ? QString() : "The library could not be saved: " + QString::fromStdString(error);
}

void AppCoreWrap::setRating(const QString& name, bool on)
{
    const settings::Settings before = m_settings;
    if (!m_settings.setRating(name.toStdString(), on)) return;
    if (m_settings.contentRatings() == before.contentRatings()) {
        emit settingsChanged();                // a refused change: the chips show the old state again
        return;
    }
    saveSettings();
    emit settingsChanged();
}

void AppCoreWrap::setLibrarySort(const std::string& key)
{
    if (m_settings.librarySort == key) return;
    m_settings.librarySort = key;
    saveSettings();
    emit settingsChanged();
}

void AppCoreWrap::setLibraryKind(const std::string& key)
{
    if (m_settings.libraryKind == key) return;
    m_settings.libraryKind = key;
    saveSettings();
    emit settingsChanged();
}

void AppCoreWrap::updateSettings(const settings::Settings& next)
{
    if (next.readerFontSize == m_settings.readerFontSize && next.readerLineSpacing == m_settings.readerLineSpacing &&
        next.readerWidth == m_settings.readerWidth && next.readerTheme == m_settings.readerTheme &&
        next.mangaReaderMode == m_settings.mangaReaderMode && next.chapterOrder == m_settings.chapterOrder &&
        next.flipSpeed == m_settings.flipSpeed)
        return;
    m_settings = next;
    saveSettings();
    emit settingsChanged();
}

void AppCoreWrap::setReaderFontSize(int size)
{
    auto next = m_settings;
    next.setReaderFontSize(size);
    updateSettings(next);
}

void AppCoreWrap::setReaderLineSpacing(double spacing)
{
    auto next = m_settings;
    next.setReaderLineSpacing(spacing);
    updateSettings(next);
}

void AppCoreWrap::setReaderWidth(int width)
{
    auto next = m_settings;
    next.setReaderWidth(width);
    updateSettings(next);
}

void AppCoreWrap::setReaderTheme(const QString& theme)
{
    auto next = m_settings;
    next.setReaderTheme(theme.toStdString());
    updateSettings(next);
}

void AppCoreWrap::setMangaReaderMode(const QString& mode)
{
    auto next = m_settings;
    next.setMangaReaderMode(mode.toStdString());
    updateSettings(next);
}

void AppCoreWrap::setChapterOrder(const QString& order)
{
    auto next = m_settings;
    next.setChapterOrder(order.toStdString());
    updateSettings(next);
}

void AppCoreWrap::setFlipSpeed(const QString& speed)
{
    auto next = m_settings;
    next.setFlipSpeed(speed.toStdString());
    updateSettings(next);
}

void AppCoreWrap::setSyncServer(const QString& address, const QString& key)
{
    m_syncAddress = address.trimmed();
    m_syncKey = key;
    if (m_syncAddress.isEmpty() || !m_settings.syncEnabled) {
        m_serverSync.stop();
        m_lanSync.stop();
    } else {
        m_serverSync.start(m_syncAddress.toStdString(), m_syncKey.toStdString(), m_settings.deviceId,
                           m_settings.serverWipedAt);
        m_lanSync.start(m_syncKey.toStdString(), m_settings.deviceId);
    }
    emit syncChanged();
}

void AppCoreWrap::syncNow()
{
    m_deleteProblem.clear();
    m_serverSync.syncNow();
    emit syncChanged();
}

void AppCoreWrap::setSyncEnabled(bool enabled)
{
    m_deleteProblem.clear();
    if (enabled) m_serverDataDeleted = false;
    if (m_settings.syncEnabled != enabled) {
        m_settings.syncEnabled = enabled;
        saveSettings();
    }
    setSyncServer(m_syncAddress, m_syncKey);
}

void AppCoreWrap::deleteServerData()
{
    m_deleteProblem.clear();
    m_serverSync.deleteAll([this](const std::string& problem) {
        if (!problem.empty()) {
            m_deleteProblem = QString::fromStdString(problem);
        } else {
            m_serverDataDeleted = true;
            m_settings.syncEnabled = false;
            saveSettings();
            m_lanSync.stop();
        }
        emit syncChanged();
    });
    emit syncChanged();
}

void AppCoreWrap::answerWipe(bool putBack)
{
    m_serverSync.answerWipe(putBack);
    if (!putBack) {
        m_settings.syncEnabled = false;
        saveSettings();
        m_lanSync.stop();
    }
    emit syncChanged();
}

void AppCoreWrap::applicationActiveChanged(bool)
{
    if (m_serverSync.running()) m_serverSync.syncNow();
}

QString AppCoreWrap::syncStatus() const
{
    if (m_syncAddress.isEmpty()) return "Sync isn't set up in this build.";
    if (!m_deleteProblem.isEmpty()) return "Your data could not be deleted: " + m_deleteProblem + ".";
    if (!m_settings.syncEnabled)
        return m_serverDataDeleted ? "Your data was deleted from the server. Sync is off on this device."
                                   : "Sync is off on this device.";
    const int nearby = m_lanSync.nearbyDevices();
    const QString wifi = !m_lanSync.error().empty() ? " " + QString::fromStdString(m_lanSync.error()) + "."
                       : nearby == 0               ? QString()
                       : nearby == 1               ? QStringLiteral(" Syncing over Wi-Fi with 1 device.")
                                                   : " Syncing over Wi-Fi with " + QString::number(nearby) + " devices.";
    return serverStatus() + wifi;
}

QString AppCoreWrap::serverStatus() const
{
    if (m_serverSync.wipeQuestion())
        return "Your data was deleted from the server on another device. Put this device's library back on the "
               "server?";
    const QString when = m_serverSync.lastSyncMs() > 0
        ? QDateTime::fromMSecsSinceEpoch(m_serverSync.lastSyncMs()).toString("HH:mm")
        : QString();
    if (!m_serverSync.error().empty())
        return QString::fromStdString(m_serverSync.error()) + "." +
               (when.isEmpty() ? QString() : " Last synced at " + when + ".");
    if (when.isEmpty()) return m_serverSync.busy() ? "Syncing…" : "Not synced yet.";
    const int others = m_serverSync.otherDevices();
    const QString devices = others == 0 ? "no other device has synced yet"
                          : others == 1 ? "1 other device"
                                        : QString::number(others) + " other devices";
    return "Last synced at " + when + " · " + devices + ".";
}

bool AppCoreWrap::isNovelSource(const QString& sourceId) const
{
    const sources::Source* s = m_sources.find(sourceId.toStdString());
    return s && s->kind() == "novel";
}

void AppCoreWrap::saveSettings()
{
    const std::string error = settings::save(m_settings, m_settingsPath.toStdString());
    if (!error.empty()) {
        m_libraryProblem = "Settings could not be saved: " + QString::fromStdString(error);
        emit libraryChanged();
    }
}

void AppCoreWrap::rememberChapters(const std::string& sourceId, const std::string& mangaId,
                                   std::vector<sources::Chapter> chapters)
{
    m_chapters[{sourceId, mangaId}] = std::move(chapters);
}

const std::vector<sources::Chapter>* AppCoreWrap::chaptersOf(const std::string& sourceId, const std::string& mangaId) const
{
    const auto it = m_chapters.find({sourceId, mangaId});
    return it == m_chapters.end() ? nullptr : &it->second;
}
