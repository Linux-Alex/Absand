// SPDX-License-Identifier: MPL-2.0

#include "ArchiveEntryModel.h"

ArchiveEntryModel::ArchiveEntryModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ArchiveEntryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant ArchiveEntryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const Entry& entry = m_entries.at(index.row());
    switch (role) {
    case NameRole: return entry.name;
    case PathRole: return entry.path;
    case SizeRole: return entry.sizeText;
    case TypeRole: return entry.typeText;
    case ModifiedRole: return entry.modifiedText;
    case IsDirRole: return entry.isDir;
    case DepthRole: return entry.depth;
    case IconRole: return entry.iconName;
    default: return {};
    }
}

QHash<int, QByteArray> ArchiveEntryModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { PathRole, "path" },
        { SizeRole, "sizeText" },
        { TypeRole, "typeText" },
        { ModifiedRole, "modifiedText" },
        { IsDirRole, "isDir" },
        { DepthRole, "depth" },
        { IconRole, "iconName" }
    };
}

void ArchiveEntryModel::setEntries(const QVector<Entry>& entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

QString ArchiveEntryModel::pathAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return {};
    }
    return m_entries.at(row).path;
}

bool ArchiveEntryModel::isDirAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return false;
    }
    return m_entries.at(row).isDir;
}
