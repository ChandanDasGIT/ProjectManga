#pragma once

#include "mangaDetails/chapterlist.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

// The details view's model: one manga, its chapter rows (in the order newestFirst says) as
// { id, label, group, date, read, isNew, external, externalUrl }, and its place in the library.
// Opening a manga that is in the library clears its "new" badge; the rows still mark what was new.
class MangaDetailsWrap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString coverUrl READ coverUrl NOTIFY changed)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY changed)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY changed)
    Q_PROPERTY(bool inLibrary READ inLibrary NOTIFY changed)
    Q_PROPERTY(QString continueChapterId READ continueChapterId NOTIFY changed)
    Q_PROPERTY(QString continueLabel READ continueLabel NOTIFY changed)
    // The order the rows are in; AppCoreWrap.chapterOrder changes it.
    Q_PROPERTY(bool newestFirst READ newestFirst NOTIFY changed)

public:
    explicit MangaDetailsWrap(QObject* parent = nullptr);

    QString title() const;
    QString description() const;
    QString coverUrl() const;
    QString subtitle() const;
    QString sourceName() const;
    bool loading() const;
    QString errorText() const;
    QVariantList chapters() const { return m_rows; }
    bool inLibrary() const;
    QString continueChapterId() const;
    QString continueLabel() const;
    bool newestFirst() const;

    Q_INVOKABLE void open(const QString& sourceId, const QString& mangaId);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void toggleLibrary();

signals:
    void changed();

private:
    void chaptersArrived();
    // A library entry saved without a cover gets the one the details found.
    void fillMissingCover();
    // A library entry remembers the manga's genres and themes (for "For you" in Discover).
    void keepTags();
    void rebuildRows();
    const library::Entry* entry() const;

    std::unique_ptr<mangadetails::ChapterList> m_list;
    QVariantList m_rows;
    double m_seenBefore = -1.0;        // what counted as seen when the manga was opened
};
