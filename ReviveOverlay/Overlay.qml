import QtQuick 2.4
import Qt.labs.folderlistmodel 2.1
import QtGraphicalEffects 1.0
import QtMultimedia 5.6
import "Oculus.js" as Oculus

OverlayForm {
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
            width: parent.parent.cellWidth
            height: parent.parent.cellHeight

            Rectangle {
                id: coverRect
                visible: coverArea.containsMouse
                anchors.centerIn: parent
                width: coverImage.paintedWidth + 10
                height: coverImage.paintedHeight + 10
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

            Image {
                id: coverImage
                anchors.fill: parent
                fillMode: Image.Pad
                source: coverURL
                MouseArea {
                    id: coverArea
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        activateSound.play();
                        ReviveManifest.launchApplication(appKey);
                    }
                }
            }
        }
    }
}
