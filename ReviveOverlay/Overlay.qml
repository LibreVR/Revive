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

    FolderListModel {
        id: manifestsModel
        folder: baseURL + 'Manifests/'
        nameFilters: ["*_assets.json"]
        showDirs: false
        onCountChanged: {
            coverModel.clear();
            for (var i = 0; i < manifestsModel.count; i++)
                Oculus.loadAssetsManifest(manifestsModel.get(i, "fileURL"));
        }
    }

    ListModel {
        id: coverModel
    }

    SoundEffect {
        id: activateSound
        source: openvrURL + "tools/content/panorama/sounds/activation.wav"
    }

    SoundEffect {
        id: moveSound
        source: openvrURL + "tools/content/panorama/sounds/focus_change.wav"
        volume: 0.6
    }

    Component {
        id: coverDelegate
        Item {
            width: coverGrid.cellWidth
            height: coverGrid.cellHeight

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
                    onClicked: {
                        activateSound.play();
                        ReviveManifest.launchApplication(appKey);
                    }
                }
            }
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
                ReviveManifest.launchApplication(cover.appKey);
            }
        }
    }
}
