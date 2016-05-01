import QtQuick 2.4
import Qt.labs.folderlistmodel 2.1
import "Oculus.js" as Oculus

OverlayForm {
    property string baseURL: 'file:///c:/Program%20Files%20%28x86%29/Oculus/'

    FolderListModel {
        id: manifestsModel
        folder: baseURL + "Manifests/"
        nameFilters: ["*.json"]
        onCountChanged: {
            coverModel.clear();
            for (var i = 0; i < manifestsModel.count; i++)
                Oculus.loadManifest(manifestsModel.get(i, "fileURL"));
        }
    }

    ListModel {
        id: coverModel
    }

    Component {
        id: coverDelegate
        Image {
            source: coverURL
            MouseArea {
                anchors.fill: parent
                onClicked: Oculus.launchApp(appId)
            }
        }
    }
}
