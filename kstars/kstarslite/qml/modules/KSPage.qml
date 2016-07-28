import QtQuick 2.4
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2
import QtQuick.Controls 1.4 as Controls
import org.kde.kirigami 1.0 as Kirigami
import "../constants" 1.0

Kirigami.Page {
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    anchors.fill: parent
    visible: false

    property Item contentItem: null
    property Item prevPage: null

    function showPage(backtoInit) {
        if(backtoInit) {
            mainWindow.currentPage.visible = false // Hide current page
            prevPage = mainWindow.initPage // Go back to SkyMap
        } else {
            prevPage = mainWindow.currentPage
        }
        prevPage.visible = false
        visible = true
        mainWindow.currentPage = this
    }

    function goBack() {
        prevPage.visible = true
        visible = false
        mainWindow.currentPage = prevPage
        prevPage = null
    }

    Connections {
        target: mainWindow
        onClosing: {
            if (Qt.platform.os == "android" && visible) {
                if(prevPage !== null) {
                    close.accepted = false;
                    goBack()
                }
            }
        }
    }

    Rectangle {
        id: headerRect
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: Screen.height * 0.09 + headerSeparator.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#207ce5" }
            GradientStop { position: 0.70; color: "#499bea" }
        }

        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10 * num.dp

            Rectangle {
                id: backRect
                radius: width * 0.5
                color: "#F0F0F0"
                //opacity: 0
                state: "released"

                width: pageTitle.height + 15 * num.dp
                height: width

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        goBack()
                    }
                    onPressedChanged: {
                        if(pressed) {
                            backRect.state = "pressed"
                            console.log(backRect.state)
                        } else {
                            backRect.state = "released"
                            console.log(backRect.state)
                        }
                    }
                }

                states: [
                    State {
                        name: "pressed"
                        PropertyChanges { target: backRect; opacity: 0.3 }
                        //PropertyChanges { target: backButton; opacity: 0.6 }
                    },
                    State {
                        name: "released"
                        PropertyChanges { target: backRect; opacity: 0 }
                        //PropertyChanges { target: backButton; opacity: 1 }
                    }
                ]

                transitions: [
                    Transition {
                        from: "released"
                        to: "pressed"
                        PropertyAnimation { target: backRect; properties: "opacity"; duration: 100 }
                    },
                    Transition {
                        from: "pressed"
                        to: "released"
                        PropertyAnimation { target: backRect; properties: "opacity"; duration: 150 }
                    }
                ]
            }

            Image {
                id: backButton
                visible: prevPage !== null
                source: "images/" + num.density + "/icons/back.png"
                sourceSize.height: pageTitle.height + 2 * num.dp
                sourceSize.width: pageTitle.height + 2 * num.dp
                anchors.centerIn: backRect
            }

            Text {
                id: pageTitle
                visible: prevPage !== null
                text: title
                color: "white"
                anchors.left: backButton.right
                anchors.leftMargin: 20 * num.dp
            }
        }

        Rectangle {
            id: headerSeparator
            anchors {
                top: parent.bottom
                left: parent.left
                right: parent.right
            }
            height: 5 * num.dp
            color: "#c5c5c5"
        }
    }

    Component.onCompleted: {
        if(contentItem !== null) {
            contentItem.anchors.top = headerRect.bottom
            contentItem.anchors.left = contentItem.parent.left
            contentItem.anchors.right = contentItem.parent.right
            //contentItem.anchors.leftMargin = 25
            //contentItem.anchors.rightMargin = 25
        }
    }
}
