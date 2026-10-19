// SPDX-License-Identifier: MPL-2.0

#ifndef WEBSHAREPROVIDERPLUGIN_H
#define WEBSHAREPROVIDERPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include "interfaces/DestinationPluginInterface.h"

/**
 * One web file host as a destination.
 *
 * The provider is chosen at build time (see ABSAND_WEB_PROVIDER) so every host
 * shows up as its own entry in the destination list and keeps its own profiles:
 * Gofile, Litterbox, temp.sh, x0.at, catbox.moe, tmpfiles.org, pixeldrain, and a
 * custom endpoint.
 */
class WebShareProviderPlugin : public QObject, public DestinationPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID DestinationPluginInterface_IID FILE "webshare_plugin.json")
    Q_INTERFACES(DestinationPluginInterface)

public:
    explicit WebShareProviderPlugin(QObject* parent = nullptr);
    ~WebShareProviderPlugin() override = default;

    QString name() const override;
    QString version() const override { return "1.1.0"; }
    QString description() const override;
    QVariantMap metadata() const override;

    QString channelName() const override;
    QString configKey() const override;
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const override;
    QString suggestedLocalSaveFolder(const QVariantMap& config) const override;
    bool transferArchive(const QString& archivePath,
                         const QVariantMap& config,
                         TransferCallback resultCallback = nullptr) override;

private:
    QString providerId() const;

    QString m_providerId;
};

#endif // WEBSHAREPROVIDERPLUGIN_H
