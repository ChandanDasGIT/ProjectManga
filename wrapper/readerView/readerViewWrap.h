#pragma once

#include "readerView/readersession.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <string>

// The reader's model: the page image links of one chapter (with the Referer the image host
// wants folded in, see QmlNetworkWrap), moving between chapters, where reading stopped (restored
// when the same chapter opens again), and marking a chapter read.
class ReaderViewWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString chapterLabel READ chapterLabel NOTIFY changed)
    Q_PROPERTY(QVariantList pages READ pages NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool external READ external NOTIFY changed)
    Q_PROPERTY(QString externalUrl READ externalUrl NOTIFY changed)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY changed)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY changed)
    // Where to go when the pages arrive (0 first page .. 1 end), from the place kept in the library.
    Q_PROPERTY(double startFraction READ startFraction NOTIFY changed)

public:
    explicit ReaderViewWrap(QObject* parent = nullptr);
    ~ReaderViewWrap() override;

    QString chapterLabel() const;
    QVariantList pages() const { return m_pages; }
    bool loading() const;
    QString errorText() const;
    bool external() const;
    QString externalUrl() const;
    bool hasNext() const;
    bool hasPrevious() const;
    double startFraction() const { return m_startFraction; }

    // Uses the chapter list the details view loaded for this manga.
    Q_INVOKABLE void open(const QString& sourceId, const QString& mangaId, const QString& chapterId);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    // The chapter before, opened at its last page (turning back past a chapter's first page).
    Q_INVOKABLE void previousAtEnd();
    Q_INVOKABLE void reload();
    // Remembers how far into the chapter the reader is (when the manga is in the library).
    Q_INVOKABLE void savePosition(double fraction);
    // The end of the chapter was reached: mark it read when the manga is in the library.
    Q_INVOKABLE void finished();

signals:
    void changed();
    // Another chapter is showing (the view scrolls back to the top).
    void chapterChanged();

private:
    void sessionChanged();

    std::unique_ptr<readerview::ReaderSession> m_session;
    std::string m_sourceId;
    std::string m_mangaId;
    std::string m_shownChapterId;
    QVariantList m_pages;
    double m_startFraction = 0.0;
    bool m_openAtEnd = false;
};
