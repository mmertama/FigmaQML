import QtQuick
Rectangle {
    // Loads the component after completed
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
