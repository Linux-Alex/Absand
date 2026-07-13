// SPDX-License-Identifier: MPL-2.0

#ifndef DESTINATIONSETTINGSDIALOG_H
#define DESTINATIONSETTINGSDIALOG_H

#include <QDialog>
#include <QHash>
#include <QVariantMap>

class QTabWidget;
class QWidget;
class PluginManager;
class DestinationPluginInterface;

class DestinationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DestinationSettingsDialog(PluginManager* manager, QWidget* parent = nullptr);

private slots:
    void accept() override;

private:
    struct Page
    {
        DestinationPluginInterface* plugin = nullptr;
        QWidget* widget = nullptr;
    };

    void buildPages();
    QVariantMap configForPlugin(DestinationPluginInterface* plugin) const;

    PluginManager* m_manager = nullptr;
    QTabWidget* m_tabs = nullptr;
    QHash<QString, Page> m_pages;
};

#endif // DESTINATIONSETTINGSDIALOG_H
