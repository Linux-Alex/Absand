// SPDX-License-Identifier: MPL-2.0

#ifndef EMAILPLUGIN_H
#define EMAILPLUGIN_H

#include <QObject>
#include "interfaces/SendPluginInterface.h"

class EmailPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "email_plugin.json")
    Q_INTERFACES(SendPluginInterface)

public:
    explicit EmailPlugin(QObject* parent = nullptr) : QObject(parent) {}

    QString name() const override { return tr("E-mail (SMTP)"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Send the archive password by e-mail through any SMTP server"); }
    QVariantMap metadata() const override;

    void send(const QString&, const QVariantMap&, std::function<void(bool, QString)>) override;
    QString channelName() const override { return "email"; }
    QString configKey() const override { return "email"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap&, QString* errorMessage = nullptr) const override;
};

#endif
