// SPDX-License-Identifier: MPL-2.0

#ifndef DESTINATIONPLUGININTERFACE_H
#define DESTINATIONPLUGININTERFACE_H

#include "PluginInterface.h"
#include "ConfigSchema.h"

#include "core/PluginConfigForm.h"

#include <QVariantMap>
#include <QWidget>

#include <functional>

/**
 * A target the finished archive is transferred to (folder, share, FTP, SFTP,
 * mail, web file host, ...).
 *
 * Plugins describe their settings with configSchema(); the default
 * implementation of the configuration widget helpers below renders that schema.
 */
class DestinationPluginInterface : public PluginInterface
{
public:
    virtual ~DestinationPluginInterface() = default;

    virtual QString channelName() const = 0;
    virtual QString configKey() const = 0;
    virtual bool requiresConfiguration() const = 0;
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

    virtual QString suggestedLocalSaveFolder(const QVariantMap& config) const = 0;

    /// Outcome of a transfer. shareLink is set when the archive was uploaded to a
    /// web file host, so it can be delivered together with the password.
    using TransferCallback = std::function<void(bool success, QString message, QString shareLink)>;

    virtual bool transferArchive(const QString& archivePath,
                                 const QVariantMap& config,
                                 TransferCallback resultCallback = nullptr) = 0;
};

#define DestinationPluginInterface_IID "com.absand.DestinationPluginInterface/1.0"
Q_DECLARE_INTERFACE(DestinationPluginInterface, DestinationPluginInterface_IID)

#endif // DESTINATIONPLUGININTERFACE_H