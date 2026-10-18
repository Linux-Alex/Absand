// SPDX-License-Identifier: MPL-2.0

#include "FTPDestinationPlugin.h"

#include "core/TransferHelpers.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

QString defaultSaveFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

} // namespace

FTPDestinationPlugin::FTPDestinationPlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap FTPDestinationPlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap FTPDestinationPlugin::defaultConfiguration() const
{
    return {
        { "host", QString() },
        { "port", 21 },
        { "username", QString() },
        { "password", QString() },
        { "remotePath", QStringLiteral("/") },
        { "saveFolder", defaultSaveFolder() }
    };
}

ConfigSchema FTPDestinationPlugin::configSchema() const
{
    return {
        { QStringLiteral("host"), tr("Host"), tr("Server name or address"), QStringLiteral("ftp.example.com"),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant(), ConfigField::Required },
        { QStringLiteral("port"), tr("Port"), QString(), QStringLiteral("21"),
          ConfigField::Type::Integer, ConfigField::Browse::None, {}, 21, ConfigField::Required },
        { QStringLiteral("username"), tr("Username"), QString(), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("password"), tr("Password"), QString(), QString(),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("remotePath"), tr("Remote folder"), tr("Folder on the server that receives the archive"),
          QStringLiteral("/"), ConfigField::Type::Text, ConfigField::Browse::None, {},
          QStringLiteral("/"), ConfigField::Recipient },
        { QStringLiteral("saveFolder"), tr("Default save folder"),
          tr("Where the archive is written before it is uploaded"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::Directory, {},
          defaultSaveFolder(), ConfigField::Required }
    };
}

bool FTPDestinationPlugin::validateConfiguration(const QVariantMap& config, QString* errorMessage) const
{
    if (config.value(QStringLiteral("host")).toString().trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Please enter the FTP host.");
        }
        return false;
    }

    const int port = config.value(QStringLiteral("port"), 21).toInt();
    if (port <= 0 || port > 65535) {
        if (errorMessage) {
            *errorMessage = tr("Please enter a port between 1 and 65535.");
        }
        return false;
    }

    return true;
}

QString FTPDestinationPlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("saveFolder", defaultSaveFolder()).toString();
}

bool FTPDestinationPlugin::transferArchive(const QString& archivePath,
                                           const QVariantMap& config,
                                           TransferCallback resultCallback)
{
    const QString host = config.value("host").toString().trimmed();
    const int port = config.value("port", 21).toInt();
    const QString username = config.value("username").toString().trimmed();
    const QString password = config.value("password").toString();
    const QString remotePath = config.value("remotePath", QStringLiteral("/")).toString();
    const QString fileName = QFileInfo(archivePath).fileName();

    QStringList args;
    args << "-T" << archivePath;
    // Create missing remote directories instead of failing the whole upload.
    args << "--ftp-create-dirs";
    if (!username.isEmpty() || !password.isEmpty()) {
        args << "--user" << (username + ":" + password);
    }
    args << TransferHelpers::buildTargetUrl("ftp", host, port, remotePath, fileName);

    QString error;
    const bool ok = TransferHelpers::uploadWithCurl(args, &error);
    if (resultCallback) {
        resultCallback(ok, ok ? tr("Uploaded to FTP server") : error, QString());
    }
    return ok;
}
