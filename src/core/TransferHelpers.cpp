// SPDX-License-Identifier: MPL-2.0

#include "TransferHelpers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace TransferHelpers {

bool copyFileToFolder(const QString& sourcePath, const QString& targetFolder, QString* errorMessage)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source file does not exist: %1").arg(sourcePath);
        }
        return false;
    }

    QDir dir(targetFolder);
    if (!dir.exists()) {
        if (!QDir().mkpath(targetFolder)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not create target folder: %1").arg(targetFolder);
            }
            return false;
        }
    }

    const QString destinationPath = dir.filePath(sourceInfo.fileName());
    if (QFileInfo(destinationPath).exists() && QFileInfo(destinationPath).canonicalFilePath() == sourceInfo.canonicalFilePath()) {
        return true;
    }
    if (QFile::exists(destinationPath)) {
        QFile::remove(destinationPath);
    }
    if (!QFile::copy(sourcePath, destinationPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to copy to %1").arg(destinationPath);
        }
        return false;
    }
    return true;
}

bool uploadWithCurl(const QStringList& arguments, QString* errorMessage)
{
    QProcess process;
    process.start(QStringLiteral("curl"), arguments);
    if (!process.waitForStarted(3000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not start curl.");
        }
        return false;
    }
    if (!process.waitForFinished(-1)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("curl did not finish.");
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

QString buildTargetUrl(const QString& scheme, const QString& host, int port, const QString& remotePath, const QString& fileName)
{
    QString path = remotePath.trimmed();
    if (!path.startsWith('/')) {
        path.prepend('/');
    }
    if (!path.endsWith('/')) {
        path.append('/');
    }
    path.append(fileName);
    return QStringLiteral("%1://%2:%3%4").arg(scheme, host).arg(port).arg(path);
}

}
