import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Window 2.14

Button {
    id: main
    property Component delegate
    property var instance
    Component.onCompleted:{
        instance = delegate.createObject(this, {
                                             'x': mapToItem(main, 0, 0).x,
                                             'y': mapToItem(main.parent, 0, main.height).y,
                                             'visible': false,
                                             'focus': true
                                         });
        instance.onVisibleChanged.connect(function() {if(!instance.visible) closed(delegate);})
        instance.onActiveFocusChanged.connect(function() {
            if(!instance.activeFocus
                    && !main.activeFocus
                    && !(new Array(instance.children)).reduce((a, c) => a || x.activeFocus)) {
                instance.visible = false;
            }
        });
    }
    onClicked: {
        if(instance) {
            if(instance.visible) {
                instance.visible = false;
            } else {
                instance.visible = true;
                instance.forceActiveFocus();
            }
        }
    }
    signal closed(var delegate)
    MouseArea {
        id: mouse
        enabled: instance && instance.visible
        x: mapToGlobal(0,0).x
        y: mapToGlobal(0,0).y
        width: Screen.width
        height: Screen.height
        Component.onCompleted: {
            x = -mapToGlobal(0,0).x
            y = -mapToGlobal(0,0).y
        }
        onClicked: {
            instance.visible = false;
        }
    }
}
