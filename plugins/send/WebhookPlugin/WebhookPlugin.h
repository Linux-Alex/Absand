// SPDX-License-Identifier: MPL-2.0

#ifndef WEBHOOKPLUGIN_H
#define WEBHOOKPLUGIN_H

#include <QObject>
#include "interfaces/SendPluginInterface.h"

class WebhookPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "webhook_plugin.json")
    Q_INTERFACES(SendPluginInterface)

public:
    explicit WebhookPlugin(QObject* parent = nullptr) : QObject(parent) {}

    QString name() const override { return tr("Webhook"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override {
        return tr("Send the password to Slack, Google Chat, Mattermost, Gotify, or any custom webhook");
    }
    QVariantMap metadata() const override;

    void send(const QString&, const QVariantMap&, std::function<void(bool, QString)>) override;
    QString channelName() const override { return "webhook"; }
    QString configKey() const override { return "webhook"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap&, QString* errorMessage = nullptr) const override;
};

#endif
