// SPDX-License-Identifier: MPL-2.0

#ifndef CONFIGSCHEMA_H
#define CONFIGSCHEMA_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

/**
 * Describes one configurable value of a plugin.
 *
 * Plugins declare their settings as a schema instead of building widgets by
 * hand. The widget based settings dialog renders it today, and the Kirigami/QML
 * frontend renders the very same schema as native Qt Quick controls, so both
 * user interfaces stay in sync automatically.
 */
struct ConfigField
{
    enum class Type {
        Text,
        Password,
        Integer,
        Bool,
        Choice,
        Multiline
    };

    enum class Browse {
        None,
        Directory,
        OpenFile,
        SaveFile
    };

    enum Flag : int {
        NoFlag = 0,
        Required = 1 << 0,
        /// The value is "where it goes" (mail address, phone number, chat id,
        /// room, topic, folder) and is therefore editable in the main window.
        Recipient = 1 << 1,
        Advanced = 1 << 2
    };

    QString key;
    QString label;
    QString help;
    QString placeholder;
    Type type = Type::Text;
    Browse browse = Browse::None;
    QStringList choices;
    QVariant defaultValue;
    int flags = NoFlag;
    /// Optional display labels, parallel to choices; the stored value stays the
    /// entry in choices so a translated label never changes the setting. Kept
    /// last, with a default, so the positional initialisers used by the plugins
    /// stay readable and do not trigger -Wmissing-field-initializers.
    QStringList choiceLabels{};

    bool isRequired() const { return (flags & Required) != 0; }
    bool isRecipient() const { return (flags & Recipient) != 0; }
    bool isAdvanced() const { return (flags & Advanced) != 0; }
};

using ConfigSchema = QVector<ConfigField>;

inline QVariantMap schemaDefaults(const ConfigSchema& schema)
{
    QVariantMap defaults;
    for (const ConfigField& field : schema) {
        if (field.defaultValue.isValid()) {
            defaults.insert(field.key, field.defaultValue);
        }
    }
    return defaults;
}

inline const ConfigField* schemaRecipientField(const ConfigSchema& schema)
{
    for (const ConfigField& field : schema) {
        if (field.isRecipient()) {
            return &field;
        }
    }
    return nullptr;
}

#endif // CONFIGSCHEMA_H
