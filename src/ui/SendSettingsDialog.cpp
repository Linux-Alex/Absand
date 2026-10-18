// SPDX-License-Identifier: MPL-2.0

#include "SendSettingsDialog.h"

#include "ProfilePage.h"
#include "core/PluginManager.h"
#include "core/UserConfig.h"
#include "interfaces/SendPluginInterface.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QVBoxLayout>

SendSettingsDialog::SendSettingsDialog(PluginManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Password Delivery Settings"));
    resize(720, 520);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Configure the channels used to deliver an encrypted archive's password. "
                                "Each channel can keep several profiles, for example one per mail server or "
                                "recipient. Keep access tokens and webhook URLs private."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs, 1);

    if (m_manager) {
        for (SendPluginInterface* plugin : m_manager->sendPlugins()) {
            if (!plugin || !plugin->requiresConfiguration()) {
                continue;
            }

            ProfilePage::Access access;
            access.kind = UserConfig::ConfigKind::Send;
            access.name = [plugin] { return plugin->name(); };
            access.configKey = [plugin] { return plugin->configKey(); };
            access.defaults = [plugin] { return plugin->defaultConfiguration(); };
            access.createWidget = [plugin](QWidget* parent) { return plugin->createConfigWidget(parent); };
            access.load = [plugin](QWidget* widget, const QVariantMap& config) {
                plugin->loadConfigurationWidget(widget, config);
            };
            access.save = [plugin](QWidget* widget) { return plugin->saveConfigurationWidget(widget); };
            access.validate = [plugin](const QVariantMap& config, QString* error) {
                return plugin->validateConfiguration(config, error);
            };
            access.schema = plugin->configSchema();

            auto* page = new ProfilePage(access, m_tabs);
            m_pages.append(page);
            m_tabs->addTab(page, plugin->name());
        }
    }

    if (m_tabs->count() == 0) {
        m_tabs->addTab(new QLabel(tr("No configurable delivery plugins were loaded."), m_tabs), tr("Empty"));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SendSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SendSettingsDialog::reject);
    layout->addWidget(buttons);
}

void SendSettingsDialog::accept()
{
    for (ProfilePage* page : m_pages) {
        QString error;
        if (!page->commit(&error)) {
            QMessageBox::warning(this, tr("Invalid Configuration"), error);
            return;
        }
    }

    for (ProfilePage* page : m_pages) {
        page->persist();
    }

    QDialog::accept();
}
