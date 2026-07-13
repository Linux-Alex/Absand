// SPDX-License-Identifier: MPL-2.0

#ifndef FTPDESTINATIONPLUGIN_H
#define FTPDESTINATIONPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class QWidget;

class FTPDestinationPlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "ftpdestination_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit FTPDestinationPlugin(QObject* parent = nullptr);

    QString name() const override { return "FTP"; }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return "Upload archives to an FTP server"; }
    QVariantMap metadata() const override;

    QString channelName() const override { return "ftp"; }
    QString configKey() const override { return "ftp"; }
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

#endif // FTPDESTINATIONPLUGIN_H
