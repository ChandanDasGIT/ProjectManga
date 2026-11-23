#pragma once

#include "common/library/library.h"
#include "common/library/siteadder.h"
#include "common/library/updatechecker.h"
#include "common/sync/lansync.h"
#include "common/sync/serversync.h"
#include "common/settings.h"
#include "common/sources/sourceregistry.h"
#include "httpClientWrap.h"
#include "lanNetworkWrap.h"
#include "schedulerWrap.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class QQmlEngine;
class QJSEngine;

// What every view shares: the network, the sources, the library (kept saved in
// <dataFolder>/library.json), the settings (<dataFolder>/settings.json) and the update checker.
// One exists while the app runs; the view wrappers reach it through instance(), QML as the
// AppCoreWrap singleton.
class AppCoreWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int newChapterCount READ newChapterCount NOTIFY libraryChanged)
    Q_PROPERTY(QString libraryProblem READ libraryProblem NOTIFY libraryChanged)
    Q_PROPERTY(bool showSafe READ showSafe NOTIFY settingsChanged)
    Q_PROPERTY(bool showSuggestive READ showSuggestive NOTIFY settingsChanged)
    Q_PROPERTY(bool showErotica READ showErotica NOTIFY settingsChanged)
    Q_PROPERTY(int readerFontSize READ readerFontSize WRITE setReaderFontSize NOTIFY settingsChanged)
    Q_PROPERTY(double readerLineSpacing READ readerLineSpacing WRITE setReaderLineSpacing NOTIFY settingsChanged)
    Q_PROPERTY(int readerWidth READ readerWidth WRITE setReaderWidth NOTIFY settingsChanged)
    Q_PROPERTY(QString readerTheme READ readerTheme WRITE setReaderTheme NOTIFY settingsChanged)
    Q_PROPERTY(QString mangaReaderMode READ mangaReaderMode WRITE setMangaReaderMode NOTIFY settingsChanged)
    Q_PROPERTY(QString chapterOrder READ chapterOrder WRITE setChapterOrder NOTIFY settingsChanged)
    Q_PROPERTY(QString flipSpeed READ flipSpeed WRITE setFlipSpeed NOTIFY settingsChanged)
    // Sync through the server built into this build (none: syncConfigured is false), and directly
    // with the user's devices on the same Wi-Fi.
    Q_PROPERTY(bool syncConfigured READ syncConfigured NOTIFY syncChanged)
    Q_PROPERTY(bool syncEnabled READ syncEnabled NOTIFY syncChanged)
    Q_PROPERTY(bool syncBusy READ syncBusy NOTIFY syncChanged)
    // The server's data was deleted (on another device): sync waits for answerWipe().
    Q_PROPERTY(bool syncWipeQuestion READ syncWipeQuestion NOTIFY syncChanged)
    // How the last sync went, in a sentence or two; syncOk is false when something failed.
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY syncChanged)
    Q_PROPERTY(bool syncOk READ syncOk NOTIFY syncChanged)

