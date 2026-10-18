// SPDX-License-Identifier: MPL-2.0

#include "LocalFolderPlugin.h"

#include "core/TransferHelpers.h"

#include <QDir>
#include <QStandardPaths>

namespace {

QString defaultFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

} // namespace

LocalFolderPlugin::LocalFolderPlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap LocalFolderPlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap LocalFolderPlugin::defaultConfiguration() const
{
    return { { "targetFolder", defaultFolder() } };
}

ConfigSchema LocalFolderPlugin::configSchema() const
{
    return {
        { QStringLiteral("targetFolder"), tr("Target folder"), tr("Folder the archive is copied to"),
          defaultFolder(), ConfigField::Type::Text, ConfigField::Browse::Directory, {},
          defaultFolder(), ConfigField::Required | ConfigField::Recipient }
    };
}

QString LocalFolderPlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("targetFolder", defaultFolder()).toString();
}

bool LocalFolderPlugin::transferArchive(const QString& archivePath,
                                        const QVariantMap& config,
                                        TransferCallback resultCallback)
{
    QString error;
    const QString folder = suggestedLocalSaveFolder(config);
    const bool ok = TransferHelpers::copyFileToFolder(archivePath, folder, &error);
    if (resultCallback) {
        resultCallback(ok, ok ? tr("Saved to %1").arg(folder) : error, QString());
    }
    return ok;
}
