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
            model: ListModel {
                ListElement {
                    name: "Grey"
                    colorCode: "grey"
                }

                ListElement {
                    name: "Red"
                    colorCode: "red"
                }

                ListElement {
                    name: "Blue"
                    colorCode: "blue"
                }

                ListElement {
                    name: "Green"
                    colorCode: "green"
                }
            }
            cellHeight: 360
            cellWidth: 360
        }
    }
}
