// SPDX-License-Identifier: MPL-2.0

import QtQuick 6.10
import QtQuick.Controls 6.10 as QQC2
import QtQuick.Layouts 6.10
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: qsTr("Destination Settings")

    property var currentConfig: ({})
    property string currentKey: ""
    property bool saveNoticeVisible: false

    Timer {
        id: saveNoticeTimer
        interval: 2200
        repeat: false
        onTriggered: page.saveNoticeVisible = false
    }

    function loadConfig() {
        const index = appController.destinationTargetIndex
        currentConfig = appController.destinationConfigForIndex(index)
        const keys = appController.destinationTargetKeys
        currentKey = index >= 0 && index < keys.length ? keys[index] : ""
    }

    function updateField(name, value) {
        const next = Object.assign({}, currentConfig)
        next[name] = value
        currentConfig = next
    }

    function saveConfig() {
        appController.saveDestinationConfigForIndex(appController.destinationTargetIndex, currentConfig)
        saveNoticeVisible = true
        saveNoticeTimer.restart()
    }

    Component.onCompleted: loadConfig()

    Connections {
        target: appController
        function onPluginDataChanged() {
            page.loadConfig()
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.saveNoticeVisible
            text: qsTr("Destination settings saved.")
            type: Kirigami.MessageType.Positive
            onVisibleChanged: if (visible) Qt.callLater(() => page.saveNoticeVisible = false)
        }

        Kirigami.Card {
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    level: 2
                    text: qsTr("Select destination type")
                }

                QQC2.ComboBox {
                    Layout.fillWidth: true
                    model: appController.destinationTargets
                    currentIndex: appController.destinationTargetIndex
                    onActivated: {
                        appController.destinationTargetIndex = currentIndex
                        page.loadConfig()
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: qsTr("These settings are saved per destination plugin in your user config folder.")
                    wrapMode: Text.WordWrap
                    opacity: 0.75
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    level: 2
                    text: qsTr("Connection details")
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true
                    visible: page.currentKey === "local_folder"

                    RowLayout {
                        Layout.fillWidth: true

                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: page.currentConfig.targetFolder || ""
                            placeholderText: qsTr("Target folder")
                            onTextEdited: page.updateField("targetFolder", text)
                        }

                        QQC2.Button {
                            text: qsTr("Browse")
                            icon.name: "folder-open"
                            onClicked: {
                                const folder = appController.chooseDirectory(qsTr("Choose Folder"), page.currentConfig.targetFolder || "")
                                if (folder) {
                                    page.updateField("targetFolder", folder)
                                }
                            }
                        }
                    }
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true
                    visible: page.currentKey === "network_share"

                    RowLayout {
                        Layout.fillWidth: true

                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: page.currentConfig.sharePath || ""
                            placeholderText: qsTr("Network share path")
                            onTextEdited: page.updateField("sharePath", text)
                        }

                        QQC2.Button {
                            text: qsTr("Browse")
                            icon.name: "folder-open"
                            onClicked: {
                                const folder = appController.chooseDirectory(qsTr("Choose Share Folder"), page.currentConfig.sharePath || "")
                                if (folder) {
                                    page.updateField("sharePath", folder)
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: page.currentConfig.saveFolder || ""
                            placeholderText: qsTr("Default local save folder")
                            onTextEdited: page.updateField("saveFolder", text)
                        }

                        QQC2.Button {
                            text: qsTr("Browse")
                            icon.name: "folder-open"
                            onClicked: {
                                const folder = appController.chooseDirectory(qsTr("Choose Save Folder"), page.currentConfig.saveFolder || "")
                                if (folder) {
                                    page.updateField("saveFolder", folder)
                                }
                            }
                        }
                    }
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true
                    visible: page.currentKey === "ftp" || page.currentKey === "sftp"

                    QQC2.TextField {
                        text: page.currentConfig.host || ""
                        placeholderText: qsTr("Host")
                        onTextEdited: page.updateField("host", text)
                        Kirigami.FormData.label: qsTr("Host")
                    }

                    QQC2.SpinBox {
                        from: 1
                        to: 65535
                        value: page.currentConfig.port ? page.currentConfig.port : (page.currentKey === "sftp" ? 22 : 21)
                        onValueModified: page.updateField("port", value)
                        Kirigami.FormData.label: qsTr("Port")
                    }

                    QQC2.TextField {
                        text: page.currentConfig.username || ""
                        placeholderText: qsTr("Username")
                        onTextEdited: page.updateField("username", text)
                        Kirigami.FormData.label: qsTr("Username")
                    }

                    QQC2.TextField {
                        text: page.currentConfig.password || ""
                        placeholderText: qsTr("Password")
                        echoMode: TextInput.Password
                        onTextEdited: page.updateField("password", text)
                        Kirigami.FormData.label: qsTr("Password")
                    }

                    QQC2.TextField {
                        text: page.currentConfig.remotePath || ""
                        placeholderText: qsTr("Remote folder")
                        onTextEdited: page.updateField("remotePath", text)
                        Kirigami.FormData.label: qsTr("Remote folder")
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        QQC2.TextField {
                            Layout.fillWidth: true
                            text: page.currentConfig.saveFolder || ""
                            placeholderText: qsTr("Default local save folder")
                            onTextEdited: page.updateField("saveFolder", text)
                        }

                        QQC2.Button {
                            text: qsTr("Browse")
                            icon.name: "folder-open"
                            onClicked: {
                                const folder = appController.chooseDirectory(qsTr("Choose Save Folder"), page.currentConfig.saveFolder || "")
                                if (folder) {
                                    page.updateField("saveFolder", folder)
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            QQC2.Button {
                text: qsTr("Save Settings")
                icon.name: "document-save"
                highlighted: true
                onClicked: page.saveConfig()
            }
        }
    }
}
