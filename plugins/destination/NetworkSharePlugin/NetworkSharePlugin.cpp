// SPDX-License-Identifier: MPL-2.0

#include "NetworkSharePlugin.h"

#include "core/TransferHelpers.h"

#include <QDir>
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

QString defaultSharePath()
{
#ifdef Q_OS_WIN
    return QStringLiteral("//server/share");
#else
    return QStringLiteral("/mnt/share");
#endif
}

} // namespace

NetworkSharePlugin::NetworkSharePlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap NetworkSharePlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap NetworkSharePlugin::defaultConfiguration() const
{
    return {
        { "sharePath", defaultSharePath() },
        { "saveFolder", defaultSaveFolder() }
    };
}

ConfigSchema NetworkSharePlugin::configSchema() const
{
    return {
        { QStringLiteral("sharePath"), tr("Share path"),
          tr("UNC path such as //server/share or a mounted share"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::Directory, {},
          defaultSharePath(), ConfigField::Required | ConfigField::Recipient },
        { QStringLiteral("saveFolder"), tr("Default save folder"),
          tr("Where the archive is written before it is copied"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::Directory, {},
          defaultSaveFolder(), ConfigField::Required }
    };
}

QString NetworkSharePlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("saveFolder", defaultSaveFolder()).toString();
}

bool NetworkSharePlugin::transferArchive(const QString& archivePath,
                                         const QVariantMap& config,
                                         TransferCallback resultCallback)
{
    QString error;
    const QString sharePath = config.value("sharePath").toString();
    const bool ok = TransferHelpers::copyFileToFolder(archivePath, sharePath, &error);
    if (resultCallback) {
        resultCallback(ok, ok ? tr("Copied to network share") : error, QString());
    }
    return ok;
}
