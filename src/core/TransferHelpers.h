// SPDX-License-Identifier: MPL-2.0

#ifndef TRANSFERHELPERS_H
#define TRANSFERHELPERS_H

#include <QString>
#include <QStringList>

namespace TransferHelpers {

// Default upper bound for a single network transfer, so a stalled connection can
// never leave the application waiting forever.
constexpr int DefaultTransferTimeoutMs = 30 * 60 * 1000;

bool copyFileToFolder(const QString& sourcePath, const QString& targetFolder, QString* errorMessage = nullptr);
bool uploadWithCurl(const QStringList& arguments, QString* errorMessage = nullptr,
                    int timeoutMs = DefaultTransferTimeoutMs);
QString resolveCurlExecutable();
QString buildTargetUrl(const QString& scheme, const QString& host, int port, const QString& remotePath, const QString& fileName);
int defaultPortForScheme(const QString& scheme);

}

#endif // TRANSFERHELPERS_H
