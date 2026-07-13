// SPDX-License-Identifier: MPL-2.0

#ifndef SENDSETTINGSDIALOG_H
#define SENDSETTINGSDIALOG_H

#include <QDialog>
#include <QHash>

class PluginManager;
class QTabWidget;
class SendPluginInterface;
class QWidget;

class SendSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SendSettingsDialog(PluginManager* manager, QWidget* parent = nullptr);
private slots:
    void accept() override;
private:
    struct Page { SendPluginInterface* plugin = nullptr; QWidget* widget = nullptr; };
    QVariantMap configForPlugin(SendPluginInterface* plugin) const;
    PluginManager* m_manager = nullptr;
    QTabWidget* m_tabs = nullptr;
    QHash<QString, Page> m_pages;
};

#endif
