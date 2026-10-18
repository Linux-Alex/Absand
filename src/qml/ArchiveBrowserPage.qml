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

    FileDialog {
        id: openDialog
        title: qsTr("Open archive")
        nameFilters: [ qsTr("Archives (*.7z *.zip)"), qsTr("All files (*)") ]
        onAccepted: passwordDialog.selectedFile = selectedFile
    }

    FileDialog {
        id: addDialog
        title: qsTr("Select files to add")
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            var error = Browser.addFiles(selectedFiles);
            page.message = error;
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
        property url selectedFile
        title: qsTr("Archive password")
        modal: true
        parent: QQC.Overlay.overlay
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        width: Kirigami.Units.gridUnit * 24
        standardButtons: QQC.Dialog.Ok | QQC.Dialog.Cancel

        onAccepted: {
            page.message = Browser.openArchive(passwordDialog.selectedFile.toString().replace("file://", ""),
                                               passwordField.text);
            page.clearSelection();
        }

        contentItem: ColumnLayout {
            QQC.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Enter the password if the archive is encrypted.")
            }
            QQC.TextField {
                id: passwordField
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Password")
            }
        }
    }

    actions: [
        Kirigami.Action {
            text: Browser.archiveOpen ? qsTr("Add files...") : qsTr("Add files...")
            icon.name: "list-add"
            enabled: Browser.archiveOpen
            onTriggered: addDialog.open()
        },
        Kirigami.Action {
            text: qsTr("Extract selected")
            icon.name: "archive-extract"
            enabled: page.selectedPaths.length > 0
            onTriggered: extractDialog.open()
        },
        Kirigami.Action {
            text: qsTr("Copy paths")
            icon.name: "edit-copy"
            enabled: page.selectedPaths.length > 0
            onTriggered: Browser.copyPaths(page.selectedPaths)
        },
        Kirigami.Action {
            text: qsTr("Refresh")
            icon.name: "view-refresh"
            enabled: Browser.archiveOpen
            onTriggered: {
                page.message = Browser.refresh();
                page.clearSelection();
            }
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true

            QQC.Button {
                text: qsTr("Open archive...")
                icon.name: "document-open"
                onClicked: openDialog.open()
            }
            QQC.TextField {
                Layout.fillWidth: true
                readOnly: true
                text: Browser.archivePath
                placeholderText: qsTr("Open an archive to browse its contents")
            }
            QQC.TextField {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                placeholderText: qsTr("Filter")
                enabled: Browser.archiveOpen
                onTextEdited: Browser.filterText = text
            }
        }

        QQC.Label {
            Layout.fillWidth: true
            visible: Browser.archiveOpen
            opacity: 0.7
            text: Browser.statusText
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: Browser.archiveOpen
        }

        ListView {
            id: entryList
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 16
            visible: Browser.archiveOpen
            clip: true
            model: Browser.entries

            delegate: QQC.ItemDelegate {
                id: entryRow
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
                        // Entries arrive flattened with a depth; indent accordingly.
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
