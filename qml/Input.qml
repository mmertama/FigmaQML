import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
        id: row
        property string text
        property string buttonText
        property alias textRectWidth: rect.width
        property color color: "white"
        property color borderColor: "black"
        property real minTextWidth: 50
        property real gap: 10
        signal clicked
        Layout.minimumWidth: rect.width + gap + button.width + spacing
        Rectangle {
            id: rect
            Layout.preferredWidth: input.width + gap
            Layout.preferredHeight: button.height - border.width * 2
            border.color: row.borderColor
            color: row.color
            TextInput {
                id: input
                anchors.centerIn: parent
                text: row.text
                width: Math.max(row.minTextWidth, metrics.width)
            }

            TextMetrics {
                id:     metrics
                font:   input.font
                text:   input.text
            }
        }
        Button {
            id: button
            Layout.alignment: Qt.AlignRight
            visible: row.buttonText.length > 0
            text: row.buttonText
            onClicked: row.clicked()
        }
    }

