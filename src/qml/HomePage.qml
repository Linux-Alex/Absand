// SPDX-License-Identifier: MPL-2.0

import QtQuick 6.10
import QtQuick.Controls 6.10 as QQC2
import QtQuick.Layouts 6.10
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: qsTr("Create Archive")

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.ActionToolBar {
            Layout.fillWidth: true
            display: QQC2.ToolButton.TextBesideIcon
            actions: [
                Kirigami.Action {
                    text: qsTr("Add Files")
                    icon.name: "list-add"
                    onTriggered: appController.addFiles()
                },
                Kirigami.Action {
                    text: qsTr("Clear")
                    icon.name: "edit-clear"
                    onTriggered: appController.clearInputs()
                },
                Kirigami.Action {
                    text: qsTr("Browse Dest")
                    icon.name: "folder-open"
                    onTriggered: appController.openDestinationFolderPicker()
                },
                Kirigami.Action {
                    text: qsTr("Create")
                    icon.name: "document-save"
                    onTriggered: appController.createArchive()
                }
            ]
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: appController.statusMessage.length > 0
            text: appController.statusMessage
            type: Kirigami.MessageType.Positive
        }

        Kirigami.Card {
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Heading {
                    level: 2
                    text: qsTr("Selected Files")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    clip: true
                    model: appController.inputFilesModel
                    spacing: Kirigami.Units.smallSpacing

                    delegate: QQC2.ItemDelegate {
                        width: ListView.view.width
                        text: name
                        icon.name: "text-x-generic"
                        highlighted: false
                        onClicked: ListView.view.currentIndex = index

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: "text-x-generic"
                                width: Kirigami.Units.iconSizes.medium
                                height: Kirigami.Units.iconSizes.medium
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                QQC2.Label {
                                    text: name
                                    elide: Text.ElideRight
                                    font.weight: Font.DemiBold
                                }
                                QQC2.Label {
                                    text: path
                                    elide: Text.ElideMiddle
                                    opacity: 0.7
                                }
                            }
                        }
                    }
                }

                QQC2.Label {
                    text: appController.statusMessage
                    opacity: 0.7
                    wrapMode: Text.WordWrap
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    level: 2
                    text: qsTr("Archive Settings")
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    QQC2.ComboBox {
                        id: archiveFormatCombo
                        model: appController.archiveFormats
                        currentIndex: appController.archiveFormatIndex
                        onActivated: appController.archiveFormatIndex = currentIndex
                        Kirigami.FormData.label: qsTr("Format")
                    }

                    QQC2.CheckBox {
                        checked: appController.encryptArchive
                        text: qsTr("Encrypt archive")
                        onToggled: appController.encryptArchive = checked
                        Kirigami.FormData.label: qsTr("Encryption")
                    }

                    QQC2.TextField {
                        text: appController.password
                        echoMode: TextInput.Password
                        onTextChanged: appController.password = text
                        placeholderText: qsTr("Password")
                        Kirigami.FormData.label: qsTr("Password")
                    }

                    QQC2.ComboBox {
                        model: appController.destinationTargets
                        currentIndex: appController.destinationTargetIndex
                        onActivated: appController.destinationTargetIndex = currentIndex
                        Kirigami.FormData.label: qsTr("Destination")
                    }

                    QQC2.TextField {
                        text: appController.destinationPath
                        onTextChanged: appController.destinationPath = text
                        placeholderText: qsTr("Archive destination path")
                        Kirigami.FormData.label: qsTr("Output")
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    QQC2.Button {
                        text: qsTr("Destination Settings")
                        icon.name: "configure"
                        onClicked: appController.configureDestinations()
                    }
                    Item { Layout.fillWidth: true }
                    QQC2.Button {
                        text: qsTr("Create Archive")
                        icon.name: "document-save"
                        highlighted: true
                        onClicked: appController.createArchive()
                    }
                }
            }
        }
    }
}
