// SPDX-License-Identifier: MPL-2.0

#ifndef CLIPBOARDPLUGIN_H
#define CLIPBOARDPLUGIN_H

#include <QObject>
#include <QVariantMap>
#include <functional>
#include "interfaces/SendPluginInterface.h"

class ClipboardPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "clipboard_plugin.json")
    Q_INTERFACES(SendPluginInterface)

public:
    explicit ClipboardPlugin(QObject* parent = nullptr);
    
    // PluginInterface
    QString name() const override { return "Clipboard"; }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return "Copy password to system clipboard"; }
    QVariantMap metadata() const override {
        QVariantMap meta;
        meta["name"] = name();
        meta["version"] = version();
        meta["description"] = description();
        meta["author"] = "Absand Team";
        meta["channel"] = channelName();
        return meta;
    }
    
    // SendPluginInterface
    void send(const QString& password,
              const QVariantMap& config,
              std::function<void(bool, QString)> resultCallback = nullptr) override;
    
    QString channelName() const override { return "clipboard"; }
    QString configKey() const override { return "clipboard"; }
    bool requiresConfiguration() const override { return false; }
    QVariantMap defaultConfiguration() const override { return QVariantMap(); }
    bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const override {
        Q_UNUSED(config); 
        Q_UNUSED(errorMessage);
        return true; 
    }
    QWidget* createConfigWidget(QWidget* parent = nullptr) override {
        Q_UNUSED(parent);
        return nullptr;
    }
    void loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const override {
        Q_UNUSED(widget);
        Q_UNUSED(config);
    }
    QVariantMap saveConfigurationWidget(QWidget* widget) const override {
        Q_UNUSED(widget);
        return {};
    }
};

#endif // CLIPBOARDPLUGIN_H
