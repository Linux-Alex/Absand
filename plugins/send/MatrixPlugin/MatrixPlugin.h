// SPDX-License-Identifier: MPL-2.0

#ifndef MATRIXPLUGIN_H
#define MATRIXPLUGIN_H
#include <QObject>
#include "interfaces/SendPluginInterface.h"
class MatrixPlugin : public QObject, public SendPluginInterface
{
    Q_OBJECT Q_PLUGIN_METADATA(IID SendPluginInterface_IID FILE "matrix_plugin.json") Q_INTERFACES(SendPluginInterface)
public:
    explicit MatrixPlugin(QObject* p=nullptr):QObject(p){} QString name() const override{return tr("Matrix");} QString version() const override{return "1.0.0";} QString description() const override{return tr("Send the archive password to a Matrix room");} QVariantMap metadata() const override;
    void send(const QString&,const QVariantMap&,std::function<void(bool,QString)>) override; QString channelName() const override{return "matrix";} QString configKey() const override{return "matrix";} bool requiresConfiguration() const override{return true;} QVariantMap defaultConfiguration() const override; QWidget* createConfigWidget(QWidget* parent = nullptr) override; void loadConfigurationWidget(QWidget*,const QVariantMap&) const override; QVariantMap saveConfigurationWidget(QWidget*) const override; bool validateConfiguration(const QVariantMap&,QString* errorMessage = nullptr) const override;
};
#endif
