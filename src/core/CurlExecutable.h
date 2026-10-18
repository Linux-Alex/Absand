// SPDX-License-Identifier: MPL-2.0

#ifndef CURLEXECUTABLE_H
#define CURLEXECUTABLE_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

namespace CurlExecutable {

/**
 * Locates the curl binary Absand should run.
 *
 * Network transfers and password delivery both go through curl. On Windows the
 * installer ships a recent curl next to Absand.exe, but the Windows directory
 * also contains an older system curl.exe, so the bundled copy has to be
 * preferred explicitly instead of relying on the default PATH lookup.
 */
inline QString resolve()
{
#ifdef Q_OS_WIN
    const QString executableName = QStringLiteral("curl.exe");
#else
    const QString executableName = QStringLiteral("curl");
#endif

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList bundled{
        appDir.filePath(executableName),
        appDir.filePath(QStringLiteral("../") + executableName),
    };
    for (const QString& candidate : bundled) {
        const QFileInfo info(candidate);
        if (info.isFile() && info.isExecutable()) {
            return info.canonicalFilePath();
        }
    }

    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (!onPath.isEmpty()) {
        return onPath;
    }

    return QStringLiteral("curl");
}

} // namespace CurlExecutable

#endif // CURLEXECUTABLE_H
