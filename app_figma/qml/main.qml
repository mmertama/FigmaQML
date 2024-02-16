import QtQuick
import QtQuick.Window
import FigmaQmlInterface

Window {
    visible: true
    title: qsTr("Figma UI")
    width: Math.max(200, figma.implicitWidth)
    height: Math.max(200, figma.implicitHeight)
    FigmaQmlUi {
        id: figma
        anchors.centerIn: parent
    }
}
