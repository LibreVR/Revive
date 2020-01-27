import QtQuick 2.4
import Qt.labs.folderlistmodel 2.1
import QtGraphicalEffects 1.0
import QtMultimedia 5.6
import QtGamepad 1.0
import "Oculus.js" as Oculus

Rectangle {
    width: 1920
    height: 1080
    color: "#183755"
    id: mainWindow

    Component.onCompleted: Oculus.createObjects();

    FolderListModel {
        id: assetsModel
        folder: Revive.BaseURL + 'CoreData/Manifests/'
        nameFilters: ["*.json"]
        showDirs: false
        onCountChanged: {
            // If an application was previously installed, its assets bundle remains.
            // Check if the corresponding applications are still installed.
            for (var i = 0; i < assetsModel.count; i++)
            {
                var key = assetsModel.get(i, "fileName");
                console.log("Found assets bundle " + key);
                var appManifest = key.substring(0, key.indexOf("_assets"));
                //verify only assets files
                if (appManifest.length !=0)
                    Oculus.verifyAppManifest(appManifest);
            }
        }
    }

    Text {
        id: emptyText
        visible: !Revive.LibraryFound
        x: 644
        y: 363
        width: 1052
        height: 134
        color: "#1cc4f7"
        text: qsTr("No Oculus Store games found, please make sure the Oculus software is installed")
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: 56
    }

    ListModel {
        id: coverModel

        ListElement {
            coverURL: "SupportAssets/oculus-worlds/cover_square_image.jpg"
            libraryId: "0"
            appKey: "oculus-worlds"
            appId: "1112064135564993"
        }
        ListElement {
            coverURL: "SupportAssets/oculus-dreamdeck-nux/cover_square_image.jpg"
            libraryId: "0"
            appKey: "oculus-dreamdeck-nux"
            appId: "919445174798085"
        }
        ListElement {
            coverURL: "SupportAssets/oculus-touch-tutorial/cover_square_image.jpg"
            libraryId: "0"
            appKey: "oculus-touch-tutorial"
            appId: "1184903171584429"
        }
        ListElement {
            coverURL: "SupportAssets/oculus-first-contact/cover_square_image.jpg"
            libraryId: "0"
            appKey: "oculus-first-contact"
            appId: "1217155751659625"
        }
    }

    SoundEffect {
        id: activateSound
        source: OpenVR.URL + "content/panorama/sounds/activation.wav"
    }

    SoundEffect {
        id: failSound
        source: OpenVR.URL + "content/panorama/sounds/activation_change_fail.wav"
    }

    SoundEffect {
        id: moveSound
        source: OpenVR.URL + "content/panorama/sounds/focus_change.wav"
        volume: 0.6
    }

    property var currentAppId: "0"

    Component {
        id: coverDelegate
        Item {
            width: coverGrid.cellWidth
            height: coverGrid.cellHeight

            Text {
                color: "#1cc4f7"
                id: coverText
                text: appKey
                font.pixelSize: 24
                width: parent.width - 25
                anchors.centerIn: parent
                wrapMode: Text.WordWrap
            }

            Image {
                id: coverImage
                anchors.fill: parent
                fillMode: Image.Pad
                source: coverURL
                MouseArea {
                    id: coverArea
                    hoverEnabled: true
                    anchors.fill: parent
                    onHoveredChanged: coverGrid.currentIndex = index
                    onPressed: {
                        if (Revive.launchApplication(appKey)) {
                            activateSound.play();
                            currentAppId = appId;
                            heartbeat.start();
                        } else {
                            failSound.play();
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: heartbeat
        interval: 10000
        repeat: true
        onTriggered: {
            Oculus.heartbeat(currentAppId)
        }
    }

    Component {
        id: coverHighlight

        Rectangle {
            id: coverRect
            x: coverGrid.currentItem.x + 5
            y: coverGrid.currentItem.y + 5
            width: coverGrid.cellWidth - 10
            height: coverGrid.cellHeight - 10
            border.color: "#b4dff7"
            border.width: 5
            radius: 5

            RectangularGlow {
                anchors.fill: parent
                glowRadius: 5
                spread: 0.2
                color: parent.border.color
                cornerRadius: parent.radius + glowRadius
            }
        }
    }

    GridView {
        id: coverGrid
        visible: Revive.LibraryFound
        focus: true
        cellHeight: 384
        cellWidth: 384
        anchors.fill: parent
        model: coverModel
        delegate: coverDelegate
        highlight: coverHighlight
        highlightFollowsCurrentItem: false
        keyNavigationWraps: true
    }

    Gamepad {
        property real lastX: 0.0
        property real lastY: 0.0

        onAxisLeftXChanged: {
            if (OpenVR.gamepadFocus) {
                if (axisLeftX > 0.5 && lastX < 0.5) {
                    moveSound.play();
                    coverGrid.moveCurrentIndexRight();
                }
                if (axisLeftX < -0.5 && lastX > -0.5) {
                    moveSound.play();
                    coverGrid.moveCurrentIndexLeft();
                }
            }
            lastX = axisLeftX;
        }

        onAxisLeftYChanged: {
            if (OpenVR.gamepadFocus) {
                if (axisLeftY > 0.5 && lastY < 0.5) {
                    moveSound.play();
                    coverGrid.moveCurrentIndexDown();
                }
                if (axisLeftY < -0.5 && lastY > -0.5) {
                    moveSound.play();
                    coverGrid.moveCurrentIndexUp();
                }
            }
            lastY = axisLeftY;
        }

        onButtonLeftChanged: {

            if (buttonLeft && OpenVR.gamepadFocus) {
                moveSound.play();
                coverGrid.moveCurrentIndexLeft();
            }
        }
        onButtonUpChanged: {

            if (buttonUp && OpenVR.gamepadFocus) {
                moveSound.play();
                coverGrid.moveCurrentIndexUp();
            }
        }
        onButtonRightChanged: {
            if (buttonRight && OpenVR.gamepadFocus) {
                moveSound.play();
                coverGrid.moveCurrentIndexRight();
            }
        }
        onButtonDownChanged: {
            if (buttonDown && OpenVR.gamepadFocus) {
                moveSound.play();
                coverGrid.moveCurrentIndexDown();
            }
        }

        onButtonAChanged: {
            if (buttonA && OpenVR.gamepadFocus && coverGrid.currentIndex != -1) {
                var cover = coverModel.get(coverGrid.currentIndex);
                activateSound.play();
                Revive.launchApplication(cover.appKey);
            }
        }
    }

    Rectangle {
        id: loading
        color: "#80000000"
        visible: OpenVR.loading
        anchors.fill: parent
        z: 1

        AnimatedImage {
            id: loadingIcon
            width: 100
            height: 100
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            source: OpenVR.URL + "content/vrmonitor/icons/icon_loading.gif"
        }
    }

    Image {
        id: link
        x: 10
        y: 10
        width: 50
        height: 50
        opacity: 0.5
        source: "no-link.svg"
        fillMode: Image.PreserveAspectFit
        visible: !Platform.connected
    }
}
