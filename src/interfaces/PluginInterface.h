// SPDX-License-Identifier: MPL-2.0

#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include <QtPlugin>
#include <QString>
#include <QVariantMap>

class PluginInterface
{
public:
    virtual ~PluginInterface() = default;
    
    // Required: Every plugin must return these
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString description() const = 0;
    
    // Optional: Get raw metadata from the plugin's JSON
    virtual QVariantMap metadata() const = 0;
};

#define PluginInterface_IID "com.absand.PluginInterface/1.0"
Q_DECLARE_INTERFACE(PluginInterface, PluginInterface_IID)

#endif // PLUGININTERFACE_H