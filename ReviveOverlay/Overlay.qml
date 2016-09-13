import QtQuick 2.4
import Qt.labs.folderlistmodel 2.1
import QtGraphicalEffects 1.0
import QtMultimedia 5.6
import "Oculus.js" as Oculus

Rectangle {
    width: 1920
    height: 1080
    color: "#183755"

    FolderListModel {
        id: manifestsModel
        folder: baseURL + 'Software/Manifests/'
        nameFilters: ["*_assets.json"]
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
    }
}
