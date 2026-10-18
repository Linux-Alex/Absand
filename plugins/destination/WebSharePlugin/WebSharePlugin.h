// SPDX-License-Identifier: MPL-2.0

#ifndef WEBSHAREPLUGIN_H
#define WEBSHAREPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

/**
 * Uploads the finished archive to an anonymous web file host and reports the
 * share link, which can then be delivered together with the password.
 */
class WebSharePlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "webshare_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit WebSharePlugin(QObject* parent = nullptr);
    ~WebSharePlugin() override = default;

    QString name() const override { return tr("Web share"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override {
        return tr("Upload the archive to an anonymous web file host and share the download link");
    }
    QVariantMap metadata() const override;

    QString channelName() const override { return "web_share"; }
    QString configKey() const override { return "web_share"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         TransferCallback resultCallback = nullptr) override;
};

#endif // WEBSHAREPLUGIN_H
