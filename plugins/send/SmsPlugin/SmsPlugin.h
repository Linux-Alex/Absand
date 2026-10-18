// SPDX-License-Identifier: MPL-2.0

#ifndef SMSPLUGIN_H
#define SMSPLUGIN_H

#include <QObject>
#include "interfaces/SendPluginInterface.h"

class SmsPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "sms_plugin.json")
    Q_INTERFACES(SendPluginInterface)

public:
    explicit SmsPlugin(QObject* parent = nullptr) : QObject(parent) {}

    QString name() const override { return tr("SMS"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override {
        return tr("Text the archive password through Twilio, Vonage, Textbelt, or any HTTP gateway");
    }
    QVariantMap metadata() const override;

    void send(const QString&, const QVariantMap&, std::function<void(bool, QString)>) override;
    QString channelName() const override { return "sms"; }
    QString configKey() const override { return "sms"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap&, QString* errorMessage = nullptr) const override;
};

#endif
