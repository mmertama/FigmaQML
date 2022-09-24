import QtQuick
import QtQuick.Controls


/*

  Qt 6.3 Checkbox is not working correctly (tested OSX)

*/


CheckBox {
    id: main
    width: parent ? parent.width : contentItem.width
    background: Rectangle {
<<<<<<< HEAD:qml/Qt6CheckBox.qml
        width: 400
        height: 40
=======
        width: main.width
        height: main.height
>>>>>>> initial wasm version:qml/QtCheckBox.qml
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
