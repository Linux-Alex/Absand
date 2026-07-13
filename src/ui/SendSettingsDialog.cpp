// SPDX-License-Identifier: MPL-2.0

#include "SendSettingsDialog.h"
#include "core/PluginManager.h"
#include "core/UserConfig.h"
#include "interfaces/SendPluginInterface.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QVBoxLayout>

SendSettingsDialog::SendSettingsDialog(PluginManager* manager, QWidget* parent)
    : QDialog(parent), m_manager(manager)
{
    setWindowTitle(tr("Password Delivery Settings"));
    resize(680, 430);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Configure the channels used to deliver an encrypted archive's password. Keep access tokens and webhook URLs private."), this);
    intro->setWordWrap(true); layout->addWidget(intro);
    m_tabs = new QTabWidget(this); layout->addWidget(m_tabs, 1);

    if (m_manager) {
        for (SendPluginInterface* plugin : m_manager->sendPlugins()) {
            if (!plugin || !plugin->requiresConfiguration()) continue;
            QWidget* page = plugin->createConfigWidget(m_tabs);
            plugin->loadConfigurationWidget(page, configForPlugin(plugin));
            m_tabs->addTab(page, plugin->name());
            m_pages.insert(plugin->configKey(), {plugin, page});
        }
    }
    if (m_tabs->count() == 0) m_tabs->addTab(new QLabel(tr("No configurable delivery plugins were loaded."), m_tabs), tr("Empty"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SendSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SendSettingsDialog::reject);
    layout->addWidget(buttons);
}

QVariantMap SendSettingsDialog::configForPlugin(SendPluginInterface* plugin) const
{
    QVariantMap config = plugin->defaultConfiguration();
    const QVariantMap stored = UserConfig::loadSendConfig(plugin->configKey());
    for (auto it = stored.cbegin(); it != stored.cend(); ++it) config[it.key()] = it.value();
    return config;
}

void SendSettingsDialog::accept()
{
    for (const Page& page : m_pages) {
        const QVariantMap config = page.plugin->saveConfigurationWidget(page.widget);
        QString error;
        const bool changedFromDefaults = config != page.plugin->defaultConfiguration();
        if (changedFromDefaults && !page.plugin->validateConfiguration(config, &error)) {
            QMessageBox::warning(this, tr("Invalid Configuration"), tr("%1: %2").arg(page.plugin->name(), error));
            return;
        }
        UserConfig::saveSendConfig(page.plugin->configKey(), config);
    }
    QDialog::accept();
}
