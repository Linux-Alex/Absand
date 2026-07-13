// SPDX-License-Identifier: MPL-2.0

#ifndef TEAMSPLUGIN_H
#define TEAMSPLUGIN_H
#include <QObject>
#include "interfaces/SendPluginInterface.h"
class TeamsPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "teams_plugin.json")
    Q_INTERFACES(SendPluginInterface)
public:
    explicit TeamsPlugin(QObject* parent = nullptr) : QObject(parent) {}
    QString name() const override { return tr("Microsoft Teams"); }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return tr("Send the archive password to a Teams workflow webhook"); }
    QVariantMap metadata() const override;
    void send(const QString&, const QVariantMap&, std::function<void(bool, QString)>) override;
    QString channelName() const override { return "teams"; }
    QString configKey() const override { return "teams"; }
    bool requiresConfiguration() const override { return true; }
    QVariantMap defaultConfiguration() const override;
    QWidget* createConfigWidget(QWidget* parent = nullptr) override;
    void loadConfigurationWidget(QWidget*, const QVariantMap&) const override;
    QVariantMap saveConfigurationWidget(QWidget*) const override;
    bool validateConfiguration(const QVariantMap&, QString* errorMessage = nullptr) const override;
};
#endif
