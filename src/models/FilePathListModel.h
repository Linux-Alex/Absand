// SPDX-License-Identifier: MPL-2.0

#ifndef FILEPATHLISTMODEL_H
#define FILEPATHLISTMODEL_H

#include <QAbstractListModel>
#include <QStringList>

class FilePathListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole
    };

    explicit FilePathListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addPaths(const QStringList& paths);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString pathAt(int row) const;
    QStringList paths() const;

private:
    QStringList m_paths;
};

#endif // FILEPATHLISTMODEL_H
