// SPDX-License-Identifier: MPL-2.0

#ifndef DESTINATIONPLUGININTERFACE_H
#define DESTINATIONPLUGININTERFACE_H

#include "PluginInterface.h"

#include <QVariantMap>
#include <functional>

class QWidget;

class DestinationPluginInterface : public PluginInterface
{
public:
    virtual ~DestinationPluginInterface() = default;

    virtual QString channelName() const = 0;
    virtual QString configKey() const = 0;
    virtual bool requiresConfiguration() const = 0;
    virtual QVariantMap defaultConfiguration() const = 0;
    virtual QWidget* createConfigWidget(QWidget* parent = nullptr) = 0;
    virtual void loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const = 0;
    virtual QVariantMap saveConfigurationWidget(QWidget* widget) const = 0;
    virtual bool validateConfiguration(const QVariantMap& config, QString* errorMessage = nullptr) const = 0;
    virtual QString suggestedLocalSaveFolder(const QVariantMap& config) const = 0;
    virtual bool transferArchive(const QString& archivePath,
                                 const QVariantMap& config,
                                 std::function<void(bool, QString)> resultCallback = nullptr) = 0;
};

#define DestinationPluginInterface_IID "com.absand.DestinationPluginInterface/1.0"
Q_DECLARE_INTERFACE(DestinationPluginInterface, DestinationPluginInterface_IID)

#endif // DESTINATIONPLUGININTERFACE_H
