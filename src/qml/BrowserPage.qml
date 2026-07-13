// SPDX-License-Identifier: MPL-2.0

import QtQuick 6.10
import QtQuick.Controls 6.10 as QQC2
import QtQuick.Layouts 6.10
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: qsTr("Archive Browser")

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.ActionToolBar {
            Layout.fillWidth: true
            display: QQC2.ToolButton.IconOnly
            actions: [
                Kirigami.Action { text: qsTr("Open"); icon.name: "document-open"; onTriggered: appController.openArchive() },
                Kirigami.Action { text: qsTr("Add"); icon.name: "list-add"; onTriggered: appController.addFilesToArchive() },
                Kirigami.Action { text: qsTr("Open"); icon.name: "document-open"; onTriggered: appController.browserOpenCurrent() },
                Kirigami.Action { text: qsTr("Open With"); icon.name: "system-run"; onTriggered: appController.browserOpenCurrentWith() },
                Kirigami.Action { text: qsTr("Extract"); icon.name: "archive-extract"; onTriggered: appController.browserExtractCurrent() },
                Kirigami.Action { text: qsTr("Copy Path"); icon.name: "edit-copy"; onTriggered: appController.browserCopyCurrentPath() },
                Kirigami.Action { text: qsTr("Refresh"); icon.name: "view-refresh"; onTriggered: appController.refreshArchive() }
            ]
        }

        RowLayout {
            Layout.fillWidth: true
            QQC2.TextField {
                Layout.fillWidth: true
                text: appController.currentArchivePath
                readOnly: true
                placeholderText: qsTr("Open an archive to browse")
            }
            QQC2.ComboBox {
                model: [qsTr("Details"), qsTr("Compact"), qsTr("Icons")]
                currentIndex: appController.browserViewMode
                onActivated: appController.browserViewMode = currentIndex
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: appController.browserStatusMessage.length > 0
            text: appController.browserStatusMessage
            type: Kirigami.MessageType.Information
        }

        Kirigami.Card {
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                ListView {
                    id: archiveList
                    Layout.fillWidth: true
                    Layout.preferredHeight: 420
                    clip: true
                    model: appController.archiveEntriesModel
                    currentIndex: -1
                    spacing: Kirigami.Units.smallSpacing

                    delegate: QQC2.ItemDelegate {
                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        onClicked: {
                            ListView.view.currentIndex = index
                            appController.browserSetCurrentIndex(index)
                        }
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: iconName
                                width: Kirigami.Units.iconSizes.medium
                                height: Kirigami.Units.iconSizes.medium
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                QQC2.Label {
                                    text: name
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                QQC2.Label {
                                    text: path
                                    opacity: 0.7
                                    elide: Text.ElideMiddle
                                }
                            }
                            QQC2.Label {
                                text: sizeText
                                horizontalAlignment: Text.AlignRight
                            }
                            QQC2.Label {
                                text: typeText
                                opacity: 0.7
                            }
                        }
                    }
                }
            }
        }
    }
}
