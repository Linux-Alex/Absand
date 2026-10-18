// SPDX-License-Identifier: MPL-2.0

#ifndef FTPDESTINATIONPLUGIN_H
#define FTPDESTINATIONPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class FTPDestinationPlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "ftpdestination_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit FTPDestinationPlugin(QObject* parent = nullptr);
    ~FTPDestinationPlugin() override = default;

    QString name() const override { return tr("FTP"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Upload archives to an FTP server"); }
    QVariantMap metadata() const override;

    QString channelName() const override { return "ftp"; }
    QString configKey() const override { return "ftp"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         TransferCallback resultCallback = nullptr) override;
};

#endif // FTPDESTINATIONPLUGIN_H
