// SPDX-License-Identifier: MPL-2.0

#include "ZipPlugin.h"
#include <zip.h>

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QVector>

#include <cstdio>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifndef ZIP_EM_AES_256
#define ZIP_EM_AES_256 0x0103
#endif

/**
 * libzip reaches the filesystem through the C library, so archive and input
 * paths must be handed over in the same narrow form that fopen() expects: the
 * ANSI code page on Windows (which is what libzip and the C runtime use) and
 * UTF-8 elsewhere. Converting a QString with toUtf8() directly, as this plugin
 * used to do, breaks every path containing characters outside ASCII on
 * Windows. Both possible encodings are therefore produced, and the first one
 * that really opens is used.
 */
namespace {

QList<QByteArray> pathCandidates(const QString& path)
{
    QList<QByteArray> candidates;
#ifdef Q_OS_WIN
    const auto* utf16 = reinterpret_cast<const wchar_t*>(path.utf16());
    const int size = WideCharToMultiByte(CP_ACP, 0, utf16, path.size(), nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        QByteArray ansi(size, Qt::Uninitialized);
        WideCharToMultiByte(CP_ACP, 0, utf16, path.size(), ansi.data(), size, nullptr, nullptr);
        candidates.append(ansi);
    }
#endif
    const QByteArray utf8 = path.toUtf8();
    if (!candidates.contains(utf8)) {
        candidates.append(utf8);
    }
    return candidates;
}

QByteArray readablePath(const QString& path)
{
    const QList<QByteArray> candidates = pathCandidates(path);
    for (const QByteArray& candidate : candidates) {
        if (FILE* probe = std::fopen(candidate.constData(), "rb")) {
            std::fclose(probe);
            return candidate;
        }
    }
    return candidates.first();
}

zip_t* openArchiveForWriting(const QString& archivePath)
{
    const QList<QByteArray> candidates = pathCandidates(archivePath);
    for (const QByteArray& candidate : candidates) {
        int error = 0;
        if (zip_t* archive = zip_open(candidate.constData(), ZIP_CREATE | ZIP_TRUNCATE, &error)) {
            return archive;
        }
        qWarning() << "Could not create archive with path encoding" << candidate << "error" << error;
    }
    return nullptr;
}

void freeProgressState(void* data)
{
    delete static_cast<std::function<void(int)>*>(data);
}

void reportProgress(zip_t* archive, double value, void* data)
{
    Q_UNUSED(archive);
    auto* callback = static_cast<std::function<void(int)>*>(data);
    if (callback && *callback) {
        // Adding entries accounts for the first 20 percent, compression for the rest.
        const int percent = 20 + static_cast<int>(value * 80.0);
        (*callback)(qBound(20, percent, 100));
    }
}

void installProgressCallback(zip_t* archive, const std::function<void(int)>& callback)
{
    if (!callback) {
        return;
    }
    zip_register_progress_callback_with_state(
        archive, 0.005, &reportProgress, &freeProgressState,
        new std::function<void(int)>(callback));
}

struct Entry
{
    QString sourcePath;
    QString archiveName;
    bool isDir = false;
};

QVector<Entry> collectEntries(const QStringList& files)
{
    QVector<Entry> entries;
    for (const QString& path : files) {
        const QFileInfo info(path);
        if (info.isDir()) {
            const QString root = info.fileName().isEmpty() ? path : info.fileName();
            entries.append({path, root, true});

            const QDir base(path);
            QDirIterator it(path,
                            QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                const QString relativePath = base.relativeFilePath(it.filePath());
                if (relativePath.isEmpty()) {
                    continue;
                }
                entries.append({it.filePath(), root + '/' + relativePath, it.fileInfo().isDir()});
            }
        } else {
            entries.append({path, info.fileName(), false});
        }
    }
    return entries;
}

} // namespace

ZipPlugin::ZipPlugin(QObject* parent) : QObject(parent) {}
ZipPlugin::~ZipPlugin() = default;

bool ZipPlugin::createArchive(const QStringList& files,
                              const QString& outputPath,
                              const QString& password,
                              std::function<void(int)> progressCallback)
{
    if (files.isEmpty()) {
        qWarning() << "No input files supplied";
        return false;
    }
    for (const QString& path : files) {
        if (!QFileInfo::exists(path)) {
            qWarning() << "Input path does not exist:" << path;
            return false;
        }
    }

    const QFileInfo outputInfo(outputPath);
    if (outputInfo.absolutePath().isEmpty() || !QDir().mkpath(outputInfo.absolutePath())) {
        qWarning() << "Cannot create the destination folder for" << outputPath;
        return false;
    }

    zip_t* archive = openArchiveForWriting(outputPath);
    if (!archive) {
        qWarning() << "Cannot create archive:" << outputPath;
        return false;
    }

    installProgressCallback(archive, progressCallback);
    if (progressCallback) {
        progressCallback(0);
    }

    const bool useEncryption = !password.isEmpty();
    const QByteArray passwordBytes = password.toUtf8();
    const auto encryptionMethod = static_cast<zip_uint16_t>(ZIP_EM_AES_256);

    const QVector<Entry> entries = collectEntries(files);
    const int total = static_cast<int>(entries.size());
    bool success = total > 0;

    for (int i = 0; success && i < total; ++i) {
        const Entry& entry = entries.at(i);
        const QByteArray name = entry.archiveName.toUtf8();
        if (name.isEmpty()) {
            continue;
        }

        zip_int64_t index = -1;
        if (entry.isDir) {
            const QByteArray dirName = name.endsWith('/') ? name : name + '/';
            index = zip_dir_add(archive, dirName.constData(), ZIP_FL_ENC_UTF_8);
        } else {
            zip_error_t zipError;
            zip_error_init(&zipError);
            zip_source_t* source = zip_source_file_create(
                readablePath(entry.sourcePath).constData(), 0, ZIP_LENGTH_UNCHECKED, &zipError);
            if (!source) {
                qWarning() << "Cannot read" << entry.sourcePath << ":"
                           << zip_error_strerror(&zipError);
                zip_error_fini(&zipError);
                success = false;
                break;
            }
            zip_error_fini(&zipError);

            index = zip_file_add(archive, name.constData(), source,
                                 ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE);
            if (index < 0) {
                zip_source_free(source);
                qWarning() << "Cannot add" << entry.archiveName << ":" << zip_strerror(archive);
                success = false;
                break;
            }
            if (useEncryption
                && zip_file_set_encryption(archive, static_cast<zip_uint64_t>(index),
                                           encryptionMethod, passwordBytes.constData()) != 0) {
                qWarning() << "Cannot encrypt" << entry.archiveName << ":" << zip_strerror(archive);
                success = false;
                break;
            }
        }

        if (index < 0) {
            qWarning() << "Cannot add archive entry" << entry.archiveName << ":" << zip_strerror(archive);
            success = false;
            break;
        }

        if (progressCallback) {
            progressCallback((i * 20) / qMax(1, total));
        }
    }

    if (!success) {
        zip_discard(archive);
        QFile::remove(outputPath);
        return false;
    }

    if (zip_close(archive) != 0) {
        qWarning() << "Cannot finish archive:" << outputPath;
        QFile::remove(outputPath);
        return false;
    }

    if (progressCallback) {
        progressCallback(100);
    }
    return true;
}
