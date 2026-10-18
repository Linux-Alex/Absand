// SPDX-License-Identifier: MPL-2.0

#ifndef SENDSETTINGSDIALOG_H
#define SENDSETTINGSDIALOG_H

#include <QDialog>
#include <QVector>

class PluginManager;
class ProfilePage;
class QTabWidget;

class SendSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendSettingsDialog(PluginManager* manager, QWidget* parent = nullptr);

private slots:
    void accept() override;

private:
    PluginManager* m_manager = nullptr;
    QTabWidget* m_tabs = nullptr;
    QVector<ProfilePage*> m_pages;
};

#endif // SENDSETTINGSDIALOG_H
