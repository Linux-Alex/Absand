// SPDX-License-Identifier: MPL-2.0

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    property bool sendMode: true
    property var plugins: []
    property int pluginIndex: 0
    property var profiles: []
    property string profile: ""
    property var values: ({})
    property string message: ""
    property bool messageIsError: false

    title: sendMode ? qsTr("Password delivery settings") : qsTr("Destination settings")

    function reload() {
        plugins = Backend.pluginSettings(sendMode);
        if (plugins.length === 0) {
            return;
        }
        if (pluginIndex >= plugins.length) {
            pluginIndex = 0;
        }

        var current = plugins[pluginIndex];
        profiles = current.profiles;
        profile = current.activeProfile;
        values = current.values;
        message = "";
    }

    function loadValues() {
        if (plugins.length === 0) {
            return;
        }
        values = Backend.profileValues(sendMode, plugins[pluginIndex].key, profile);
    }

    Component.onCompleted: reload()

    function save() {
        if (plugins.length === 0) {
            return;
        }
        var error = Backend.saveProfileValues(sendMode, plugins[pluginIndex].key, profile, values);
        if (error.length > 0) {
            page.message = error;
            page.messageIsError = true;
        } else {
            page.message = qsTr("Settings saved.");
            page.messageIsError = false;
            page.reload();
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC.ToolBar {
            Layout.fillWidth: true

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC.ToolButton {
                    icon.name: "go-previous"
                    text: qsTr("Back")
                    display: QQC.AbstractButton.TextBesideIcon
                    onClicked: page.applicationWindow.pageStack.pop()
                }
                QQC.ToolButton {
                    icon.name: "document-save"
                    text: qsTr("Save")
                    display: QQC.AbstractButton.TextBesideIcon
                    onClicked: page.save()
                }

                Item { Layout.fillWidth: true }
            }
        }

        QQC.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: sendMode
                ? qsTr("Configure how the password is delivered. Each channel keeps several profiles, for example one per mail server or recipient.")
                : qsTr("Configure where the archive is transferred. Each destination keeps several profiles, for example one per server or recipient.")
        }

        RowLayout {
            Layout.fillWidth: true
            QQC.Label { text: qsTr("Plugin") }
            QQC.ComboBox {
                Layout.fillWidth: true
                model: page.plugins
                textRole: "name"
                currentIndex: page.pluginIndex
                onActivated: (index) => {
                    page.pluginIndex = index;
                    page.reload();
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            QQC.Label { text: qsTr("Profile") }
            QQC.ComboBox {
                Layout.fillWidth: true
                model: page.profiles
                currentIndex: Math.max(0, page.profiles.indexOf(page.profile))
                onActivated: (index) => {
                    page.profile = page.profiles[index];
                    Backend.selectProfile(page.sendMode, page.plugins[page.pluginIndex].key, page.profile);
                    page.loadValues();
                    page.message = "";
                }
            }
            QQC.Button {
                text: qsTr("New")
                icon.name: "list-add"
                onClicked: {
                    nameDialog.renaming = false;
                    nameDialog.open();
                }
            }
            QQC.Button {
                text: qsTr("Rename")
                icon.name: "edit-rename"
                enabled: page.profiles.length > 1
                onClicked: {
                    nameDialog.renaming = true;
                    nameDialog.open();
                }
            }
            QQC.Button {
                text: qsTr("Delete")
                icon.name: "edit-delete"
                enabled: page.profiles.length > 1
                onClicked: deleteDialog.open()
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        SchemaForm {
            id: form
            Layout.fillWidth: true
            schema: page.plugins.length > 0 ? page.plugins[page.pluginIndex].schema : []
            values: page.values
            onEdited: (key, value) => {
                var updated = page.values;
                updated[key] = value;
                page.values = updated;
                page.message = "";
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.message.length > 0
            type: page.messageIsError ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            text: page.message
        }
    }

    QQC.Dialog {
        id: nameDialog
        property bool renaming: false
        title: renaming ? qsTr("Rename profile") : qsTr("New profile")
        modal: true
        parent: QQC.Overlay.overlay
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        width: Kirigami.Units.gridUnit * 22
        standardButtons: QQC.Dialog.Ok | QQC.Dialog.Cancel

        onOpened: {
            nameField.text = nameDialog.renaming ? page.profile : "";
            nameField.forceActiveFocus();
        }
        onAccepted: {
            if (page.plugins.length === 0) {
                return;
            }
            var key = page.plugins[page.pluginIndex].key;
            var error = nameDialog.renaming
                ? Backend.renameProfile(page.sendMode, key, page.profile, nameField.text)
                : Backend.createProfile(page.sendMode, key, nameField.text, page.profile);
            if (error.length > 0) {
                page.message = error;
                page.messageIsError = true;
            } else {
                page.reload();
            }
        }

        contentItem: ColumnLayout {
            QQC.TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("Profile name")
            }
        }
    }

    QQC.Dialog {
        id: deleteDialog
        title: qsTr("Delete profile")
        modal: true
        parent: QQC.Overlay.overlay
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        width: Kirigami.Units.gridUnit * 22
        standardButtons: QQC.Dialog.Ok | QQC.Dialog.Cancel

        onAccepted: {
            if (page.plugins.length === 0) {
                return;
            }
            var error = Backend.deleteProfile(page.sendMode, page.plugins[page.pluginIndex].key, page.profile);
            if (error.length > 0) {
                page.message = error;
                page.messageIsError = true;
            } else {
                page.reload();
            }
        }

        contentItem: QQC.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Delete the profile \"%1\"?").arg(page.profile)
        }
    }
}