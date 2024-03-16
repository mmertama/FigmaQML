import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion

Item {
    id: main
    property string text
    property alias font: hidden.font
    property alias wrapMode: hidden.wrapMode
    property alias color: hidden.color
    property alias leftPadding: view.leftMargin
    property alias rightPadding: view.rightMargin
    property alias topPadding: view.topMargin
    property alias bottomPadding: view.bottomMargin
    property alias interactive: view.interactive
    readonly property int topIndex: view.indexAt(1, view.contentY)
    readonly property int bottomIndex: view.indexAt(1, view.contentY + view.contentHeight)
    readonly property alias lineCount: view.count
    property Component linePredessor

    Label {
        id: hidden
        visible: false
        width: 0
        height: 0

    }
    ListView {
        id: view
        anchors.fill:parent
        visible: parent.text.length > 0
        model: parent.text.split('\n')
        clip: true
        delegate: Row {
               width: view.width
               Loader {
                   id: cont
                   property int lineIndex : index
                   sourceComponent: main.linePredessor
               }
               Text {
                   text: modelData
                   textFormat: Text.PlainText
                   wrapMode: hidden.wrapMode
                   font: hidden.font
                   color: hidden.color
                   width: parent.width
                }
        }
        ScrollBar.vertical: ScrollBar {}
    }
}
