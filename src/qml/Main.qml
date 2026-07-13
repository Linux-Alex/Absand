// SPDX-License-Identifier: MPL-2.0

import QtQuick 6.10
import QtQuick.Controls 6.10 as QQC2
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 820
    title: qsTr("Absand")

    Component { id: homePageComponent; HomePage {} }
    Component { id: browserPageComponent; BrowserPage {} }
    Component { id: destinationSettingsPageComponent; DestinationSettingsPage {} }

    pageStack.initialPage: homePageComponent

    globalDrawer: Kirigami.GlobalDrawer {
        title: qsTr("Absand")
        titleIcon: "archive-encrypted"
        actions: [
            Kirigami.Action {
                text: qsTr("Create Archive")
                icon.name: "document-new"
                onTriggered: {
                    root.pageStack.clear()
                    root.pageStack.push(root.homePageComponent)
                }
            },
            Kirigami.Action {
                text: qsTr("Archive Browser")
                icon.name: "folder-open"
                onTriggered: root.pageStack.push(root.browserPageComponent)
            },
            Kirigami.Action {
                text: qsTr("Destination Settings")
                icon.name: "configure"
                onTriggered: {
                    root.pageStack.push(root.destinationSettingsPageComponent)
                }
            }
        ]
    }

    Connections {
        target: appController
        function onShowBrowserRequested() {
            root.pageStack.push(root.browserPageComponent)
        }
        function onShowDestinationSettingsRequested() {
            root.pageStack.push(root.destinationSettingsPageComponent)
        }
    }
}
