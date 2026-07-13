// SPDX-License-Identifier: MPL-2.0

#include "FilePathListModel.h"

#include <QFileInfo>

FilePathListModel::FilePathListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int FilePathListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_paths.size();
}

QVariant FilePathListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_paths.size()) {
        return {};
    }

    const QString& path = m_paths.at(index.row());
    const QFileInfo info(path);

    switch (role) {
    case NameRole:
        return info.fileName().isEmpty() ? path : info.fileName();
    case PathRole:
        return path;
    default:
        return {};
    }
}

QHash<int, QByteArray> FilePathListModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { PathRole, "path" }
    };
}

void FilePathListModel::addPaths(const QStringList& paths)
{
    QStringList unique;
    for (const QString& path : paths) {
        if (!m_paths.contains(path) && !unique.contains(path)) {
            unique.append(path);
        }
    }

    if (unique.isEmpty()) {
        return;
    }

    const int start = m_paths.size();
    beginInsertRows(QModelIndex(), start, start + unique.size() - 1);
    m_paths.append(unique);
    endInsertRows();
}

void FilePathListModel::removeAt(int row)
{
    if (row < 0 || row >= m_paths.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_paths.removeAt(row);
    endRemoveRows();
}

void FilePathListModel::clear()
{
    if (m_paths.isEmpty()) {
        return;
    }
    beginResetModel();
    m_paths.clear();
    endResetModel();
}

QString FilePathListModel::pathAt(int row) const
{
    if (row < 0 || row >= m_paths.size()) {
        return {};
    }
    return m_paths.at(row);
}

QStringList FilePathListModel::paths() const
{
    return m_paths;
}
