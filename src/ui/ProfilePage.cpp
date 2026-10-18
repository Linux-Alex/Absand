// SPDX-License-Identifier: MPL-2.0

#include "ProfilePage.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

ProfilePage::ProfilePage(const Access& access, QWidget* parent)
    : QWidget(parent)
    , m_access(access)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* selectorRow = new QHBoxLayout();
    selectorRow->setSpacing(6);

    auto* label = new QLabel(tr("Profile:"), this);
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_profileCombo->setMinimumWidth(180);

    m_newButton = new QPushButton(tr("New"), this);
    m_newButton->setToolTip(tr("Add a profile copied from the current one"));
    m_renameButton = new QPushButton(tr("Rename"), this);
    m_renameButton->setToolTip(tr("Rename the current profile"));
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_deleteButton->setToolTip(tr("Delete the current profile"));

    selectorRow->addWidget(label);
    selectorRow->addWidget(m_profileCombo, 1);
    selectorRow->addWidget(m_newButton);
    selectorRow->addWidget(m_renameButton);
    selectorRow->addWidget(m_deleteButton);
    layout->addLayout(selectorRow);

    m_form = m_access.createWidget(this);
    layout->addWidget(m_form);
    layout->addStretch(1);

    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfilePage::onProfileChosen);
    connect(m_newButton, &QPushButton::clicked, this, &ProfilePage::onProfileCreated);
    connect(m_renameButton, &QPushButton::clicked, this, &ProfilePage::onProfileRenamed);
    connect(m_deleteButton, &QPushButton::clicked, this, &ProfilePage::onProfileDeleted);

    loadProfiles();
}

QString ProfilePage::pluginLabel() const
{
    return m_access.name ? m_access.name() : QString();
}

void ProfilePage::loadProfiles()
{
    const QString key = m_access.configKey();
    const QStringList names = UserConfig::profileNames(m_access.kind, key);

    m_profiles.clear();
    m_storedProfiles.clear();
    for (const QString& name : names) {
        m_profiles.insert(name, UserConfig::loadProfile(m_access.kind, key, name));
    }

    if (m_profiles.isEmpty()) {
        m_profiles.insert(UserConfig::defaultProfileName(), m_access.defaults());
    }

    m_active = UserConfig::activeProfile(m_access.kind, key);
    if (!m_profiles.contains(m_active)) {
        m_active = m_profiles.firstKey();
    }

    m_storedProfiles = m_profiles;
    refreshSelector();
    loadFormFromActive();
}

void ProfilePage::refreshSelector()
{
    const QSignalBlocker blocker(m_profileCombo);
    m_profileCombo->clear();
    m_profileCombo->addItems(m_profiles.keys());
    const int index = m_profileCombo->findText(m_active);
    m_profileCombo->setCurrentIndex(index >= 0 ? index : 0);

    const bool multiple = m_profiles.size() > 1;
    m_renameButton->setEnabled(multiple);
    m_deleteButton->setEnabled(multiple);
}

void ProfilePage::loadFormFromActive()
{
    if (!m_form) {
        return;
    }
    QVariantMap config = m_access.defaults();
    const QVariantMap stored = m_profiles.value(m_active);
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        config.insert(it.key(), it.value());
    }
    if (m_access.load) {
        m_access.load(m_form, config);
    }
}

void ProfilePage::storeFormIntoActive()
{
    if (!m_form || m_active.isEmpty() || !m_access.save) {
        return;
    }
    m_profiles.insert(m_active, m_access.save(m_form));
}

void ProfilePage::onProfileChosen(int index)
{
    if (index < 0 || !m_profileCombo) {
        return;
    }
    const QString chosen = m_profileCombo->itemText(index);
    if (chosen == m_active) {
        return;
    }

    storeFormIntoActive();
    m_active = chosen;
    loadFormFromActive();
}

QString ProfilePage::suggestedNewProfileName() const
{
    int counter = 2;
    QString candidate;
    do {
        candidate = tr("Profile %1").arg(counter++);
    } while (m_profiles.contains(candidate));
    return candidate;
}

void ProfilePage::onProfileCreated()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("New Profile"), tr("Profile name:"), QLineEdit::Normal,
        suggestedNewProfileName(), &accepted).trimmed();

    if (!accepted || name.isEmpty()) {
        return;
    }
    if (m_profiles.contains(name)) {
        QMessageBox::warning(this, tr("Profile Exists"), tr("A profile named \"%1\" already exists.").arg(name));
        return;
    }

    storeFormIntoActive();
    m_profiles.insert(name, m_profiles.value(m_active));
    m_active = name;
    refreshSelector();
    loadFormFromActive();
}

void ProfilePage::onProfileRenamed()
{
    if (!m_profiles.contains(m_active)) {
        return;
    }

    QStringList existing = m_profiles.keys();
    existing.removeAll(m_active);

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Profile"), tr("New profile name:"), QLineEdit::Normal,
        m_active, &accepted).trimmed();
    if (!accepted || name.isEmpty() || name == m_active) {
        return;
    }
    if (m_profiles.contains(name)) {
        QMessageBox::warning(this, tr("Profile Exists"), tr("A profile named \"%1\" already exists.").arg(name));
        return;
    }

    storeFormIntoActive();
    const QVariantMap values = m_profiles.take(m_active);
    m_profiles.insert(name, values);
    m_active = name;
    refreshSelector();
    loadFormFromActive();
}

void ProfilePage::onProfileDeleted()
{
    if (m_profiles.size() <= 1) {
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Delete Profile"), tr("Delete the profile \"%1\"?").arg(m_active));
    if (answer != QMessageBox::Yes) {
        return;
    }

    m_profiles.remove(m_active);
    m_active = m_profiles.firstKey();
    refreshSelector();
    loadFormFromActive();
}

bool ProfilePage::commit(QString* errorMessage)
{
    storeFormIntoActive();
    if (!m_access.validate) {
        return true;
    }

    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        QString error;
        // A profile that only carries a recipient (typed in the main window) or
        // still equals the defaults is incomplete by design, so it is not
        // validated until the user fills in the credentials.
        QVariantMap candidate = it.value();
        QVariantMap defaults = m_access.defaults();
        for (const ConfigField& field : m_access.schema) {
            if (field.isRecipient()) {
                candidate.remove(field.key);
                defaults.remove(field.key);
            }
        }
        if (candidate == defaults) {
            continue;
        }
        if (!m_access.validate(it.value(), &error)) {
            if (errorMessage) {
                *errorMessage = tr("%1 (profile \"%2\"): %3").arg(pluginLabel(), it.key(), error);
            }
            return false;
        }
    }
    return true;
}

void ProfilePage::persist()
{
    const QString key = m_access.configKey();

    // Remove profiles the user deleted, then write every remaining one.
    const QStringList storedNames = UserConfig::profileNames(m_access.kind, key);
    for (const QString& name : storedNames) {
        if (!m_profiles.contains(name)) {
            UserConfig::deleteProfile(m_access.kind, key, name);
        }
    }

    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        UserConfig::saveProfile(m_access.kind, key, it.key(), it.value());
    }
    UserConfig::setActiveProfile(m_access.kind, key, m_active);
}
