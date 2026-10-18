// SPDX-License-Identifier: MPL-2.0

#ifndef SFTPDESTINATIONPLUGIN_H
#define SFTPDESTINATIONPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class SFTPDestinationPlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "sftpdestination_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit SFTPDestinationPlugin(QObject* parent = nullptr);
    ~SFTPDestinationPlugin() override = default;

    QString name() const override { return tr("SFTP"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Upload archives to an SFTP server"); }
    QVariantMap metadata() const override;

    QString channelName() const override { return "sftp"; }
    QString configKey() const override { return "sftp"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         TransferCallback resultCallback = nullptr) override;
};

#endif // SFTPDESTINATIONPLUGIN_H