public:
    // cacheFolder: kept downloads such as Gutenberg's catalogue (empty: <dataFolder>/cache).
    explicit AppCoreWrap(const QString& dataFolder, const QString& cacheFolder = QString(), QObject* parent = nullptr);
    ~AppCoreWrap() override;

    static AppCoreWrap* instance();
    static AppCoreWrap* create(QQmlEngine*, QJSEngine*);

    // Starts the update checks (the app calls this once the window is up).
    void start();

    library::Library& library() { return m_library; }
    const sources::SourceRegistry& sources() const { return m_sources; }
    platform::Scheduler& scheduler() { return m_scheduler; }

    // The chapter list the details view loaded last, for the reader.
    void rememberChapters(const std::string& sourceId, const std::string& mangaId,
                          std::vector<sources::Chapter> chapters);
    const std::vector<sources::Chapter>* chaptersOf(const std::string& sourceId, const std::string& mangaId) const;

    const settings::Settings& settings() const { return m_settings; }
    bool showSafe() const { return m_settings.safe; }
    bool showSuggestive() const { return m_settings.suggestive; }
    bool showErotica() const { return m_settings.erotica; }
    // "safe", "suggestive" or "erotica"; the last one on stays on.
    Q_INVOKABLE void setRating(const QString& name, bool on);
    void setLibrarySort(const std::string& key);
    void setLibraryKind(const std::string& key);

    int readerFontSize() const { return m_settings.readerFontSize; }
    double readerLineSpacing() const { return m_settings.readerLineSpacing; }
    int readerWidth() const { return m_settings.readerWidth; }
    QString readerTheme() const { return QString::fromStdString(m_settings.readerTheme); }
    void setReaderFontSize(int size);
    void setReaderLineSpacing(double spacing);
    void setReaderWidth(int width);
    void setReaderTheme(const QString& theme);
    QString mangaReaderMode() const { return QString::fromStdString(m_settings.mangaReaderMode); }
    QString chapterOrder() const { return QString::fromStdString(m_settings.chapterOrder); }
    void setMangaReaderMode(const QString& mode);
    void setChapterOrder(const QString& order);
    QString flipSpeed() const { return QString::fromStdString(m_settings.flipSpeed); }
    void setFlipSpeed(const QString& speed);

    // The sync server's address and key (the build's own, or a test server's); empty: no sync.
    void setSyncServer(const QString& address, const QString& key);
    bool syncConfigured() const { return !m_syncAddress.isEmpty(); }
    bool syncEnabled() const { return m_settings.syncEnabled; }
    bool syncBusy() const { return m_serverSync.busy(); }
    bool syncWipeQuestion() const { return m_serverSync.wipeQuestion(); }
    // putBack: upload this device's data to the server again; otherwise sync turns off here.
    Q_INVOKABLE void answerWipe(bool putBack);
    QString syncStatus() const;
    bool syncOk() const
    {
        return (m_serverSync.error().empty() || m_serverSync.unreachable()) && m_deleteProblem.isEmpty();
    }
    Q_INVOKABLE void syncNow();
    Q_INVOKABLE void setSyncEnabled(bool enabled);
    // Deletes everything on the server and turns sync off on this device.
    Q_INVOKABLE void deleteServerData();
    // The app went to the background (false) or came back (true): sync either way.
    void applicationActiveChanged(bool active);
    // True for sources whose chapters are text (the novel reader opens them).
    Q_INVOKABLE bool isNovelSource(const QString& sourceId) const;
    library::SiteAdder& siteAdder() { return m_siteAdder; }

    int newChapterCount() const { return m_library.totalNew(); }
    // Why the saved library could not be read or saved; empty when all is well.
    QString libraryProblem() const { return m_libraryProblem; }
    QString libraryPath() const { return m_libraryPath; }

signals:
    void libraryChanged();
    void settingsChanged();
    void syncChanged();

private:
    QString serverStatus() const;
    void loadLibrary();
    void saveLibrary();
    void saveSettings();
    void updateSettings(const settings::Settings& next);

    SchedulerWrap m_scheduler;
    HttpClientWrap m_http;
    sources::SourceRegistry m_sources;
    library::Library m_library;
    library::UpdateChecker m_checker;
    library::SiteAdder m_siteAdder;
    devicesync::ServerSync m_serverSync;
    LanNetworkWrap m_lanNetwork;
    devicesync::LanSync m_lanSync;
    QString m_syncAddress;
    QString m_syncKey;
    QString m_deleteProblem;
    bool m_serverDataDeleted = false;
    QString m_libraryPath;
    QString m_libraryProblem;
    QString m_settingsPath;
    settings::Settings m_settings;
    bool m_loading = false;
    std::map<std::pair<std::string, std::string>, std::vector<sources::Chapter>> m_chapters;
};
