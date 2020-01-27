import QtQuick 2.4;
import Qt.labs.folderlistmodel 2.1;
import "Oculus.js" as Oculus;

Item {
    property string library;
    property string libraryURL;

    FolderListModel {
        folder: libraryURL + 'Manifests/';
        nameFilters: ["*.json"];
        showDirs: false;
        onCountChanged: {
            Oculus.removeLibrary(library)
            for (var i = 0; i < count; i++) {
                Oculus.loadManifest(get(i, "fileURL"), library);
            }
        }
    }
}
