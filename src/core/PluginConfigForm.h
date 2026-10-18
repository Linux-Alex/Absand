// SPDX-License-Identifier: MPL-2.0

#ifndef PLUGINCONFIGFORM_H
#define PLUGINCONFIGFORM_H

#include "interfaces/ConfigSchema.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QWidget>

/**
 * Renders a ConfigSchema with Qt Widgets and moves values between the widgets
 * and a QVariantMap. Every plugin therefore gets its settings dialog for free,
 * and the QML frontend can mirror the same schema.
 */
namespace PluginConfigForm {

inline QLineEdit* lineEditFor(QWidget* widget, const QString& key)
{
    return widget ? widget->findChild<QLineEdit*>(key) : nullptr;
}

inline QPlainTextEdit* plainTextFor(QWidget* widget, const QString& key)
{
    return widget ? widget->findChild<QPlainTextEdit*>(key) : nullptr;
}

inline QComboBox* comboFor(QWidget* widget, const QString& key)
{
    return widget ? widget->findChild<QComboBox*>(key) : nullptr;
}

inline QCheckBox* checkBoxFor(QWidget* widget, const QString& key)
{
    return widget ? widget->findChild<QCheckBox*>(key) : nullptr;
}

inline QWidget* createEditor(QWidget* parent, const ConfigField& field)
{
    const bool isSecret = field.type == ConfigField::Type::Password;

    switch (field.type) {
    case ConfigField::Type::Bool: {
        auto* box = new QCheckBox(parent);
        box->setObjectName(field.key);
        if (!field.help.isEmpty()) {
            box->setToolTip(field.help);
        }
        return box;
    }
    case ConfigField::Type::Choice: {
        auto* combo = new QComboBox(parent);
        combo->setObjectName(field.key);
        const bool hasLabels = field.choiceLabels.size() == field.choices.size();
        for (int index = 0; index < field.choices.size(); ++index) {
            const QString& choice = field.choices.at(index);
            combo->addItem(hasLabels ? field.choiceLabels.at(index) : choice, choice);
        }
        if (!field.help.isEmpty()) {
            combo->setToolTip(field.help);
        }
        return combo;
    }
    case ConfigField::Type::Multiline: {
        auto* text = new QPlainTextEdit(parent);
        text->setObjectName(field.key);
        text->setPlaceholderText(field.placeholder);
        text->setTabChangesFocus(true);
        text->setToolTip(field.help);
        text->setMinimumHeight(70);
        return text;
    }
    default:
        break;
    }

    auto* edit = new QLineEdit(parent);
    edit->setObjectName(field.key);
    edit->setPlaceholderText(field.placeholder);
    edit->setToolTip(field.help.isEmpty() ? field.placeholder : field.help);
    if (isSecret) {
        edit->setEchoMode(QLineEdit::Password);
    }
    if (field.type == ConfigField::Type::Integer) {
        edit->setValidator(new QIntValidator(1, 65535, edit));
        edit->setMaximumWidth(120);
    }

    if (field.browse == ConfigField::Browse::None) {
        return edit;
    }

    // Wrap the line edit together with a Browse button.
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit, 1);

    auto* browse = new QPushButton(QObject::tr("Browse..."), row);
    layout->addWidget(browse);

    QObject::connect(browse, &QPushButton::clicked, row, [edit, row, field]() {
        QString selected;
        switch (field.browse) {
        case ConfigField::Browse::Directory:
            selected = QFileDialog::getExistingDirectory(row, field.label, edit->text());
            break;
        case ConfigField::Browse::OpenFile:
            selected = QFileDialog::getOpenFileName(row, field.label, edit->text());
            break;
        case ConfigField::Browse::SaveFile:
            selected = QFileDialog::getSaveFileName(row, field.label, edit->text());
            break;
        case ConfigField::Browse::None:
            break;
        }
        if (!selected.isEmpty()) {
            edit->setText(QDir::toNativeSeparators(selected));
        }
    });

    return row;
}

inline QWidget* createWidget(QWidget* parent, const ConfigSchema& schema)
{
    auto* container = new QWidget(parent);
    if (schema.isEmpty()) {
        return container;
    }

    auto* layout = new QFormLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    for (const ConfigField& field : schema) {
        // Every field is editable here; recipient fields additionally get a
        // quick edit box in the main window.
        layout->addRow(field.label, createEditor(container, field));
    }

    return container;
}

inline void load(QWidget* widget, const ConfigSchema& schema, const QVariantMap& config)
{
    if (!widget) {
        return;
    }

    for (const ConfigField& field : schema) {
        const QVariant value = config.value(field.key, field.defaultValue);
        switch (field.type) {
        case ConfigField::Type::Bool:
            if (auto* box = checkBoxFor(widget, field.key)) {
                box->setChecked(value.toBool());
            }
            break;
        case ConfigField::Type::Choice:
            if (auto* combo = comboFor(widget, field.key)) {
                const int index = combo->findData(value.toString());
                combo->setCurrentIndex(index >= 0 ? index : 0);
            }
            break;
        case ConfigField::Type::Multiline:
            if (auto* text = plainTextFor(widget, field.key)) {
                text->setPlainText(value.toString());
            }
            break;
        default:
            if (auto* edit = lineEditFor(widget, field.key)) {
                edit->setText(field.type == ConfigField::Type::Integer
                                  ? QString::number(value.toInt())
                                  : value.toString());
            }
            break;
        }
    }
}

inline QVariantMap save(QWidget* widget, const ConfigSchema& schema)
{
    QVariantMap config;
    if (!widget) {
        return config;
    }

    for (const ConfigField& field : schema) {
        switch (field.type) {
        case ConfigField::Type::Bool:
            if (auto* box = checkBoxFor(widget, field.key)) {
                config.insert(field.key, box->isChecked());
            }
            break;
        case ConfigField::Type::Choice:
            if (auto* combo = comboFor(widget, field.key)) {
                config.insert(field.key, combo->currentData().toString());
            }
            break;
        case ConfigField::Type::Multiline:
            if (auto* text = plainTextFor(widget, field.key)) {
                config.insert(field.key, text->toPlainText().trimmed());
            }
            break;
        case ConfigField::Type::Integer:
            if (auto* edit = lineEditFor(widget, field.key)) {
                config.insert(field.key, edit->text().trimmed().toInt());
            }
            break;
        default:
            if (auto* edit = lineEditFor(widget, field.key)) {
                const bool secret = field.type == ConfigField::Type::Password;
                config.insert(field.key, secret ? edit->text() : edit->text().trimmed());
            }
            break;
        }
    }

    return config;
}

inline bool validate(const ConfigSchema& schema, const QVariantMap& config, QString* errorMessage)
{
    for (const ConfigField& field : schema) {
        if (!field.isRequired()) {
            continue;
        }
        const QVariant value = config.value(field.key);
        const bool empty = value.toString().trimmed().isEmpty()
            || (field.type == ConfigField::Type::Integer && value.toInt() <= 0);
        if (empty) {
            if (errorMessage) {
                *errorMessage = QObject::tr("%1 is required.").arg(field.label);
            }
            return false;
        }
    }
    return true;
}

} // namespace PluginConfigForm

#endif // PLUGINCONFIGFORM_H
