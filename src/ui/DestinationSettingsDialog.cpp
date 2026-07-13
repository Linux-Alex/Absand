// SPDX-License-Identifier: MPL-2.0

#include "DestinationSettingsDialog.h"

#include "core/PluginManager.h"
#include "core/UserConfig.h"
#include "interfaces/DestinationPluginInterface.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QVBoxLayout>

DestinationSettingsDialog::DestinationSettingsDialog(PluginManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Destination Settings"));
    resize(640, 420);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Configure how each destination plugin behaves and where it stores archives by default."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DestinationSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DestinationSettingsDialog::reject);
    layout->addWidget(buttons);

    buildPages();
}

void DestinationSettingsDialog::buildPages()
{
    if (!m_manager) {
        return;
    }

    const auto plugins = m_manager->destinationPlugins();
    for (DestinationPluginInterface* plugin : plugins) {
        if (!plugin) {
            continue;
        }

        QWidget* page = plugin->createConfigWidget(m_tabs);
        const QVariantMap config = configForPlugin(plugin);
        plugin->loadConfigurationWidget(page, config);
        m_tabs->addTab(page, plugin->name());
        m_pages.insert(plugin->configKey(), Page{ plugin, page });
    }

    if (m_tabs->count() == 0) {
        m_tabs->addTab(new QLabel(tr("No destination plugins were loaded."), m_tabs), tr("Empty"));
    }
}

QVariantMap DestinationSettingsDialog::configForPlugin(DestinationPluginInterface* plugin) const
{
    if (!plugin) {
        return {};
    }

    QVariantMap config = plugin->defaultConfiguration();
    const QVariantMap stored = UserConfig::loadDestinationConfig(plugin->configKey());
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        config[it.key()] = it.value();
    }
    return config;
}

void DestinationSettingsDialog::accept()
{
    if (!m_manager) {
        QDialog::accept();
        return;
    }

    for (auto it = m_pages.constBegin(); it != m_pages.constEnd(); ++it) {
        const Page& page = it.value();
        if (!page.plugin || !page.widget) {
            continue;
        }

        const QVariantMap config = page.plugin->saveConfigurationWidget(page.widget);
        QString error;
        if (!page.plugin->validateConfiguration(config, &error)) {
            QMessageBox::warning(this, tr("Invalid Configuration"),
                                 tr("%1: %2").arg(page.plugin->name(), error));
            return;
        }
        UserConfig::saveDestinationConfig(page.plugin->configKey(), config);
    }

    QDialog::accept();
}
