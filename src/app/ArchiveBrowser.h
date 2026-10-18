// SPDX-License-Identifier: MPL-2.0

#ifndef ARCHIVEBROWSER_H
#define ARCHIVEBROWSER_H

#include "models/ArchiveEntryModel.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include <memory>

namespace bit7z {
class Bit7zLibrary;
class BitArchiveReader;
class BitInFormat;
class BitInOutFormat;
}

/**
 * Reads an archive and exposes its contents to the QML browser page.
 *
 * The heavy lifting (7-Zip through bit7z) is the same as in the widget dialog;
 * this class only turns the entries into a flat model with a depth column, so a
 * Qt Quick list can show the tree with indentation.
 */
class ArchiveBrowser : public QObject
{
    Q_OBJECT

public:
    explicit ArchiveBrowser(QObject* parent = nullptr);
    ~ArchiveBrowser() override;

    Q_PROPERTY(QAbstractItemModel* entries READ entries CONSTANT)
    Q_PROPERTY(bool archiveOpen READ archiveOpen NOTIFY stateChanged)
    Q_PROPERTY(QString archivePath READ archivePath NOTIFY stateChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY stateChanged)
    Q_PROPERTY(int matchingCount READ matchingCount NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

    QAbstractItemModel* entries();
    bool archiveOpen() const;
    QString archivePath() const;
    QString filterText() const;
    void setFilterText(const QString& text);
    int itemCount() const;
    int matchingCount() const;
    QString statusText() const;

    Q_INVOKABLE QString openArchive(const QString& path, const QString& password);
    Q_INVOKABLE void closeArchive();
    Q_INVOKABLE QString refresh();
    Q_INVOKABLE QString extractPaths(const QStringList& archivePaths, const QString& destinationFolder);
    Q_INVOKABLE void copyPaths(const QStringList& archivePaths);
    Q_INVOKABLE QString addFiles(const QStringList& localFiles);

signals:
    void stateChanged();
    void filterTextChanged();

private:
    struct Entry
    {
        QString path;
        QString name;
        bool isDir = false;
        int depth = 0;
        qint64 size = 0;
        QDateTime modified;
        int archiveIndex = -1;
    };

    void loadEntries();
    void rebuildModel();
    bool ensureLibrary();
    QStringList indicesForSelection(const QStringList& archivePaths) const;

    std::unique_ptr<bit7z::Bit7zLibrary> m_library;
    std::unique_ptr<bit7z::BitArchiveReader> m_reader;
    const bit7z::BitInOutFormat* m_format = nullptr;

    ArchiveEntryModel* m_model = nullptr;
    QVector<Entry> m_entries;
    QString m_archivePath;
    QString m_password;
    QString m_filterText;
    QString m_statusText;
    int m_matchingCount = 0;
};

#endif // ARCHIVEBROWSER_H
