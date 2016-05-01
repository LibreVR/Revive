import QtQuick 2.4
import Qt.labs.folderlistmodel 2.1

OverlayForm {
    property var baseURL: 'file:///c:/Program%20Files%20%28x86%29/Oculus/'

    FolderListModel {
        id: manifestsModel
        folder: baseURL + "Manifests/"
        nameFilters: ["*.json"]
        onCountChanged: {
            var assetsFolder = baseURL + "Software/StoreAssets/";

            for (var i = 0; i < manifestsModel.count; i++)
            {
                var manifestURL = manifestsModel.get(i, "fileURL");

                (function(manifestURL) {
                    var xhr = new XMLHttpRequest;
                    xhr.onreadystatechange = function() {
                        if (xhr.readyState == XMLHttpRequest.DONE) {
                            var manifest = JSON.parse(xhr.responseText);
                            if (manifest["packageType"] == "ASSET_BUNDLE") {
                                var cover = assetsFolder + manifest["canonicalName"] + "/cover_square_image.jpg";
                                coverModel.append({coverURL: cover});
                            }
                        }
                    }
                    xhr.open('GET', manifestURL);
                    xhr.send();
                })(manifestURL);
            }
        }
    }

    Component {
        id: fileDelegate
        Text { text: fileName }
    }

    ListModel {
        id: coverModel
    }

    Component {
        id: coverDelegate
        Image { source: coverURL }
    }
}
