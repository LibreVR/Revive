import QtQuick 2.4
import QtQuick.Controls 1.4

Item {
    width: 1920
    height: 1080

    Rectangle {
        id: background
        color: "#183755"
        anchors.fill: parent

        ScrollView {
            verticalScrollBarPolicy: 1
            horizontalScrollBarPolicy: 1
            anchors.fill: parent
            GridView {
                id: coverGrid
                interactive: false
                cellHeight: 384
                cellWidth: 384
                anchors.fill: parent
                model: coverModel
                delegate: coverDelegate
            }
        }
    }
}
