// SPDX-License-Identifier: MPL-2.0

#ifndef LOCALFOLDERPLUGIN_H
#define LOCALFOLDERPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

class QLineEdit;
class QWidget;

class LocalFolderPlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "localfolder_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit LocalFolderPlugin(QObject* parent = nullptr);

    QString name() const override { return "Local Folder"; }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return "Save archives to a local folder"; }
    QVariantMap metadata() const override;

    QString channelName() const override { return "local_folder"; }
    QString configKey() const override { return "local_folder"; }
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

#endif // LOCALFOLDERPLUGIN_H
