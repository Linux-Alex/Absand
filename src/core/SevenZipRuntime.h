// SPDX-License-Identifier: MPL-2.0

#ifndef SEVENZIPRUNTIME_H
#define SEVENZIPRUNTIME_H

#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include <QFileInfo>

inline QString resolveSevenZipLibraryPath()
{
    const QString overridePath = qEnvironmentVariable("ABSAND_7ZIP_LIBRARY");
    if (!overridePath.isEmpty() && QFileInfo::exists(overridePath)) {
        return overridePath;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
#ifdef Q_OS_WIN
        appDir + "/7z.dll",
        appDir + "/../7z.dll",
        appDir + "/../../7z.dll",
        appDir + "/plugins/7z.dll",
#elif defined(Q_OS_MACOS)
        appDir + "/7z.so",
        appDir + "/../7z.so",
        appDir + "/../../7z.so",
        appDir + "/../Frameworks/7z.so",
        appDir + "/../Resources/7z.so",
#else
        appDir + "/7z.so",
        appDir + "/../lib/7z.so",
        appDir + "/../plugins/7z.so",
        appDir + "/../../7z.so",
        QStringLiteral("/usr/lib/7zip/7z.so"),
        QStringLiteral("/usr/lib/p7zip/7z.so"),
#endif
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return {};
}
#endif // SEVENZIPRUNTIME_H
