// SPDX-License-Identifier: MPL-2.0

#ifndef DISCORDPLUGIN_H
#define DISCORDPLUGIN_H

#include <QObject>
#include "interfaces/SendPluginInterface.h"

class DiscordPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "discord_plugin.json")
    Q_INTERFACES(SendPluginInterface)

public:
    explicit DiscordPlugin(QObject* parent = nullptr) : QObject(parent) {}

    QString name() const override { return tr("Discord"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Post the archive password through a Discord webhook"); }
    QVariantMap metadata() const override;

    void send(const QString&, const QVariantMap&, std::function<void(bool, QString)>) override;
    QString channelName() const override { return "discord"; }
    QString configKey() const override { return "discord"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap&, QString* errorMessage = nullptr) const override;
};

#endif
