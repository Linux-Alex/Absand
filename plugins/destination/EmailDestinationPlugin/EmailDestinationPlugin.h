// SPDX-License-Identifier: MPL-2.0

#ifndef EMAILDESTINATIONPLUGIN_H
#define EMAILDESTINATIONPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class EmailDestinationPlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "emaildestination_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit EmailDestinationPlugin(QObject* parent = nullptr);
    ~EmailDestinationPlugin() override = default;

    QString name() const override { return tr("E-mail (SMTP)"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Send the finished archive as an e-mail attachment"); }
    QVariantMap metadata() const override;

    QString channelName() const override { return "email"; }
    QString configKey() const override { return "email_destination"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         TransferCallback resultCallback = nullptr) override;
};

#endif // EMAILDESTINATIONPLUGIN_H
