import QtQuick 2.14
import QtQuick.Shapes 1.14
import QtQuick.Window 2.14

Item {
    id: main
    property alias paths: repeater.model
    property size pathSize: Qt.size(width, height)
    Repeater {
        id: repeater
        model: paths
        anchors.fill: parent
        Shape {
            layer.enabled: true
            layer.smooth: true
            layer.samples: 8
            anchors.fill: parent
            ShapePath {
                scale: Qt.size(main.width / main.pathSize.width, main.height / main.pathSize.height)
                id: path
                strokeColor: 'strokeColor' in  modelData ? modelData['strokeColor'] : "transparent"
                strokeWidth: 'strokeWidth' in  modelData ? modelData['strokeWidth'] : "0"
                fillColor: 'fillColor' in  modelData ? modelData['fillColor'] : "black"
                PathSvg {
                    path:  modelData['path']
               }
            }
        }
    }
}
