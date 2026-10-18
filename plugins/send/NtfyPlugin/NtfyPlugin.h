// SPDX-License-Identifier: MPL-2.0

#ifndef NTFYPLUGIN_H
#define NTFYPLUGIN_H

#include <QObject>
#include "interfaces/SendPluginInterface.h"

class NtfyPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "ntfy_plugin.json")
    Q_INTERFACES(SendPluginInterface)

public:
    explicit NtfyPlugin(QObject* parent = nullptr) : QObject(parent) {}

    QString name() const override { return tr("ntfy push"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override {
        return tr("Push the archive password to a free ntfy topic (no account needed)");
    }
    QVariantMap metadata() const override;

    void send(const QString&, const QVariantMap&, std::function<void(bool, QString)>) override;
    QString channelName() const override { return "ntfy"; }
    QString configKey() const override { return "ntfy"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    ConfigSchema configSchema() const override;
    bool validateConfiguration(const QVariantMap&, QString* errorMessage = nullptr) const override;
};

#endif
