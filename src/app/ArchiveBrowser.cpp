// SPDX-License-Identifier: MPL-2.0

#include "ArchiveBrowser.h"

#include "core/SevenZipRuntime.h"

#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/biterror.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitarchiveitem.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitarchivewriter.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitformat.hpp>

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>

#include <exception>
#include <memory>

namespace {

const bit7z::BitInOutFormat* formatForExtension(const QString& suffix)
{
    const QString lower = suffix.toLower();
    if (lower == QStringLiteral("7z")) {
        return &bit7z::BitFormat::SevenZip;
    }
    if (lower == QStringLiteral("zip")) {
        return &bit7z::BitFormat::Zip;
    }
    return nullptr;
}

QString humanSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    if (bytes < 1024LL * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    }
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

QString normalizePath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    while (path.contains(QStringLiteral("//"))) {
        path.replace(QStringLiteral("//"), QStringLiteral("/"));
    }
    return path;
}

} // namespace

ArchiveBrowser::ArchiveBrowser(QObject* parent)
    : QObject(parent)
    , m_model(new ArchiveEntryModel(this))
{
}

ArchiveBrowser::~ArchiveBrowser() = default;

QAbstractItemModel* ArchiveBrowser::entries()
{
    return m_model;
}

bool ArchiveBrowser::archiveOpen() const
{
    return m_reader != nullptr;
}

bool ArchiveBrowser::archiveEncrypted() const
{
    return m_archiveEncrypted;
}

bool ArchiveBrowser::passwordRequired() const
{
    return m_passwordRequired;
}

QString ArchiveBrowser::archivePath() const
{
    return m_archivePath;
}

QString ArchiveBrowser::filterText() const
{
    return m_filterText;
}

void ArchiveBrowser::setFilterText(const QString& text)
{
    if (text == m_filterText) {
        return;
    }
    m_filterText = text;
    emit filterTextChanged();
    rebuildModel();
}

int ArchiveBrowser::itemCount() const
{
    return m_entries.size();
}

int ArchiveBrowser::matchingCount() const
{
    return m_matchingCount;
}

QString ArchiveBrowser::statusText() const
{
    return m_statusText;
}

bool ArchiveBrowser::ensureLibrary()
{
    if (m_library) {
        return true;
    }

    const QString libraryPath = resolveSevenZipLibraryPath();
    if (libraryPath.isEmpty()) {
        m_statusText = tr("Could not find the bundled 7-Zip runtime. Put it next to the app or set ABSAND_7ZIP_LIBRARY.");
        return false;
    }

    try {
        m_library = std::make_unique<bit7z::Bit7zLibrary>(libraryPath.toStdString());
        return true;
    } catch (const std::exception& exception) {
        m_statusText = tr("The 7-Zip library could not be loaded: %1").arg(exception.what());
        return false;
    }
}

QString ArchiveBrowser::openArchive(const QString& path, const QString& password)
{
    closeArchive();

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        m_statusText = tr("The archive %1 does not exist.").arg(path);
        emit stateChanged();
        return m_statusText;
    }

    const bit7z::BitInOutFormat* format = formatForExtension(info.suffix());
    if (!format) {
        m_statusText = tr("Unsupported archive type. Open a .zip or .7z archive.");
        emit stateChanged();
        return m_statusText;
    }

    if (!ensureLibrary()) {
        emit stateChanged();
        return m_statusText;
    }

    m_archiveEncrypted = false;
    m_passwordRequired = false;

    try {
        m_reader = std::make_unique<bit7z::BitArchiveReader>(
            *m_library, path.toStdString(), *format, password.toStdString());
        m_format = format;
        m_archivePath = path;
        m_password = password;
        m_filterText.clear();
        loadEntries();

        // Archives whose entries are encrypted open without a password, but the
        // password is needed for extraction, so the browser says so instead of
        // failing later.
        m_passwordRequired = m_archiveEncrypted && password.isEmpty();
        m_statusText = tr("%1 — %2 item(s)").arg(info.fileName()).arg(m_entries.size());
        if (m_archiveEncrypted) {
            m_statusText += QLatin1String(" · ") + (m_passwordRequired
                ? tr("encrypted, password needed to extract")
                : tr("encrypted"));
        }
        emit filterTextChanged();
        emit stateChanged();
        return QString();
    } catch (const bit7z::BitException& exception) {
        m_reader.reset();
        m_format = nullptr;
        if (exception.code() == bit7z::BitFailureSource::WrongPassword) {
            // The headers are encrypted, so the password is needed before the
            // contents can even be listed.
            m_passwordRequired = true;
            m_statusText = password.isEmpty()
                ? tr("This archive is encrypted. Enter its password to browse it.")
                : tr("The password does not open this archive.");
            emit stateChanged();
            return m_statusText;
        }
        m_statusText = tr("The archive could not be opened: %1").arg(exception.what());
        emit stateChanged();
        return m_statusText;
    } catch (const std::exception& exception) {
        m_reader.reset();
        m_format = nullptr;
        m_statusText = tr("The archive could not be opened: %1").arg(exception.what());
        emit stateChanged();
        return m_statusText;
    }
}

