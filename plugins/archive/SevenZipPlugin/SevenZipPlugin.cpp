// SPDX-License-Identifier: MPL-2.0

#include "SevenZipPlugin.h"
#include "core/SevenZipRuntime.h"

#include <bit7z/bit7z.hpp>
#include <bit7z/bitabstractarchivecreator.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <bit7z/bit7zlibrary.hpp>

#include <QDebug>
#include <QFileInfo>

#include <exception>

namespace {

std::vector<bit7z::tstring> toBit7zPaths(const QStringList& files)
{
    std::vector<bit7z::tstring> paths;
    paths.reserve(static_cast<std::size_t>(files.size()));
    for (const QString& file : files) {
        paths.push_back(file.toStdString());
    }
    return paths;
}

} // namespace

SevenZipPlugin::SevenZipPlugin(QObject* parent) : QObject(parent) {}

SevenZipPlugin::~SevenZipPlugin() = default;

bool SevenZipPlugin::createArchive(const QStringList& files,
                                   const QString& outputPath,
                                   const QString& password,
                                   std::function<void(int)> progressCallback)
{
    Q_UNUSED(progressCallback);

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

    const QString libraryPath = resolveSevenZipLibraryPath();
    if (libraryPath.isEmpty()) {
        qWarning() << "Could not find the 7-Zip shared library. Set ABSAND_7ZIP_LIBRARY or place 7z.so / 7z.dll next to the app.";
        return false;
    }

    qDebug() << "Using 7-Zip library:" << libraryPath;
    qDebug() << "Creating 7z archive:" << outputPath;

    try {
        bit7z::Bit7zLibrary library{ libraryPath.toStdString() };
        bit7z::BitFileCompressor compressor{ library, bit7z::BitFormat::SevenZip };

        if (!password.isEmpty()) {
            compressor.setPassword(password.toStdString(), bit7z::EncryptionScope::DataAndHeaders);
        }

        compressor.compress(toBit7zPaths(files), outputPath.toStdString());
        qDebug() << "Archive created successfully:" << outputPath;
        return true;
    } catch (const bit7z::BitException& ex) {
        qWarning() << "bit7z error:" << ex.what();
    } catch (const std::exception& ex) {
        qWarning() << "Standard exception:" << ex.what();
    } catch (...) {
        qWarning() << "Unknown error while creating 7z archive";
    }

    return false;
}
