#pragma once

#include "novelReaderView/novelreadersession.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <string>

// The novel reader's model: one chapter's paragraphs, moving between chapters, where reading
// stopped (restored when the same chapter opens again), and marking a chapter read at its end.
class NovelReaderViewWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString chapterLabel READ chapterLabel NOTIFY changed)
    Q_PROPERTY(QString heading READ heading NOTIFY changed)
    Q_PROPERTY(QStringList paragraphs READ paragraphs NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY changed)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY changed)
    // How far down to scroll when the text arrives (0 top .. 1 end).
    Q_PROPERTY(double startFraction READ startFraction NOTIFY changed)

public:
    explicit NovelReaderViewWrap(QObject* parent = nullptr);

    QString chapterLabel() const;
    QString heading() const;
    QStringList paragraphs() const { return m_paragraphs; }
    bool loading() const;
    QString errorText() const;
    bool hasNext() const;
    bool hasPrevious() const;
    double startFraction() const { return m_startFraction; }

    Q_INVOKABLE void open(const QString& sourceId, const QString& mangaId, const QString& chapterId);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void reload();
    // Remembers how far down the chapter the reader is (when the book is in the library).
    Q_INVOKABLE void savePosition(double fraction);
    // The end of the chapter was reached: mark it read.
    Q_INVOKABLE void finished();

signals:
    void changed();
    // Another chapter's text is showing (the view scrolls to startFraction).
    void textChanged();

private:
    void sessionChanged();

    std::unique_ptr<novelreaderview::NovelReaderSession> m_session;
    std::string m_sourceId;
    std::string m_mangaId;
    std::string m_shownChapterId;
    QStringList m_paragraphs;
    double m_startFraction = 0.0;
};
