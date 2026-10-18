// SPDX-License-Identifier: MPL-2.0

#ifndef NETWORKSHAREPLUGIN_H
#define NETWORKSHAREPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class NetworkSharePlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "networkshare_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit NetworkSharePlugin(QObject* parent = nullptr);
    ~NetworkSharePlugin() override = default;

    QString name() const override { return tr("Network Share"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Save archives to a network share or mounted share"); }
    QVariantMap metadata() const override;

    QString channelName() const override { return "network_share"; }
    QString configKey() const override { return "network_share"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         TransferCallback resultCallback = nullptr) override;
};

#endif // NETWORKSHAREPLUGIN_H
