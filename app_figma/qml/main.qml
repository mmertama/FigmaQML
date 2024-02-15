import QtQuick
import QtQuick.Window
import FigmaQmlInterface

Window {
    visible: true
    title: qsTr("Figma UI")
    width: figma.implicitWidth
    height: figma.implicitHeight
    FigmaQmlUi {
        id: figma
    }
}
