// SPDX-License-Identifier: MPL-2.0

#ifndef PROFILEPAGE_H
#define PROFILEPAGE_H

#include "core/UserConfig.h"
#include "interfaces/ConfigSchema.h"

#include <QMap>
#include <QVariantMap>
#include <QWidget>

#include <functional>

class QComboBox;
class QPushButton;

/**
 * One settings page for a plugin: a profile chooser plus the plugin's form.
 *
 * Plugins can keep several named profiles (e.g. "marinic.eu SMTP" and
 * "arcont.si SMTP", or "work" and "private" addresses). The page edits them all
 * in memory and writes them back on accept, so switching profiles never loses
 * edits and validation happens in one place.
 */
class ProfilePage : public QWidget
{
    Q_OBJECT

public:
    struct Access
    {
        UserConfig::ConfigKind kind = UserConfig::ConfigKind::Send;
        std::function<QString()> name;
        std::function<QString()> configKey;
        std::function<QVariantMap()> defaults;
        std::function<QWidget*(QWidget*)> createWidget;
        std::function<void(QWidget*, const QVariantMap&)> load;
        std::function<QVariantMap(QWidget*)> save;
        std::function<bool(const QVariantMap&, QString*)> validate;
        ConfigSchema schema;
    };

    explicit ProfilePage(const Access& access, QWidget* parent = nullptr);

    /// Stores the values currently shown in the form and validates every profile.
    bool commit(QString* errorMessage);

    /// Writes all profiles of this plugin to the configuration store.
    void persist();

    QString pluginLabel() const;

private:
    void loadProfiles();
    void loadFormFromActive();
    void storeFormIntoActive();
    void refreshSelector();
    void onProfileChosen(int index);
    void onProfileCreated();
    void onProfileRenamed();
    void onProfileDeleted();
    QString suggestedNewProfileName() const;

    Access m_access;
    QWidget* m_form = nullptr;
    QComboBox* m_profileCombo = nullptr;
    QPushButton* m_newButton = nullptr;
    QPushButton* m_renameButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QMap<QString, QVariantMap> m_profiles;
    QString m_active;
    QMap<QString, QVariantMap> m_storedProfiles;
};

#endif // PROFILEPAGE_H