void ArchiveBrowser::loadEntries()
{
    m_entries.clear();
    if (!m_reader) {
        rebuildModel();
        return;
    }

    const quint32 count = m_reader->itemsCount();
    for (quint32 index = 0; index < count; ++index) {
        const auto item = m_reader->itemAt(index);
        const QString archivePath = normalizePath(QString::fromStdString(item.path()));
        if (archivePath.isEmpty()) {
            continue;
        }

        Entry entry;
        entry.path = archivePath;
        entry.name = archivePath.section(QLatin1Char('/'), -1);
        entry.isDir = item.isDir();
        entry.depth = archivePath.count(QLatin1Char('/'));
        entry.size = static_cast<qint64>(item.size());
        entry.modified = QDateTime::fromSecsSinceEpoch(
            static_cast<qint64>(std::chrono::duration_cast<std::chrono::seconds>(
                item.lastWriteTime().time_since_epoch()).count()));
        entry.archiveIndex = static_cast<int>(index);
        m_entries.append(entry);

        if (!m_archiveEncrypted && item.isEncrypted()) {
            m_archiveEncrypted = true;
        }
    }

    // Only directory entries coming from the archive are used for parents.
    rebuildModel();
}

void ArchiveBrowser::rebuildModel()
{
    QVector<ArchiveEntryModel::Entry> modelEntries;
    const QString needle = m_filterText.trimmed();

    if (needle.isEmpty()) {
        for (const Entry& entry : m_entries) {
            ArchiveEntryModel::Entry modelEntry;
            modelEntry.name = entry.name;
            modelEntry.path = entry.path;
            modelEntry.sizeText = entry.isDir ? QString() : humanSize(entry.size);
            modelEntry.typeText = entry.isDir ? tr("Folder")
                                              : (entry.name.contains(QLatin1Char('.'))
                                                     ? entry.name.section(QLatin1Char('.'), -1).toUpper() + QStringLiteral(" file")
                                                     : tr("File"));
            modelEntry.modifiedText = entry.modified.isValid()
                ? entry.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                : QString();
            modelEntry.isDir = entry.isDir;
            modelEntry.depth = entry.depth;
            modelEntry.iconName = entry.isDir ? QStringLiteral("folder") : QStringLiteral("file");
            modelEntries.append(modelEntry);
        }
    } else {
        // Keep matching entries plus their parent folders so the tree stays readable.
        QSet<QString> keep;
        for (const Entry& entry : m_entries) {
            if (!entry.path.contains(needle, Qt::CaseInsensitive)) {
                continue;
            }
            keep.insert(entry.path);
            QString parent = entry.path;
            while (parent.contains(QLatin1Char('/'))) {
                parent = parent.section(QLatin1Char('/'), 0, -2);
                keep.insert(parent);
            }
        }

        for (const Entry& entry : m_entries) {
            if (!keep.contains(entry.path)) {
                continue;
            }
            ArchiveEntryModel::Entry modelEntry;
            modelEntry.name = entry.name;
            modelEntry.path = entry.path;
            modelEntry.sizeText = entry.isDir ? QString() : humanSize(entry.size);
            modelEntry.typeText = entry.isDir ? tr("Folder") : tr("File");
            modelEntry.modifiedText = entry.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
            modelEntry.isDir = entry.isDir;
            modelEntry.depth = entry.depth;
            modelEntry.iconName = entry.isDir ? QStringLiteral("folder") : QStringLiteral("file");
            modelEntries.append(modelEntry);
        }
    }

    m_matchingCount = modelEntries.size();
    m_model->setEntries(modelEntries);
    emit stateChanged();
}

