import QtQuick
import FigmaQmlInterface
// This file is generated
Item {
    implicitWidth: loader.width
    implicitHeight: loader.height
    Loader {
        id: loader
        anchors.centerIn: parent
        source: FigmaQmlSingleton.currentView
        onLoaded: FigmaQmlSingleton.viewLoaded(FigmaQmlSingleton.currentView);
    }
}
