import QtQuick
import FigmaQmlInterface
Rectangle {
    // Loads the component after completed

    Exit_icon_figma {}

    Loader {
        id: loader
        anchors.fill: parent
        active: false
        source:""
        onLoaded: {
            console.log("loaded");
        }
        onItemChanged: {
            if(item)
                console.log("item ok");
            else
                console.log("item failed");

        }
    }
    Component.onCompleted: loader.active = true;
}
