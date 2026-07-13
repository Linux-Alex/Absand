// SPDX-License-Identifier: MPL-2.0

#ifndef TELEGRAMPLUGIN_H
#define TELEGRAMPLUGIN_H

#include <QObject>
#include "interfaces/SendPluginInterface.h"

class TelegramPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "telegram_plugin.json")
    Q_INTERFACES(SendPluginInterface)
public:
    explicit TelegramPlugin(QObject* parent = nullptr) : QObject(parent) {}
    QString name() const override { return tr("Telegram"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Send the archive password with a Telegram bot"); }
    QVariantMap metadata() const override;
    void send(const QString&, const QVariantMap&, std::function<void(bool, QString)>) override;
    QString channelName() const override { return "telegram"; }
    QString configKey() const override { return "telegram"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    QWidget* createConfigWidget(QWidget* parent = nullptr) override;
    void loadConfigurationWidget(QWidget*, const QVariantMap&) const override;
    QVariantMap saveConfigurationWidget(QWidget*) const override;
    bool validateConfiguration(const QVariantMap&, QString* errorMessage = nullptr) const override;
};

#endif
