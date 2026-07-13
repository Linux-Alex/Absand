// SPDX-License-Identifier: MPL-2.0

#ifndef SENDPLUGININTERFACE_H
#define SENDPLUGININTERFACE_H

#include "PluginInterface.h"
#include <QVariantMap>
#include <functional>

class QWidget;

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
    virtual QWidget* createConfigWidget(QWidget* parent = nullptr) = 0;
    virtual void loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const = 0;
    virtual QVariantMap saveConfigurationWidget(QWidget* widget) const = 0;
    virtual bool validateConfiguration(const QVariantMap& config,
                                       QString* errorMessage = nullptr) const = 0;
};

#define SendPluginInterface_IID "com.absand.SendPluginInterface/1.0"
Q_DECLARE_INTERFACE(SendPluginInterface, SendPluginInterface_IID)

#endif // SENDPLUGININTERFACE_H