void ArchiveBrowser::closeArchive()
{
    m_reader.reset();
    m_format = nullptr;
    m_archivePath.clear();
    m_password.clear();
    m_entries.clear();
    m_filterText.clear();
    m_model->setEntries({});
    m_matchingCount = 0;
    m_statusText.clear();
    m_archiveEncrypted = false;
    m_passwordRequired = false;
    emit filterTextChanged();
    emit stateChanged();
}

QString ArchiveBrowser::refresh()
{
    if (!m_reader || m_archivePath.isEmpty()) {
        return tr("No archive is open.");
    }
    const QString path = m_archivePath;
    const QString password = m_password;
    return openArchive(path, password);
}

QStringList ArchiveBrowser::indicesForSelection(const QStringList& archivePaths) const
{
    QStringList indices;
    for (const Entry& entry : m_entries) {
        for (const QString& selected : archivePaths) {
            const QString prefix = normalizePath(selected);
            if (entry.path == prefix || entry.path.startsWith(prefix + QLatin1Char('/'))) {
                indices.append(QString::number(entry.archiveIndex));
                break;
            }
        }
    }
    indices.removeDuplicates();
    return indices;
}

QString ArchiveBrowser::extractPaths(const QStringList& archivePaths, const QString& destinationFolder)
{
    if (!m_reader || !m_library || !m_format) {
        return tr("Open an archive first.");
    }
    if (archivePaths.isEmpty()) {
        return tr("Select one or more items first.");
    }

    const QStringList indexStrings = indicesForSelection(archivePaths);
    if (indexStrings.isEmpty()) {
        return tr("Nothing to extract.");
    }

    bit7z::IndicesVector indices;
    for (const QString& value : indexStrings) {
        indices.push_back(static_cast<std::uint32_t>(value.toUInt()));
    }

    if (!QDir().mkpath(destinationFolder)) {
        return tr("The folder %1 could not be created.").arg(destinationFolder);
    }

    try {
        m_reader->extractTo(destinationFolder.toStdString(), indices);
    } catch (const bit7z::BitException& exception) {
        if (exception.code() == bit7z::BitFailureSource::WrongPassword) {
            m_passwordRequired = true;
            m_archiveEncrypted = true;
            emit stateChanged();
            return m_password.isEmpty()
                ? tr("This archive is encrypted. Enter the password in the archive browser and try again.")
                : tr("The password does not open this archive.");
        }
        return tr("Extraction failed: %1").arg(exception.what());
    } catch (const std::exception& exception) {
        return tr("Extraction failed: %1").arg(exception.what());
    }

    return tr("Extracted %1 item(s) to %2.").arg(archivePaths.size()).arg(destinationFolder);
}

void ArchiveBrowser::copyPaths(const QStringList& archivePaths)
{
    if (archivePaths.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(archivePaths.join(QLatin1Char('\n')));
    m_statusText = tr("Copied %1 archive path(s) to the clipboard.").arg(archivePaths.size());
    emit stateChanged();
}

QString ArchiveBrowser::addFiles(const QStringList& localFiles)
{
    if (!m_reader || !m_library || !m_format || m_archivePath.isEmpty()) {
        return tr("Open an archive first.");
    }
    if (localFiles.isEmpty()) {
        return QString();
    }

    try {
        bit7z::BitArchiveWriter writer(*m_library, m_archivePath.toStdString(), *m_format,
                                       m_password.toStdString());
        std::vector<bit7z::tstring> paths;
        paths.reserve(static_cast<std::size_t>(localFiles.size()));
        for (const QString& file : localFiles) {
            paths.push_back(file.toStdString());
        }
        writer.addFiles(paths);
        writer.compressTo(m_archivePath.toStdString());
    } catch (const std::exception& exception) {
        return tr("Adding files failed: %1").arg(exception.what());
    }

    return refresh();
}