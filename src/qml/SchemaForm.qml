// SPDX-License-Identifier: MPL-2.0

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

/**
 * Renders a plugin configuration schema (the ConfigField list the plugins
 * declare) with native Qt Quick controls. Because the plugins describe their
 * settings instead of building widgets, both front ends share one definition.
 */
ColumnLayout {
    id: form

    property var schema: []
    property var values: ({})
    property bool hasRecipientField: false

    signal edited(string key, var value)

    spacing: Kirigami.Units.smallSpacing

    function valueOf(key, fallback) {
        if (form.values === undefined || form.values === null) {
            return fallback;
        }
        var value = form.values[key];
        return (value === undefined || value === null) ? fallback : value;
    }

    FolderDialog {
        id: directoryDialog
        property string targetKey: ""
        onAccepted: form.edited(targetKey, selectedFolder.toString().replace("file://", ""))
    }

    FileDialog {
        id: fileDialog
        property string targetKey: ""
        property bool saving: false
        fileMode: saving ? FileDialog.SaveFile : FileDialog.OpenFile
        onAccepted: form.edited(targetKey, selectedFile.toString().replace("file://", ""))
    }

    Repeater {
        model: form.schema

        delegate: ColumnLayout {
            id: block
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            readonly property var field: modelData
            readonly property string fieldType: field ? field.type : "text"
            readonly property var fieldDefault: (field && field.defaultValue !== undefined
                                                  && field.defaultValue !== null)
                                                  ? field.defaultValue : ""
            readonly property var currentValue: form.valueOf(field.key, fieldDefault)

            // Bool fields put their label next to the switch.
            QQC.CheckBox {
                Layout.fillWidth: true
                visible: block.fieldType === "bool"
                text: block.field.label
                checked: block.currentValue === true
                onToggled: form.edited(block.field.key, checked)
            }

            QQC.Label {
                visible: block.fieldType !== "bool"
                text: block.field.label + (block.field.required ? " *" : "")
                font.bold: false
                opacity: 0.85
            }

            QQC.Label {
                Layout.fillWidth: true
                visible: block.field.help && block.field.help.length > 0 && block.fieldType !== "bool"
                text: block.field.help
                opacity: 0.6
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }

            // Choice
            QQC.ComboBox {
                Layout.fillWidth: true
                visible: block.fieldType === "choice"
                model: block.field.choices
                textRole: "label"
                valueRole: "value"
                onActivated: (index) => form.edited(block.field.key, block.field.choices[index])

                Component.onCompleted: {
                    var list = [];
                    for (var i = 0; i < block.field.choices.length; ++i) {
                        list.push({ value: block.field.choices[i],
                                    label: block.field.choiceLabels[i] });
                    }
                    model = list;
                    currentIndex = Math.max(0, block.field.choices.indexOf(block.currentValue.toString()));
                }
                onModelChanged: currentIndex = Math.max(0, block.field.choices.indexOf(block.currentValue.toString()))
            }

            // Multiline
            QQC.TextArea {
                Layout.fillWidth: true
                visible: block.fieldType === "multiline"
                text: block.currentValue.toString()
                placeholderText: block.field.placeholder
                wrapMode: TextArea.Wrap
                onTextEdited: form.edited(block.field.key, text)
            }

            // Single line (text, password, integer) with an optional browse button
            RowLayout {
                Layout.fillWidth: true
                visible: block.fieldType !== "choice" && block.fieldType !== "bool"
                         && block.fieldType !== "multiline"

                QQC.TextField {
                    id: lineEdit
                    Layout.fillWidth: true
                    text: block.currentValue.toString()
                    placeholderText: block.field.placeholder
                    echoMode: block.fieldType === "password" ? TextInput.Password : TextInput.Normal
                    inputMethodHints: block.fieldType === "integer" ? Qt.ImhDigitsOnly : Qt.ImhNone
                    validator: block.fieldType === "integer" ? intValidator : null
                    onTextEdited: form.edited(block.field.key, text)
                }

                QQC.Button {
                    visible: block.field.browse && block.field.browse.length > 0
                    text: qsTr("Browse...")
                    onClicked: {
                        if (block.field.browse === "directory") {
                            directoryDialog.targetKey = block.field.key;
                            directoryDialog.open();
                        } else {
                            fileDialog.targetKey = block.field.key;
                            fileDialog.saving = block.field.browse === "saveFile";
                            fileDialog.open();
                        }
                    }
                }
            }

            QQC.Label {
                Layout.fillWidth: true
                visible: block.field.recipient
                text: qsTr("This value can also be changed in the main window.")
                opacity: 0.55
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }

            QQC.MenuSeparator { Layout.fillWidth: true }
        }
    }

    IntValidator {
        id: intValidator
        bottom: 1
        top: 65535
    }
}
