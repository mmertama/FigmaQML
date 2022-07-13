import QtQuick
import QtQuick.Controls


/*

  Qt 6.3 Checkbox is not working correctly (tested OSX)

*/


CheckBox {
    id: main
    background: Rectangle {
        width: 400
        height: 20
        color: "black"
    }
    contentItem: Text {
        text: main.text
        font: main.font
        opacity: enabled ? 1.0 : 0.3
        color: "white"
        verticalAlignment: Text.AlignVCenter
        leftPadding: main.indicator.width + main.spacing
   }
}
