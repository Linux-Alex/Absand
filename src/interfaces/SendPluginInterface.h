// SPDX-License-Identifier: MPL-2.0

#ifndef SENDPLUGININTERFACE_H
#define SENDPLUGININTERFACE_H

#include "PluginInterface.h"
#include "ConfigSchema.h"

#include "core/PluginConfigForm.h"

#include <QVariantMap>
#include <QWidget>

#include <functional>

/**
 * A channel that delivers the archive password to the recipient.
 *
 * Plugins describe their settings with configSchema(); the default
 * implementation of the configuration widget helpers below renders that schema,
 * so plugins normally do not need any UI code at all.
 */
class SendPluginInterface : public PluginInterface
{
public:
    virtual ~SendPluginInterface() = default;

    // Core send operation
    virtual void send(const QString& password,
                      const QVariantMap& config,
                      std::function<void(bool, QString)> resultCallback = nullptr) = 0;

    // Channel specific
    virtual QString channelName() const = 0;
    virtual QString configKey() const = 0;
    virtual bool requiresConfiguration() const = 0;

    // Configuration
    virtual QVariantMap defaultConfiguration() const = 0;
    virtual ConfigSchema configSchema() const { return {}; }

    virtual QWidget* createConfigWidget(QWidget* parent = nullptr)
    {
        return PluginConfigForm::createWidget(parent, configSchema());
    }
    virtual void loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const
    {
        PluginConfigForm::load(widget, configSchema(), config);
    }
    virtual QVariantMap saveConfigurationWidget(QWidget* widget) const
    {
        return PluginConfigForm::save(widget, configSchema());
    }

    virtual bool validateConfiguration(const QVariantMap& config,
                                       QString* errorMessage = nullptr) const
    {
        return PluginConfigForm::validate(configSchema(), config, errorMessage);
    }
};

#define SendPluginInterface_IID "com.absand.SendPluginInterface/1.0"
Q_DECLARE_INTERFACE(SendPluginInterface, SendPluginInterface_IID)

#endif // SENDPLUGININTERFACE_H
