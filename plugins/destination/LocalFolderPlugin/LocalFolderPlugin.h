// SPDX-License-Identifier: MPL-2.0

#ifndef LOCALFOLDERPLUGIN_H
#define LOCALFOLDERPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class LocalFolderPlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "localfolder_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit LocalFolderPlugin(QObject* parent = nullptr);
    ~LocalFolderPlugin() override = default;

    QString name() const override { return tr("Local Folder"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Save archives to a local folder"); }
    QVariantMap metadata() const override;

    QString channelName() const override { return "local_folder"; }
    QString configKey() const override { return "local_folder"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         TransferCallback resultCallback = nullptr) override;
};

#endif // LOCALFOLDERPLUGIN_H
