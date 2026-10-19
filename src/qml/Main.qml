// SPDX-License-Identifier: MPL-2.0

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: window
    title: qsTr("Absand %1").arg(Backend.applicationVersion)
    width: 780
    height: 900
    minimumWidth: 620
    minimumHeight: 560

    pageStack.initialPage: mainPage

    // KDE style navigation: a global drawer with the main destinations.
    globalDrawer: Kirigami.GlobalDrawer {
        title: qsTr("Absand")
        titleIcon: "document-encrypt"
        isMenu: false

        footer: QQC.Label {
            text: qsTr("Version %1").arg(Backend.applicationVersion)
            opacity: 0.6
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            padding: Kirigami.Units.smallSpacing
        }

        actions: [
            Kirigami.Action {
                text: qsTr("Create archive")
                icon.name: "document-new"
                onTriggered: window.pageStack.pop(null)
            },
            Kirigami.Action {
                text: qsTr("Archive browser")
                icon.name: "folder-open"
                onTriggered: window.pageStack.push(browserComponent)
            },
            Kirigami.Action {
                text: qsTr("Destination settings")
                icon.name: "configure"
                onTriggered: window.openSettings(false)
            },
            Kirigami.Action {
                text: qsTr("Password delivery settings")
                icon.name: "mail-send"
                onTriggered: window.openSettings(true)
            },
            Kirigami.Action {
                text: qsTr("Language")
                icon.name: "set-language"
                Kirigami.Action {
                    text: Backend.languages[0]
                    checkable: true
                    checked: Backend.languageIndex === 0
                    onTriggered: Backend.languageIndex = 0
                }
                Kirigami.Action {
                    text: Backend.languages[1]
                    checkable: true
                    checked: Backend.languageIndex === 1
                    onTriggered: Backend.languageIndex = 1
                }
            }
        ]
    }

    function openSettings(sendMode) {
        pageStack.push(settingsComponent, { sendMode: sendMode });
    }

    FileDialog {
        id: filesDialog
        title: qsTr("Select files to archive")
        fileMode: FileDialog.OpenFiles
        onAccepted: Backend.addFiles(selectedFiles)
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Select a folder to archive")
        onAccepted: Backend.addFiles([selectedFolder])
    }

    FolderDialog {
        id: outputFolderDialog
        title: qsTr("Select the folder for the archive")
        onAccepted: Backend.destinationFolder = selectedFolder.toString().replace("file://", "")
    }

    Kirigami.ScrollablePage {
        id: mainPage
        title: qsTr("Absand %1").arg(Backend.applicationVersion)

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            // ---------------------------------------------------------- files
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        level: 3
                        text: qsTr("Files to archive")
                    }

                    QQC.Label {
                        text: Backend.inputSummary + "  ·  " + Backend.totalSizeText
                        opacity: 0.7
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: listView.visible ? listView.Layout.preferredHeight
                                                                : Kirigami.Units.gridUnit * 3
                        radius: Kirigami.Units.smallSpacing
                        color: Kirigami.Theme.alternateBackgroundColor
                        visible: !dropArea.containsDrag
                        QQC.Label {
                            anchors.centerIn: parent
                            visible: Backend.inputCount === 0
                            text: qsTr("Drop files or folders here")
                            opacity: 0.6
                        }
                    }

                    DropArea {
                        id: dropArea
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit
                        onDropped: (drop) => Backend.addFiles(drop.urls.map(url => url.toString().replace("file://", "")))
                    }

                    ListView {
                        id: listView
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(Kirigami.Units.gridUnit * 10, contentHeight)
                        visible: Backend.inputCount > 0
                        clip: true
                        model: Backend.inputModel

                        delegate: QQC.ItemDelegate {
                            width: ListView.view.width
                            text: model.name
                            icon.name: "application-x-archive"
                            highlighted: ListView.isCurrentItem
                            onClicked: ListView.view.currentIndex = index
                            QQC.ToolTip.visible: hovered
                            QQC.ToolTip.text: model.path
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC.Button {
                            text: qsTr("Add files...")
                            icon.name: "list-add"
                            display: QQC.AbstractButton.TextBesideIcon
                            onClicked: filesDialog.open()
                        }
                        QQC.Button {
                            text: qsTr("Add folder...")
                            icon.name: "folder-new"
                            display: QQC.AbstractButton.TextBesideIcon
                            onClicked: folderDialog.open()
                        }
                        QQC.Button {
                            text: qsTr("Remove")
                            icon.name: "list-remove"
                            display: QQC.AbstractButton.TextBesideIcon
                            enabled: listView.currentIndex >= 0
                            onClicked: {
                                Backend.removeFile(listView.currentIndex);
                                listView.currentIndex = -1;
                            }
                        }
                        QQC.Button {
                            text: qsTr("Clear all")
                            icon.name: "edit-clear-all"
                            display: QQC.AbstractButton.TextBesideIcon
                            enabled: Backend.inputCount > 0
                            onClicked: Backend.clearFiles()
                        }
                    }
                }
            }

            // -------------------------------------------------------- archive
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        level: 3
                        text: qsTr("Archive settings")
                    }

                    QQC.Label { text: qsTr("Format") }
                    QQC.ComboBox {
                        Layout.fillWidth: true
                        model: Backend.formats
                        textRole: "name"
                        currentIndex: Backend.formatIndex
                        onActivated: (index) => Backend.chooseArchiveFormat(index)
                    }

                    QQC.CheckBox {
                        text: qsTr("Encrypt the archive")
                        enabled: Backend.formatSupportsEncryption
                        checked: Backend.encrypt
                        onToggled: Backend.encrypt = checked
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        enabled: Backend.encrypt

                        QQC.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                            model: Backend.encryptionMethods
                            currentIndex: Backend.encryptionMethodIndex
                            onActivated: (index) => Backend.encryptionMethodIndex = index
                        }
                        QQC.TextField {
                            id: passwordField
                            Layout.fillWidth: true
                            text: Backend.password
                            echoMode: showPassword.checked ? TextInput.Normal : TextInput.Password
                            placeholderText: qsTr("Archive password")
                            onTextEdited: Backend.password = text
                        }
                        QQC.CheckBox {
                            id: showPassword
                            text: qsTr("Show")
                        }
                        QQC.Button {
                            text: qsTr("Generate")
                            icon.name: "password-generate"
                            onClicked: Backend.generatePassword()
                        }
                        QQC.Button {
                            text: qsTr("Copy")
                            icon.name: "edit-copy"
                            onClicked: Backend.copyPassword()
                        }
                    }

                    QQC.Label {
                        Layout.fillWidth: true
                        visible: !Backend.formatSupportsEncryption
                        text: qsTr("This format does not support encryption.")
                        opacity: 0.7
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ---------------------------------------------------- destination
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        level: 3
                        text: qsTr("Destination")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        QQC.Label { text: qsTr("Plugin") }
                        QQC.ComboBox {
                            Layout.fillWidth: true
                            model: Backend.destinations
                            textRole: "name"
                            currentIndex: Backend.destinationIndex
                            onActivated: (index) => Backend.destinationIndex = index
                        }
                        QQC.Label { text: qsTr("Profile") }
                        QQC.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                            model: Backend.destinationProfiles
                            currentIndex: Math.max(0, Backend.destinationProfiles.indexOf(Backend.destinationProfile))
                            onActivated: (index) => Backend.destinationProfile = Backend.destinationProfiles[index]
                        }
                        QQC.Button {
                            text: qsTr("Configure...")
                            icon.name: "configure"
                            onClicked: window.openSettings(false)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: Backend.destinationHasTarget
                        QQC.Label { text: Backend.destinationTargetLabel }
                        QQC.TextField {
                            Layout.fillWidth: true
                            text: Backend.destinationTarget
                            placeholderText: Backend.destinationTargetPlaceholder
                            onTextEdited: Backend.destinationTarget = text
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        QQC.Label { text: qsTr("Save to") }
                        QQC.TextField {
                            Layout.fillWidth: true
                            text: Backend.destinationFolder
                            onTextEdited: Backend.destinationFolder = text
                        }
                        QQC.Button {
                            text: qsTr("Browse...")
                            icon.name: "folder-open"
                            onClicked: outputFolderDialog.open()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        QQC.Label { text: qsTr("File name") }
                        QQC.TextField {
                            Layout.fillWidth: true
                            text: Backend.outputFileName
                            onTextEdited: Backend.outputFileName = text
                        }
                    }
                }
            }

            // ------------------------------------------------------- delivery
            Kirigami.Card {
                Layout.fillWidth: true
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        level: 3
                        text: qsTr("Password delivery")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        QQC.Label { text: qsTr("Send password via") }
                        QQC.ComboBox {
                            Layout.fillWidth: true
                            model: Backend.channels
                            textRole: "name"
                            currentIndex: Backend.channelIndex
                            onActivated: (index) => Backend.channelIndex = index
                        }
                        QQC.Button {
                            text: qsTr("Configure...")
                            icon.name: "configure"
                            enabled: Backend.channelIndex > 0
                            onClicked: window.openSettings(true)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: Backend.channelIndex > 0
                        QQC.Label { text: qsTr("Profile") }
                        QQC.ComboBox {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                            model: Backend.channelProfiles
                            currentIndex: Math.max(0, Backend.channelProfiles.indexOf(Backend.channelProfile))
                            onActivated: (index) => Backend.channelProfile = Backend.channelProfiles[index]
                        }
                        QQC.Label {
                            visible: Backend.channelHasRecipient
                            text: Backend.channelRecipientLabel
                        }
                        QQC.TextField {
                            Layout.fillWidth: true
                            visible: Backend.channelHasRecipient
                            text: Backend.channelRecipient
                            placeholderText: Backend.channelRecipientPlaceholder
                            onTextEdited: Backend.channelRecipient = text
                        }
                    }

                    QQC.Label {
                        Layout.fillWidth: true
                        visible: Backend.channelIndex > 0
                        opacity: 0.7
                        wrapMode: Text.WordWrap
                        text: qsTr("The message template may contain {password} and {link}.")
                    }
                }
            }

            // ---------------------------------------------------------- action
            QQC.ProgressBar {
                Layout.fillWidth: true
                visible: Backend.busy
                from: 0
                to: 100
                value: Backend.progress
            }

            QQC.Label {
                Layout.fillWidth: true
                visible: !Backend.busy && Backend.statusText.length > 0
                text: Backend.statusText
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.negativeTextColor
            }

            QQC.Button {
                Layout.fillWidth: true
                text: Backend.busy ? qsTr("Creating archive...") : qsTr("Create archive")
                icon.name: "document-export"
                enabled: !Backend.busy
                onClicked: Backend.startArchive()
            }
        }
    }

    // -------------------------------------------------------------- settings
    Component {
        id: settingsComponent
        SettingsPage {}
    }

    Component {
        id: browserComponent
        ArchiveBrowserPage {}
    }

    // ------------------------------------------------------------------ result
    QQC.Dialog {
        id: resultDialog
        modal: true
        title: Backend.resultTitle
        parent: QQC.Overlay.overlay
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        width: Math.min(window.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 30)
        visible: Backend.resultVisible
        closePolicy: QQC.Popup.CloseOnEscape
        onClosed: Backend.dismissResult()

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC.Label {
                Layout.fillWidth: true
                text: Backend.resultDetails
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }
            QQC.Label {
                Layout.fillWidth: true
                visible: Backend.resultProblems.length > 0
                text: Backend.resultProblems
                color: Kirigami.Theme.negativeTextColor
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }
            RowLayout {
                Layout.fillWidth: true
                QQC.Button {
                    text: qsTr("Copy password")
                    icon.name: "edit-copy"
                    visible: Backend.resultHasPassword
                    onClicked: Backend.copyPassword()
                }
                QQC.Button {
                    text: qsTr("Copy link")
                    icon.name: "edit-copy"
                    visible: Backend.resultShareLink.length > 0
                    onClicked: Backend.copyShareLink()
                }
                Item { Layout.fillWidth: true }
                QQC.Button {
                    text: qsTr("Close")
                    onClicked: {
                        Backend.dismissResult();
                        resultDialog.close();
                    }
                }
            }
        }
    }
}