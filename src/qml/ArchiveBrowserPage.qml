// SPDX-License-Identifier: MPL-2.0

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    title: qsTr("Archive browser")

    // Paths of the entries the user ticked.
    property var selectedPaths: []
    property string message: ""

    function togglePath(path) {
        var list = page.selectedPaths.slice();
        var index = list.indexOf(path);
        if (index >= 0) {
            list.splice(index, 1);
        } else {
            list.push(path);
        }
        page.selectedPaths = list;
    }

    function isSelected(path) {
        return page.selectedPaths.indexOf(path) >= 0;
    }

    function clearSelection() {
        page.selectedPaths = [];
    }

    /// Opens an archive and only asks for a password when the file needs one.
    function openPath(path) {
        page.message = "";
        page.clearSelection();

        const error = Browser.openArchive(path, "");
        if (error.length > 0) {
            if (Browser.passwordRequired) {
                page.askPassword(path);
            } else {
                page.message = error;
            }
            return;
        }

        // Archives with encrypted entries list fine, but need the password for
        // extracting, so ask right away instead of failing later.
        if (Browser.passwordRequired) {
            page.askPassword(Browser.archivePath);
        }
    }

    function askPassword(path) {
        passwordDialog.targetPath = path;
        passwordField.text = "";
        passwordDialog.open();
    }

    function applyPassword(path, password) {
        const error = Browser.openArchive(path, password);
        if (error.length > 0) {
            page.message = error;
            if (Browser.passwordRequired) {
                page.askPassword(path);
            }
            return;
        }
        page.clearSelection();
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open archive")
        nameFilters: [ qsTr("Archives (*.7z *.zip)"), qsTr("All files (*)") ]
        onAccepted: page.openPath(selectedFile.toString().replace("file://", ""))
    }

    FileDialog {
        id: addDialog
        title: qsTr("Select files to add")
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            page.message = Browser.addFiles(selectedFiles);
            page.clearSelection();
        }
    }

    FolderDialog {
        id: extractDialog
        title: qsTr("Choose the extraction folder")
        onAccepted: {
            page.message = Browser.extractPaths(page.selectedPaths,
                                                selectedFolder.toString().replace("file://", ""));
        }
    }

    QQC.Dialog {
        id: passwordDialog
        property string targetPath: ""
        title: qsTr("Archive password")
        modal: true
        parent: QQC.Overlay.overlay
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        width: Kirigami.Units.gridUnit * 26
        standardButtons: QQC.Dialog.Ok | QQC.Dialog.Cancel

        onAccepted: page.applyPassword(passwordDialog.targetPath, passwordField.text)

        contentItem: ColumnLayout {
            QQC.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("This archive is encrypted. Enter its password.")
            }
            QQC.TextField {
                id: passwordField
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Password")
                onAccepted: passwordDialog.accept()
            }
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // A real toolbar: icons with readable labels, grouped with separators.
        QQC.ToolBar {
            Layout.fillWidth: true

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC.ToolButton {
                    icon.name: "document-open"
                    text: qsTr("Open")
                    display: QQC.AbstractButton.TextBesideIcon
                    QQC.ToolTip.text: qsTr("Open an archive")
                    QQC.ToolTip.visible: hovered
                    onClicked: openDialog.open()
                }
                QQC.ToolButton {
                    icon.name: "list-add"
                    text: qsTr("Add files")
                    display: QQC.AbstractButton.TextBesideIcon
                    enabled: Browser.archiveOpen
                    onClicked: addDialog.open()
                }
                QQC.ToolButton {
                    icon.name: "archive-extract"
                    text: qsTr("Extract")
                    display: QQC.AbstractButton.TextBesideIcon
                    enabled: page.selectedPaths.length > 0
                    onClicked: extractDialog.open()
                }
                QQC.ToolButton {
                    icon.name: "edit-copy"
                    text: qsTr("Copy paths")
                    display: QQC.AbstractButton.TextBesideIcon
                    enabled: page.selectedPaths.length > 0
                    onClicked: Browser.copyPaths(page.selectedPaths)
                }
                QQC.ToolButton {
                    icon.name: "password-generate"
                    text: qsTr("Set password")
                    display: QQC.AbstractButton.TextBesideIcon
                    visible: Browser.archiveOpen && Browser.archiveEncrypted
                    onClicked: page.askPassword(Browser.archivePath)
                }

                QQC.ToolSeparator {}

                QQC.ToolButton {
                    icon.name: "view-refresh"
                    text: qsTr("Refresh")
                    display: QQC.AbstractButton.TextBesideIcon
                    enabled: Browser.archiveOpen
                    onClicked: {
                        page.message = Browser.refresh();
                        page.clearSelection();
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            QQC.TextField {
                Layout.fillWidth: true
                readOnly: true
                text: Browser.archivePath
                placeholderText: qsTr("Open an archive to browse its contents")
            }
            QQC.TextField {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                placeholderText: qsTr("Filter")
                enabled: Browser.archiveOpen
                onTextEdited: Browser.filterText = text
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Browser.archiveOpen && Browser.archiveEncrypted
            type: Browser.passwordRequired ? Kirigami.MessageType.Warning
                                          : Kirigami.MessageType.Information
            text: Browser.passwordRequired
                  ? qsTr("This archive is encrypted. Set its password to extract files.")
                  : qsTr("This archive is encrypted.")
            actions: [
                Kirigami.Action {
                    text: qsTr("Set password")
                    visible: Browser.passwordRequired
                    onTriggered: page.askPassword(Browser.archivePath)
                }
            ]
        }

        QQC.Label {
            Layout.fillWidth: true
            visible: Browser.archiveOpen
            opacity: 0.7
            text: Browser.statusText
        }

        ListView {
            id: entryList
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 16
            visible: Browser.archiveOpen
            clip: true
            model: Browser.entries

            delegate: QQC.ItemDelegate {
                width: ListView.view.width
                highlighted: page.isSelected(model.path)

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC.CheckBox {
                        checked: page.isSelected(model.path)
                        onToggled: page.togglePath(model.path)
                    }

                    Kirigami.Icon {
                        source: model.isDir ? "folder" : "text-x-generic"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    }

                    QQC.Label {
                        Layout.fillWidth: true
                        text: model.name
                        elide: Text.ElideMiddle
                        leftPadding: model.depth * Kirigami.Units.gridUnit
                    }

                    QQC.Label {
                        text: model.sizeText
                        opacity: 0.6
                    }
                    QQC.Label {
                        text: model.modifiedText
                        opacity: 0.6
                    }
                }
            }
        }

        QQC.Label {
            Layout.fillWidth: true
            visible: Browser.archiveOpen && Browser.matchingCount === 0
            text: qsTr("No matching entries.")
            opacity: 0.7
        }

        QQC.Label {
            Layout.fillWidth: true
            visible: page.message.length > 0
            text: page.message
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.negativeTextColor
        }

        Item { Layout.fillHeight: true }
    }
}
