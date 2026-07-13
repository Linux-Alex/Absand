// SPDX-License-Identifier: MPL-2.0

#ifndef ARCHIVEENTRYMODEL_H
#define ARCHIVEENTRYMODEL_H

#include <QAbstractListModel>

class ArchiveEntryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        SizeRole,
        TypeRole,
        ModifiedRole,
        IsDirRole,
        DepthRole,
        IconRole
    };

    struct Entry
    {
        QString name;
        QString path;
        QString sizeText;
        QString typeText;
        QString modifiedText;
        QString iconName;
        int depth = 0;
        bool isDir = false;
    };

    explicit ArchiveEntryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QVector<Entry>& entries);
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirAt(int row) const;

private:
    QVector<Entry> m_entries;
};

#endif // ARCHIVEENTRYMODEL_H
