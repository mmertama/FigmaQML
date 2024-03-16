import QtQuick
import QtQuick.Controls


/*

  Qt 6.3 Checkbox is not working correctly (tested OSX)

*/


CheckBox {
    id: main
    width: parent ? parent.width : contentItem.width
    background: Rectangle {
        width: main.width
        height: main.height
        color: "black"
    }
    contentItem: Label {
        text: main.text
        font: main.font
        opacity: enabled ? 1.0 : 0.3
        color: "white"
        verticalAlignment: Text.AlignVCenter
        leftPadding: main.indicator.width + main.spacing
   }
}
