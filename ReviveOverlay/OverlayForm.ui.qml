import QtQuick 2.4

Item {
    width: 1920
    height: 1080

    Rectangle {
        id: background
        color: "#183755"
        anchors.fill: parent

        GridView {
            id: gridCovers
            anchors.fill: parent
            model: coverModel
            delegate: coverDelegate
            cellHeight: 360
            cellWidth: 360
        }
    }
}
