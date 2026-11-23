// The wrappers on Qt's real network and event loop, against a small web server on 127.0.0.1 that
// serves sample pages written here. Nothing is read from or written to the user's folders: every
// test keeps its library in a fresh temporary folder ($TMPDIR).
//     wrappertests                 every check
//     wrappertests qtreader        checks whose name contains "qtreader"
#include "testkit.h"

#include "appCoreWrap.h"
#include "discoverViewWrap.h"
#include "libraryViewWrap.h"
#include "mangaDetailsWrap.h"
#include "novelReaderViewWrap.h"
#include "novelsViewWrap.h"
#include "qmlNetworkWrap.h"
#include "readerViewWrap.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>

namespace {

bool waitUntil(const std::function<bool()>& done, int ms = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return done();
}

// Answers GET requests from a path -> body map; remembers each request's head.
class WebServer : public QTcpServer {
public:
    std::map<QByteArray, QByteArray> pages;
    QList<QByteArray> heads;

    WebServer()
    {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    m_buffers[socket] += socket->readAll();
                    const QByteArray& data = m_buffers[socket];
                    if (!data.contains("\r\n\r\n")) return;
                    heads.append(data.left(data.indexOf("\r\n\r\n")));
                    const QByteArray path = data.split(' ').value(1);
                    const auto it = pages.find(path);
                    const bool found = it != pages.end();
                    const QByteArray body = found ? it->second : QByteArray("not here");
                    socket->write((found ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n") +
                                  QByteArray("Content-Type: text/html\r\nConnection: close\r\nContent-Length: ") +
                                  QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                    m_buffers.erase(socket);
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QString url(const char* path) const { return "http://127.0.0.1:" + QString::number(serverPort()) + path; }

private:
    std::map<QTcpSocket*, QByteArray> m_buffers;
};

// The sync server's API on 127.0.0.1, files kept in memory; requests need X-App-Key: secret.
class SyncServer : public QTcpServer {
public:
    std::map<QByteArray, QByteArray> files;
    qint64 wipedAt = 0;
    qint64 version = 1;
    QList<QByteArray> requests;            // "METHOD /path"
    QList<qint64> requestTimes;            // when each arrived (ms since the server started)

    SyncServer()
    {
        m_started.start();
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    QByteArray& data = m_buffers[socket];
                    data += socket->readAll();
                    const qsizetype end = data.indexOf("\r\n\r\n");
                    if (end < 0) return;
                    const QByteArray head = data.left(end);
                    qsizetype length = 0;
                    for (const QByteArray& line : head.split('\n'))
                        if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toLongLong();
                    if (data.size() < end + 4 + length) return;
                    const QByteArray body = data.mid(end + 4, length);
                    const QList<QByteArray> first = head.left(head.indexOf("\r\n")).split(' ');
                    const QByteArray method = first.value(0);
                    const QByteArray path = first.value(1);
                    requests.append(method + " " + path);
                    requestTimes.append(m_started.elapsed());
                    QByteArray status = "200 OK";
                    QByteArray answer;
                    if (head.contains("X-App-Key: secret") && method == "GET" && path.startsWith("/changes")) {
                        // Waits for a change (at most 2 s here; 25 s on the real server).
                        const qint64 after = path.contains("after=") ? path.mid(path.indexOf("after=") + 6).toLongLong() : -1;
                        m_buffers.erase(socket);
                        if (version > after) {
                            answerChanges(socket);
                        } else {
                            m_waiting.append(socket);
                            QTimer::singleShot(2000, socket, [this, socket] {
                                if (m_waiting.removeOne(socket)) answerChanges(socket);
                            });
                        }
                        return;
                    }
                    if (!head.contains("X-App-Key: secret")) {
                        status = "403 Forbidden";
                        answer = R"({"error":"app_key"})";
                    } else if (method == "GET" && path == "/info") {
                        answer = "{\"syncKey\":\"abc=\",\"wipedAt\":" + QByteArray::number(wipedAt) + "}";
                    } else if (method == "PUT" && head.contains("X-Wiped-At: ") &&
                               !head.contains("X-Wiped-At: " + QByteArray::number(wipedAt) + "\r\n")) {
                        status = "409 Conflict";
                        answer = "{\"error\":\"wiped\",\"wipedAt\":" + QByteArray::number(wipedAt) + "}";
                    } else if (method == "GET" && path == "/files") {
                        QByteArray list = "[";
                        for (const auto& [name, file] : files) {
                            if (list.size() > 1) list += ",";
                            list += "{\"name\":\"" + name + "\",\"size\":" + QByteArray::number(file.size()) +
                                    ",\"mtime\":" + QByteArray::number(m_mtimes[name]) + "}";
                        }
                        answer = list + "]";
                    } else if (method == "POST" && path == "/delete-all") {
                        files.clear();
                        wipedAt = ++m_clock;
                        changed();
                        status = "204 No Content";
                    } else if (path.startsWith("/files/")) {
                        const QByteArray name = path.mid(7);
                        if (method == "PUT") {
                            files[name] = body;
                            m_mtimes[name] = ++m_clock;
                            answer = "{}";
                            changed();
                        } else if (!files.contains(name)) {
                            status = "404 Not Found";
                        } else if (method == "GET") {
                            answer = files[name];
                        }
                    } else {
                        status = "404 Not Found";
                    }
                    socket->write("HTTP/1.1 " + status + "\r\nConnection: close\r\nContent-Length: " +
                                  QByteArray::number(answer.size()) + "\r\n\r\n" + answer);
                    socket->disconnectFromHost();
                    m_buffers.erase(socket);
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    void put(const QByteArray& name, const QByteArray& body)
    {
        files[name] = body;
        m_mtimes[name] = ++m_clock;
    }
    QString url() const { return "http://127.0.0.1:" + QString::number(serverPort()); }

private:
    void changed()
    {
        ++version;
        const auto waiting = m_waiting;
        m_waiting.clear();
        for (QTcpSocket* socket : waiting) answerChanges(socket);
    }
    void answerChanges(QTcpSocket* socket)
    {
        const QByteArray answer = "{\"version\":" + QByteArray::number(version) + "}";
        socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(answer.size()) +
                      "\r\n\r\n" + answer);
        socket->disconnectFromHost();
    }

    QList<QTcpSocket*> m_waiting;
    QElapsedTimer m_started;
    std::map<QTcpSocket*, QByteArray> m_buffers;
    std::map<QByteArray, qint64> m_mtimes;
    qint64 m_clock = 1000;
};

QByteArray seriesPage(int chapters)
{
    QByteArray html = "<html><head><title>Moon Knights</title>"
                      "<meta property='og:image' content='/cover.jpg'></head><body>";
    for (int i = chapters; i >= 1; --i)
        html += "<a href='/moon/chapter-" + QByteArray::number(i) + "/'>Chapter " + QByteArray::number(i) + "</a>";
    return html + "</body></html>";
}

struct Setup {
    QTemporaryDir folder;
    WebServer web;
    AppCoreWrap core{folder.path()};

    Setup()
    {
        web.pages["/moon/"] = seriesPage(2);
        web.pages["/moon/chapter-1/"] = "<img src='/img/moon/1a.jpg'><img src='/img/moon/1b.jpg'>";
        web.pages["/moon/chapter-2/"] = "<img src='/img/moon/2a.jpg'>";
        web.pages["/plain/"] = "<p>Nothing to read</p>";
    }

    // Adds the sample site through the library view, as the dialog does.
    bool addMoon(const QString& name = "Moon")
    {
        LibraryViewWrap library;
        bool added = false;
        QObject::connect(&library, &LibraryViewWrap::siteAdded, [&] { added = true; });
        library.addSite(name, web.url("/moon/"));
        return waitUntil([&] { return added || !library.errorText().isEmpty(); }) && added;
    }
};

} // namespace

TEST(qtlibrary, adds_a_site_and_saves_it)
{
    Setup s;
    CHECK(s.addMoon("  Moon  "));
    LibraryViewWrap library;
    const QVariantList entries = library.entries();
    CHECK_EQ(entries.size(), qsizetype(1));
    const QVariantMap e = entries.value(0).toMap();
    CHECK(e["title"].toString() == "Moon");
    CHECK(e["sourceName"].toString() == "Web link");
    const QUrl cover(e["coverUrl"].toString());
    CHECK(cover.adjusted(QUrl::RemoveFragment).toString() == s.web.url("/cover.jpg"));
    CHECK(cover.fragment(QUrl::FullyEncoded).startsWith("pm-referer="));
    CHECK_EQ(e["unreadCount"].toInt(), 0);
    CHECK(e["nextEstimate"].toString().isEmpty());

    QFile saved(s.folder.path() + "/library.json");
    CHECK(saved.open(QIODevice::ReadOnly));
    CHECK(saved.readAll().contains("\"title\": \"Moon\""));
}

TEST(qtlibrary, site_without_cover_uses_first_page)
{
    Setup s;
    s.web.pages["/bare/"] = "<a href='/bare/chapter-1/'>Chapter 1</a>";
    s.web.pages["/bare/chapter-1/"] = "<img src='/img/bare/01.jpg'><img src='/img/bare/02.jpg'>";
    LibraryViewWrap library;
    bool added = false;
    QObject::connect(&library, &LibraryViewWrap::siteAdded, [&] { added = true; });
    library.addSite("Bare", s.web.url("/bare/"));
    CHECK(waitUntil([&] { return added; }));
    const QUrl cover(library.entries().value(0).toMap()["coverUrl"].toString());
    CHECK(cover.adjusted(QUrl::RemoveFragment).toString() == s.web.url("/img/bare/01.jpg"));
}

TEST(qtdetails, fills_in_a_missing_cover)
{
    Setup s;
    library::Entry old;
    old.sourceId = "web";
    old.mangaId = s.web.url("/moon/").toStdString();
    old.title = "Saved before covers";
    s.core.library().add(old);
    MangaDetailsWrap details;
    details.open("web", s.web.url("/moon/"));
    CHECK(waitUntil([&] { return !details.loading(); }));
    CHECK_EQ(s.core.library().find("web", old.mangaId)->coverUrl, s.web.url("/cover.jpg").toStdString());
    CHECK(QUrl(details.coverUrl()).fragment(QUrl::FullyEncoded).startsWith("pm-referer="));
}

TEST(qtlibrary, explains_why_a_site_was_not_added)
{
    Setup s;
    LibraryViewWrap library;
    library.addSite("Moon", "moon.example");
    CHECK(library.errorText().startsWith("Enter the full address"));
    library.addSite("", s.web.url("/moon/"));
    CHECK(library.errorText() == "Give the site a name.");
    library.addSite("Plain", s.web.url("/plain/"));
    CHECK(library.busy());
    CHECK(waitUntil([&] { return !library.busy(); }));
    CHECK(library.errorText() == "No chapter links found on this page.");
    CHECK(library.entries().isEmpty());
    CHECK(s.addMoon());
    library.addSite("Again", s.web.url("/moon/"));
    CHECK(library.errorText() == "This address is already in your library.");
}

TEST(qtdetails, shows_chapters_and_new_ones)
{
    Setup s;
    CHECK(s.addMoon());
    s.web.pages["/moon/"] = seriesPage(3);
    // The daily check would find chapter 3; here the library learns of it directly.
    sources::Chapter three;
    three.id = s.web.url("/moon/chapter-3/").toStdString();
    three.number = "3";
    three.numberValue = 3;
    std::vector<sources::Chapter> list(1, three);
    s.core.library().applyChapters("web", s.web.url("/moon/").toStdString(), list, s.core.scheduler().nowMs());
    CHECK_EQ(s.core.newChapterCount(), 1);

    MangaDetailsWrap details;
    details.open("web", s.web.url("/moon/"));
    CHECK(waitUntil([&] { return !details.loading(); }));
    CHECK(details.errorText().isEmpty());
    CHECK(details.title() == "Moon");                         // the name the user gave
    CHECK(details.inLibrary());
    const QVariantList rows = details.chapters();
    CHECK_EQ(rows.size(), qsizetype(3));
    CHECK(rows.value(0).toMap()["label"].toString() == "Chapter 3");
    CHECK(rows.value(0).toMap()["isNew"].toBool());
    CHECK(!rows.value(1).toMap()["isNew"].toBool());
    CHECK_EQ(s.core.newChapterCount(), 0);                    // opened: the badge is gone
    CHECK(details.continueLabel() == "Start reading");
}

TEST(qtreader, reads_marks_and_moves_on)
{
    Setup s;
    CHECK(s.addMoon());
    MangaDetailsWrap details;
    details.open("web", s.web.url("/moon/"));
    CHECK(waitUntil([&] { return !details.loading(); }));

    ReaderViewWrap reader;
    reader.open("web", s.web.url("/moon/"), details.continueChapterId());
    CHECK(waitUntil([&] { return !reader.loading(); }));
    CHECK(reader.errorText().isEmpty());
    CHECK(reader.chapterLabel() == "Chapter 1");
    const QVariantList pages = reader.pages();
    CHECK_EQ(pages.size(), qsizetype(2));
    const QUrl first(pages.value(0).toString());
    CHECK(first.adjusted(QUrl::RemoveFragment).toString() == s.web.url("/img/moon/1a.jpg"));
    CHECK(first.fragment(QUrl::FullyEncoded).startsWith("pm-referer="));

    reader.finished();
    CHECK(details.continueLabel() == "Continue · Chapter 2");
    CHECK(details.chapters().value(1).toMap()["read"].toBool());

    reader.next();
    CHECK(waitUntil([&] { return !reader.loading(); }));
    CHECK(reader.chapterLabel() == "Chapter 2");
    CHECK_EQ(reader.pages().size(), qsizetype(1));
    CHECK(!reader.hasNext());
    CHECK(reader.hasPrevious());
}

TEST(qtreader, keeps_the_page_and_resumes_there)
{
    Setup s;
    s.web.pages["/moon/chapter-1/"] = "<img src='/img/1.jpg'><img src='/img/2.jpg'><img src='/img/3.jpg'><img src='/img/4.jpg'>";
    CHECK(s.addMoon());
    LibraryViewWrap library;
    CHECK(!library.entries().value(0).toMap()["started"].toBool());
    CHECK_EQ(library.entries().value(0).toMap()["progress"].toDouble(), -1.0);

    MangaDetailsWrap details;
    details.open("web", s.web.url("/moon/"));
    CHECK(waitUntil([&] { return !details.loading(); }));
    {
        ReaderViewWrap reader;
        reader.open("web", s.web.url("/moon/"), details.continueChapterId());
        CHECK(waitUntil([&] { return !reader.loading(); }));
        CHECK_EQ(reader.startFraction(), 0.0);
        reader.savePosition(0.5);                                 // page 3 of 4 at the top
    }
    CHECK(details.continueLabel() == "Continue · Chapter 1");

    CHECK(library.entries().value(0).toMap()["started"].toBool());
    CHECK_EQ(library.entries().value(0).toMap()["progress"].toDouble(), 0.25);   // half of chapter 1 of 2

    ReaderViewWrap again;
    again.open("web", s.web.url("/moon/"), details.continueChapterId());
    CHECK(waitUntil([&] { return !again.loading(); }));
    CHECK_EQ(again.startFraction(), 0.5);
    again.next();                                                 // another chapter starts at the top
    CHECK(waitUntil([&] { return !again.loading(); }));
    CHECK_EQ(again.startFraction(), 0.0);

    // Kept across a restart.
    QFile saved(s.folder.path() + "/library.json");
    CHECK(saved.open(QIODevice::ReadOnly));
    CHECK(saved.readAll().contains("\"positionLabel\": \"Chapter 1\""));
}

TEST(qtreader, turning_back_opens_the_previous_chapter_at_its_end)
{
    Setup s;
    CHECK(s.addMoon());
    MangaDetailsWrap details;
    details.open("web", s.web.url("/moon/"));
    CHECK(waitUntil([&] { return !details.loading(); }));
    const QString chapter2 = details.chapters().value(0).toMap()["id"].toString();

    ReaderViewWrap reader;
    reader.open("web", s.web.url("/moon/"), chapter2);
    CHECK(waitUntil([&] { return !reader.loading(); }));
    CHECK(reader.chapterLabel() == "Chapter 2");
    reader.previousAtEnd();
    CHECK(waitUntil([&] { return !reader.loading(); }));
    CHECK(reader.chapterLabel() == "Chapter 1");
    CHECK_EQ(reader.startFraction(), 1.0);
    reader.next();                                                // only that one turn opens at the end
    CHECK(waitUntil([&] { return !reader.loading(); }));
    CHECK_EQ(reader.startFraction(), 0.0);
}

TEST(qtdetails, chapter_order_is_chosen_and_kept)
{
    Setup s;
    CHECK(s.addMoon());
    MangaDetailsWrap details;
    details.open("web", s.web.url("/moon/"));
    CHECK(waitUntil([&] { return !details.loading(); }));
    CHECK(details.newestFirst());
    CHECK(details.chapters().value(0).toMap()["label"].toString() == "Chapter 2");

    s.core.setChapterOrder("oldest");
    CHECK(!details.newestFirst());
    CHECK(details.chapters().value(0).toMap()["label"].toString() == "Chapter 1");
    CHECK(s.core.chapterOrder() == "oldest");

    s.core.setMangaReaderMode("flip");
    CHECK(s.core.mangaReaderMode() == "flip");
    s.core.setFlipSpeed("fast");
    s.core.setFlipSpeed("nope");
    CHECK(s.core.flipSpeed() == "normal");
    s.core.setFlipSpeed("slow");
    QFile saved(s.folder.path() + "/settings.json");
    CHECK(saved.open(QIODevice::ReadOnly));
    const QByteArray text = saved.readAll();
    CHECK(text.contains("\"chapterOrder\": \"oldest\""));
    CHECK(text.contains("\"mode\": \"flip\""));
    CHECK(text.contains("\"flipSpeed\": \"slow\""));
}

TEST(qtlibrary, syncs_through_the_server)
{
    Setup s;
    CHECK(!s.core.syncConfigured());
    CHECK(s.core.syncStatus() == "Sync isn't set up in this build.");
    CHECK(s.addMoon());

    SyncServer server;
    server.put("ProjectManga-otherdevice.json",
               R"({"entries":[{"sourceId":"mangadex","mangaId":"from-phone","title":"From phone","addedMs":5}]})");
    s.core.setSyncServer(server.url(), "secret");
    CHECK(s.core.syncConfigured());
    CHECK(waitUntil([&] { return s.core.syncStatus().startsWith("Last synced"); }));
    CHECK(s.core.syncOk());
    CHECK(s.core.syncStatus().contains("1 other device"));
    CHECK(s.core.library().contains("mangadex", "from-phone"));
    const QByteArray own = "ProjectManga-" + QByteArray::fromStdString(s.core.settings().deviceId) + ".json";
    CHECK(waitUntil([&] { return server.files.contains(own); }));
    CHECK(server.files[own].contains("from-phone"));
    CHECK(server.files[own].contains("Moon"));

    // Delete my data: the server is emptied and sync turns off here, also after a restart.
    s.core.deleteServerData();
    CHECK(waitUntil([&] { return !s.core.syncEnabled(); }));
    CHECK(server.files.empty());
    CHECK(s.core.syncStatus() == "Your data was deleted from the server. Sync is off on this device.");
    QFile saved(s.folder.path() + "/settings.json");
    CHECK(saved.open(QIODevice::ReadOnly));
    CHECK(saved.readAll().contains("\"enabled\": false"));
    s.core.syncNow();
    QCoreApplication::processEvents();
    CHECK(server.files.empty());

    // Turned on again: this device's file goes back up.
    s.core.setSyncEnabled(true);
    CHECK(waitUntil([&] { return server.files.contains(own); }));
    CHECK(s.core.syncEnabled());

    // Deleted on another device: this one asks before putting its data back.
    server.files.clear();
    server.wipedAt = 99999;
    s.core.syncNow();
    CHECK(waitUntil([&] { return s.core.syncWipeQuestion(); }));
    CHECK(s.core.syncStatus().startsWith("Your data was deleted from the server on another device."));
    CHECK(server.files.empty());
    s.core.answerWipe(true);
    CHECK(waitUntil([&] { return server.files.contains(own); }));
    CHECK(!s.core.syncWipeQuestion());
    CHECK_EQ(s.core.settings().serverWipedAt, qint64(99999));

    // Deleted elsewhere again, and this time sync stays off here.
    server.files.clear();
    server.wipedAt = 100000;
    s.core.syncNow();
    CHECK(waitUntil([&] { return s.core.syncWipeQuestion(); }));
    s.core.answerWipe(false);
    CHECK(!s.core.syncEnabled());
    CHECK_EQ(s.core.settings().serverWipedAt, qint64(100000));
    QCoreApplication::processEvents();
    CHECK(server.files.empty());
    s.core.setSyncEnabled(true);
    CHECK(waitUntil([&] { return server.files.contains(own); }));
    CHECK(!s.core.syncWipeQuestion());

    // A wrong key is reported.
    s.core.setSyncServer(server.url(), "wrong");
    CHECK(waitUntil([&] { return !s.core.syncOk(); }));
    CHECK(s.core.syncStatus().startsWith("The sync server refused this app's key."));
}

TEST(qtlibrary, syncs_over_wifi_with_own_devices_only)
{
    // Three app cores on this computer, as if on one Wi-Fi: two with the same key, one with
    // another. The server address leads nowhere, so only Wi-Fi sync can carry anything.
    QTemporaryDir pcFolder, phoneFolder, strangerFolder;
    AppCoreWrap pc(pcFolder.path());
    AppCoreWrap phone(phoneFolder.path());
    AppCoreWrap stranger(strangerFolder.path());
    library::Entry e;
    e.sourceId = "mangadex";
    e.title = "Shared";
    e.mangaId = "from-pc";
    e.addedMs = 10;
    pc.library().add(e);
    e.mangaId = "from-stranger";
    stranger.library().add(e);

    const QString nowhere = "http://127.0.0.1:1";
    pc.setSyncServer(nowhere, "secret");
    phone.setSyncServer(nowhere, "secret");
    stranger.setSyncServer(nowhere, "another secret");
    CHECK(waitUntil([&] { return phone.library().contains("mangadex", "from-pc"); }, 20000));
    CHECK(waitUntil([&] { return pc.syncStatus().contains("Syncing over Wi-Fi with 1 device."); }, 20000));

    // A change on the phone reaches the pc.
    e.mangaId = "from-phone";
    phone.library().add(e);
    CHECK(waitUntil([&] { return pc.library().contains("mangadex", "from-phone"); }, 20000));

    CHECK(!pc.library().contains("mangadex", "from-stranger"));
    CHECK(!stranger.library().contains("mangadex", "from-pc"));
    CHECK(!stranger.syncStatus().contains("Wi-Fi"));
}

TEST(qtlibrary, filters_by_kind_and_removes)
{
    Setup s;
    library::Entry e;
    e.sourceId = "mangadex";
    e.mangaId = "m1";
    e.title = "A manga";
    e.addedMs = 10;
    s.core.library().add(e);
    e.sourceId = "standardebooks";
    e.mangaId = "/ebooks/holmes";
    e.title = "A novel";
    e.addedMs = 20;
    s.core.library().add(e);

    LibraryViewWrap library;
    CHECK(library.kindKey() == "all");
    CHECK_EQ(library.entries().size(), qsizetype(2));
    CHECK_EQ(library.kindCount(), 2);
    library.setKindKey("novel");
    CHECK(library.kindKey() == "novel");
    CHECK_EQ(library.entries().size(), qsizetype(1));
    CHECK(library.entries().value(0).toMap()["title"].toString() == "A novel");
    CHECK_EQ(library.kindCount(), 1);
    CHECK_EQ(library.totalCount(), 2);
    library.setKindKey("manga");
    CHECK(library.entries().value(0).toMap()["title"].toString() == "A manga");
    // The choice is kept in the settings.
    CHECK(s.core.settings().libraryKind == "manga");
    library.setKindKey("nonsense");
    CHECK(library.kindKey() == "all");

    library.remove("standardebooks", "/ebooks/holmes");
    CHECK(!s.core.library().contains("standardebooks", "/ebooks/holmes"));
    CHECK_EQ(library.totalCount(), 1);
    // Remembered as removed, so another device's copy doesn't bring it back.
    CHECK_EQ(s.core.library().removals().size(), std::size_t(1));
}

TEST(qtlibrary, a_change_reaches_another_device_at_once)
{
    // Two app instances on one sync server. (Their Wi-Fi sync runs too, so the check below looks
    // at the server's own record to prove the server path delivered the change.)
    SyncServer server;
    QTemporaryDir pcFolder, phoneFolder;
    AppCoreWrap pc(pcFolder.path());
    AppCoreWrap phone(phoneFolder.path());
    pc.setSyncServer(server.url(), "secret");
    phone.setSyncServer(server.url(), "secret");
    CHECK(waitUntil([&] { return pc.syncStatus().startsWith("Last synced") && phone.syncStatus().startsWith("Last synced"); }));
    // Let both settle into waiting for news.
    QElapsedTimer settle;
    settle.start();
    while (settle.elapsed() < 500) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    library::Entry e;
    e.sourceId = "mangadex";
    e.mangaId = "instant";
    e.title = "Instant";
    e.addedMs = 10;
    QElapsedTimer took;
    took.start();
    phone.library().add(e);
    CHECK(waitUntil([&] { return pc.library().contains("mangadex", "instant"); }, 10000));
    // The 1.5 s upload delay plus the transfer; the regular check would take a minute.
    CHECK(took.elapsed() < 5000);
    std::printf("    change arrived after %lld ms\n", static_cast<long long>(took.elapsed()));

    // Only the pc downloads the phone's file, and it does so as soon as the server tells it.
    const QByteArray phoneFile = "/files/ProjectManga-" + QByteArray::fromStdString(phone.settings().deviceId) + ".json";
    CHECK(waitUntil([&] { return server.requests.lastIndexOf("GET " + phoneFile) > server.requests.lastIndexOf("PUT " + phoneFile); }));
    const qsizetype put = server.requests.lastIndexOf("PUT " + phoneFile);
    const qsizetype fetched = server.requests.lastIndexOf("GET " + phoneFile);
    CHECK(put >= 0);
    const qint64 gap = server.requestTimes.value(fetched) - server.requestTimes.value(put);
    std::printf("    the pc fetched it %lld ms after the upload\n", static_cast<long long>(gap));
    CHECK(gap < 1000);
}

TEST(qtimages, image_requests_carry_referer_and_cache)
{
    WebServer web;
    web.pages["/img/p1.jpg"] = "fake image bytes";
    QTemporaryDir cache;
    QmlNetworkWrap factory(cache.path());
    QObject owner;
    QNetworkAccessManager* network = factory.create(&owner);

    const QUrl link = QmlNetworkWrap::withReferer(web.url("/img/p1.jpg"), "https://site.example/ch 1/?a=1&b=2#x");
    QNetworkReply* reply = network->get(QNetworkRequest(link));
    CHECK(waitUntil([&] { return reply->isFinished(); }));
    CHECK(reply->readAll() == "fake image bytes");
    CHECK_EQ(web.heads.size(), qsizetype(1));
    const QByteArray head = web.heads.value(0);
    CHECK(head.startsWith("GET /img/p1.jpg HTTP/1.1"));
    CHECK(head.contains("Referer: https://site.example/ch 1/?a=1&b=2#x"));
    CHECK(head.contains("User-Agent: ProjectManga/0.1"));
    CHECK(!QDir(cache.path() + "/0").isEmpty());
}

TEST(qtimages, only_image_load_errors_are_quiet)
{
    CHECK(QmlNetworkWrap::isImageLoadError("qrc:/x/CoverImage.qml:14:5: QML QQuickImage: Error transferring https://a.example/c.jpg - server replied: Service Unavailable"));
    CHECK(!QmlNetworkWrap::isImageLoadError("qrc:/x/Main.qml:3: TypeError: Cannot read property 'title' of undefined"));
    CHECK(!QmlNetworkWrap::isImageLoadError("Unable to assign [undefined] to double"));
}

TEST(qtlibrary, estimate_text)
{
    const qint64 day = 86'400'000;
    const qint64 now = 1'700'000'000'000;
    CHECK(LibraryViewWrap::estimateText(0, false, now).isEmpty());
    CHECK(LibraryViewWrap::estimateText(0, true, now) == "unknown");
    CHECK(LibraryViewWrap::estimateText(now + 3 * day, true, now).endsWith("(estimated)"));
    CHECK(LibraryViewWrap::estimateText(now - day, true, now).endsWith("(estimated, late)"));
    CHECK(LibraryViewWrap::estimateText(now + 3 * day, true, now).startsWith("≈ "));
}

TEST(qtlibrary, damaged_library_file_is_kept_aside)
{
    QTemporaryDir folder;
    {
        QFile broken(folder.path() + "/library.json");
        CHECK(broken.open(QIODevice::WriteOnly));
        broken.write("{\"entries\": [");
    }
    AppCoreWrap core(folder.path());
    CHECK(core.libraryProblem().contains("damaged"));
    CHECK(core.library().entries().empty());
    const QStringList kept = QDir(folder.path()).entryList({"library.json.damaged-*"});
    CHECK_EQ(kept.size(), qsizetype(1));
}

TEST(qtnovels, add_a_novel_site_read_and_come_back)
{
    Setup s;
    QByteArray story;
    for (int i = 1; i <= 30; ++i) story += "<p>Paragraph " + QByteArray::number(i) + " of a story long enough to be found as the text of this page.</p>";
    s.web.pages["/tale/"] = "<title>Tale</title><a href='/tale/chapter-1/'>Chapter 1</a><a href='/tale/chapter-2/'>Chapter 2</a>";
    s.web.pages["/tale/chapter-1/"] = "<nav><p>Menu</p></nav><h1>One</h1><div class='text'>" + story + "</div>";
    s.web.pages["/tale/chapter-2/"] = "<h1>Two</h1><div>" + story + "</div>";
    CHECK(s.core.isNovelSource("webnovel"));
    CHECK(!s.core.isNovelSource("web"));

    NovelsViewWrap novels;
    bool added = false;
    QObject::connect(&novels, &NovelsViewWrap::siteAdded, [&] { added = true; });
    novels.addSite("Tale", s.web.url("/tale/"));
    CHECK(waitUntil([&] { return added || !novels.addError().isEmpty(); }));
    CHECK(added);

    const QString chapter1 = s.web.url("/tale/chapter-1/");
    NovelReaderViewWrap reader;
    reader.open("webnovel", s.web.url("/tale/"), chapter1);
    CHECK(waitUntil([&] { return !reader.loading(); }));
    CHECK(reader.heading() == "One");
    CHECK_EQ(reader.paragraphs().size(), qsizetype(30));
    CHECK_EQ(reader.startFraction(), 0.0);
    CHECK(reader.hasNext());
    reader.savePosition(0.4);

    NovelReaderViewWrap again;
    again.open("webnovel", s.web.url("/tale/"), chapter1);
    CHECK(waitUntil([&] { return !again.loading(); }));
    CHECK(std::abs(again.startFraction() - 0.4) < 0.01);
    again.finished();
    CHECK_EQ(s.core.library().find("webnovel", s.web.url("/tale/").toStdString())->readNumbers.size(), std::size_t(1));
}

TEST(qtnovels, reader_settings_are_kept)
{
    Setup s;
    s.core.setReaderTheme("light");
    s.core.setReaderFontSize(99);
    CHECK(s.core.readerTheme() == "light");
    CHECK_EQ(s.core.readerFontSize(), 40);
    QFile saved(s.folder.path() + "/settings.json");
    CHECK(saved.open(QIODevice::ReadOnly));
    CHECK(saved.readAll().contains("\"theme\": \"light\""));
}

TEST(qtdiscover, a_finished_shelf_updates_only_its_own_row)
{
    ShelfListModel model;
    int resets = 0;
    QList<std::pair<int, QList<int>>> changes;
    QObject::connect(&model, &QAbstractItemModel::modelReset, [&] { ++resets; });
    QObject::connect(&model, &QAbstractItemModel::dataChanged,
                     [&](const QModelIndex& from, const QModelIndex& to, const QList<int>& roles) {
                         CHECK_EQ(from.row(), to.row());
                         changes.append({from.row(), roles});
                     });

    std::vector<discoverview::Shelf> shelves(2);
    shelves[0].key = "popular";
    shelves[0].title = "Popular";
    shelves[0].loading = true;
    shelves[1].key = "updated";
    shelves[1].title = "Recently updated";
    shelves[1].loading = true;
    model.update(shelves);
    CHECK_EQ(resets, 1);
    CHECK_EQ(model.rowCount(), 2);

    sources::MangaSummary solo;
    solo.mangaId = "solo";
    solo.title = "Solo";
    shelves[0].items = {solo};
    shelves[0].loading = false;
    model.update(shelves);
    CHECK_EQ(resets, 1);
    CHECK_EQ(changes.size(), qsizetype(1));
    CHECK_EQ(changes.value(0).first, 0);
    CHECK(changes.value(0).second == (QList<int>{ShelfListModel::ItemsRole, ShelfListModel::LoadingRole}));
    const QVariantList items = model.data(model.index(0), ShelfListModel::ItemsRole).toList();
    CHECK_EQ(items.size(), qsizetype(1));
    CHECK(items.value(0).toMap().value("mangaId").toString() == "solo");
    CHECK(model.data(model.index(1), ShelfListModel::LoadingRole).toBool());

    model.update(shelves);
    CHECK_EQ(changes.size(), qsizetype(1));
    CHECK_EQ(resets, 1);

    discoverview::Shelf forYou;
    forYou.key = "foryou";
    shelves.insert(shelves.begin(), forYou);
    model.update(shelves);
    CHECK_EQ(resets, 2);
    CHECK_EQ(model.rowCount(), 3);
}

TEST(qtdiscover, more_titles_are_added_without_a_reset)
{
    ResultListModel model;
    int resets = 0;
    QList<std::pair<int, int>> inserted;
    QObject::connect(&model, &QAbstractItemModel::modelReset, [&] { ++resets; });
    QObject::connect(&model, &QAbstractItemModel::rowsInserted,
                     [&](const QModelIndex&, int first, int last) { inserted.append({first, last}); });

    std::vector<sources::MangaSummary> items(2);
    items[0].mangaId = "a";
    items[0].title = "A";
    items[1].mangaId = "b";
    model.update("mangadex", items);
    CHECK_EQ(resets, 1);
    CHECK_EQ(model.rowCount(), 2);
    CHECK(model.data(model.index(0), ResultListModel::TitleRole).toString() == "A");
    CHECK(model.data(model.index(0), ResultListModel::SourceIdRole).toString() == "mangadex");

    items.resize(5);
    items[2].mangaId = "c";
    items[3].mangaId = "d";
    items[4].mangaId = "e";
    model.update("mangadex", items);
    CHECK_EQ(resets, 1);
    CHECK_EQ(inserted.size(), qsizetype(1));
    CHECK(inserted.value(0) == (std::pair<int, int>{2, 4}));
    CHECK_EQ(model.rowCount(), 5);

    model.update("mangadex", items);
    CHECK_EQ(resets, 1);
    CHECK_EQ(inserted.size(), qsizetype(1));

    items.erase(items.begin());
    model.update("mangadex", items);
    CHECK_EQ(resets, 2);
    CHECK_EQ(model.rowCount(), 4);

    // The same titles from another source are another list.
    model.update("gutenberg", items);
    CHECK_EQ(resets, 3);
    CHECK(model.data(model.index(0), ResultListModel::SourceIdRole).toString() == "gutenberg");
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion("0.1");
    const std::string filter = argc > 1 ? argv[1] : "";
    auto& tests = testkit::allTests();
    std::sort(tests.begin(), tests.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    int ran = 0;
    for (const auto& test : tests) {
        if (test.name.find(filter) == std::string::npos) continue;
        std::printf("%s\n", test.name.c_str());
        test.run();
        ++ran;
    }
    if (ran == 0) {
        std::printf("No check matches \"%s\".\n", filter.c_str());
        return 1;
    }
    std::printf("\n%d checks, %d failed.\n", ran, testkit::failures());
    return testkit::failures() == 0 ? 0 : 1;
}
