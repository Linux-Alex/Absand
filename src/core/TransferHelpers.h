// SPDX-License-Identifier: MPL-2.0

#ifndef TRANSFERHELPERS_H
#define TRANSFERHELPERS_H

#include <QString>
#include <QStringList>

namespace TransferHelpers {

bool copyFileToFolder(const QString& sourcePath, const QString& targetFolder, QString* errorMessage = nullptr);
bool uploadWithCurl(const QStringList& arguments, QString* errorMessage = nullptr);
QString buildTargetUrl(const QString& scheme, const QString& host, int port, const QString& remotePath, const QString& fileName);

}

#endif // TRANSFERHELPERS_H
