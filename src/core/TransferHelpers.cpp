// SPDX-License-Identifier: MPL-2.0

#include "TransferHelpers.h"

#include "CurlExecutable.h"
#include "ProcessWait.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace TransferHelpers {

int defaultPortForScheme(const QString& scheme)
{
    const QString lowered = scheme.trimmed().toLower();
    if (lowered == QStringLiteral("sftp") || lowered == QStringLiteral("ssh")
        || lowered == QStringLiteral("scp") || lowered == QStringLiteral("ftps")) {
        return 22;
    }
    return 21;
}

QString resolveCurlExecutable()
{
    return CurlExecutable::resolve();
}

bool copyFileToFolder(const QString& sourcePath, const QString& targetFolder, QString* errorMessage)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source file does not exist: %1").arg(sourcePath);
        }
        return false;
    }

    QDir dir(targetFolder);
    if (!dir.exists() && !QDir().mkpath(targetFolder)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create target folder: %1").arg(targetFolder);
        }
        return false;
    }

    const QString destinationPath = dir.filePath(sourceInfo.fileName());
    const QFileInfo destinationInfo(destinationPath);
    if (destinationInfo.exists()
        && destinationInfo.canonicalFilePath() == sourceInfo.canonicalFilePath()) {
        return true;
    }

    // Copy to a sibling temporary file first, so a failure half way through a slow
    // network share can never destroy the previously delivered archive.
    const QString temporaryPath = destinationPath + QStringLiteral(".absand-part");
    QFile::remove(temporaryPath);

    if (!QFile::copy(sourcePath, temporaryPath)) {
        QFile::remove(temporaryPath);
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to copy %1 to %2").arg(sourcePath, destinationPath);
        }
        return false;
    }

    if (QFileInfo(temporaryPath).size() != sourceInfo.size()) {
        QFile::remove(temporaryPath);
        if (errorMessage) {
            *errorMessage = QStringLiteral("The copy of %1 to %2 is incomplete")
                                .arg(sourcePath, destinationPath);
        }
        return false;
    }

    QFile::remove(destinationPath);
    if (!QFile::rename(temporaryPath, destinationPath)) {
        // Some network shares refuse renames; fall back to a direct copy.
        QFile::remove(destinationPath);
        const bool copied = QFile::copy(temporaryPath, destinationPath);
        QFile::remove(temporaryPath);
        if (!copied) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to copy %1 to %2").arg(sourcePath, destinationPath);
            }
            return false;
        }
    }

    return true;
}

bool uploadWithCurl(const QStringList& arguments, QString* errorMessage, int timeoutMs)
{
    const QString curl = resolveCurlExecutable();

    QStringList finalArguments;
    // Fail quickly on unreachable hosts instead of waiting for the TCP timeout.
    finalArguments << QStringLiteral("--connect-timeout") << QStringLiteral("20");
    finalArguments << arguments;

    QProcess process;
    process.start(curl, finalArguments);
    if (!process.waitForStarted(3000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not start curl (%1): %2")
                                .arg(curl, process.errorString());
        }
        return false;
    }

    if (!ProcessWait::waitFinished(process, timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        if (errorMessage) {
            *errorMessage = QStringLiteral("The upload timed out after %1 minute(s).")
                                .arg(qMax(1, timeoutMs / 60000));
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = QStringLiteral("curl failed with exit code %1").arg(process.exitCode());
            }
        }
        return false;
    }
    return true;
}

QString buildTargetUrl(const QString& scheme, const QString& host, int port,
                       const QString& remotePath, const QString& fileName)
{
    QString hostName = host.trimmed();

    // Accept values pasted from a browser or file manager ("ftp://example.com",
    // "sftp://user@example.com:2222/") in addition to a bare host name.
    const int schemeSeparator = hostName.indexOf(QStringLiteral("://"));
    if (schemeSeparator >= 0) {
        hostName = hostName.mid(schemeSeparator + 3);
    }
    const int userSeparator = hostName.lastIndexOf(QLatin1Char('@'));
    if (userSeparator >= 0) {
        hostName = hostName.mid(userSeparator + 1);
    }
    while (hostName.endsWith(QLatin1Char('/'))) {
        hostName.chop(1);
    }

    const int colon = hostName.lastIndexOf(QLatin1Char(':'));
    if (colon > 0 && hostName.indexOf(QLatin1Char(':')) == colon) {
        bool ok = false;
        const int hostPort = hostName.mid(colon + 1).toInt(&ok);
        if (ok && hostPort > 0 && hostPort <= 65535) {
            port = hostPort;
            hostName.truncate(colon);
        }
    }

    if (port <= 0 || port > 65535) {
        port = defaultPortForScheme(scheme);
    }

    QString remote = remotePath.trimmed();
    if (remote.isEmpty()) {
        remote = QStringLiteral("/");
    }
    if (!remote.startsWith(QLatin1Char('/'))) {
        remote.prepend(QLatin1Char('/'));
    }

    // Spaces and non-ASCII characters are not valid in a URL, so every path
    // segment has to be percent-encoded before curl sees it.
    QString encodedPath;
    const QStringList segments = remote.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& segment : segments) {
        encodedPath += QLatin1Char('/');
        encodedPath += QString::fromLatin1(QUrl::toPercentEncoding(segment));
    }
    encodedPath += QLatin1Char('/');
    encodedPath += QString::fromLatin1(QUrl::toPercentEncoding(fileName));

    return QStringLiteral("%1://%2:%3%4").arg(scheme, hostName, QString::number(port), encodedPath);
}

} // namespace TransferHelpers