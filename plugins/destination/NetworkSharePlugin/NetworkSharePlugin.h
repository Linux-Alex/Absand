// SPDX-License-Identifier: MPL-2.0

#ifndef NETWORKSHAREPLUGIN_H
#define NETWORKSHAREPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class QWidget;

class NetworkSharePlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "networkshare_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit NetworkSharePlugin(QObject* parent = nullptr);

    QString name() const override { return "Network Share"; }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return "Save archives to a network share or mounted share"; }
    QVariantMap metadata() const override;

    QString channelName() const override { return "network_share"; }
    QString configKey() const override { return "network_share"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    QWidget* createConfigWidget(QWidget* parent = nullptr) override;
    void loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const override;
    QVariantMap saveConfigurationWidget(QWidget* widget) const override;
    bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         std::function<void(bool, QString)> resultCallback = nullptr) override;
};

#endif // NETWORKSHAREPLUGIN_H
