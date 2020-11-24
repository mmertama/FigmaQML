import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15

Popup {
    id: main
    modal: true
    focus: true
    width: 350
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
    property var fonts
    property double _cw: 0
    contentItem: Column {
        Text {
            id: placeHolder
            visible: Object.keys(main.fonts).length === 0
            text: "No QML file with fonts available"
            height: 200
            width: 300
        }
        ListView {
            visible: !placeHolder.visible
            id: list
            height: 200
            width: 300
            model: Object.keys(main.fonts)
            delegate: Row {
                spacing: 10
                Text {
                    text: modelData
                    //width: Math.max(width, main._cw)
                    //Component.onCompleted: main._cw = Math.max(width, main._cw)
                }
                Text {
                    text: main.fonts[modelData]
                }
            }
        }
    }
}
