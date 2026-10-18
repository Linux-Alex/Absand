// SPDX-License-Identifier: MPL-2.0

#ifndef DESTINATIONSETTINGSDIALOG_H
#define DESTINATIONSETTINGSDIALOG_H

#include <QDialog>
#include <QVector>

class PluginManager;
class ProfilePage;
class QTabWidget;

class DestinationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DestinationSettingsDialog(PluginManager* manager, QWidget* parent = nullptr);

private slots:
    void accept() override;

private:
    PluginManager* m_manager = nullptr;
    QTabWidget* m_tabs = nullptr;
    QVector<ProfilePage*> m_pages;
};

#endif // DESTINATIONSETTINGSDIALOG_H
