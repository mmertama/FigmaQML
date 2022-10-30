import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: main
    modal: true
    focus: true
    width: buttonRow.width + 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
    property var fonts
    signal pickFolder
    signal pickFont(string fontName)
    signal resetFont(string fontName)
    signal removeAllMappings
    property alias fontFolder: fontFolderText.text
    property alias alternativeSearchAlgorithm: alternativeSearchAlgorithmCheck.checked
    property alias keepFigmaFont: keepFigmaFontCheck.checked
    contentItem: ColumnLayout {
        Button {
            id: fontFolderText
            Layout.alignment: Qt.AlignRight
            width: list.width
            Component.onCompleted: {
                if(text.length === 0) {
                    text = isWebAssembly ?
                        "Import font" : "Choose font folder";
                }
            }
            onClicked: main.pickFolder()
        }
        Text {
            id: placeHolder
            visible: Object.keys(main.fonts).length === 0
            text: "No QML file with fonts available"
            height: list.height
            width: list.width
        }
        ListView {
            visible: !placeHolder.visible
            id: list
            height: 400
            width:  parent.width
            model: Object.keys(main.fonts)
            clip: true
            delegate: Row {
                spacing: 10
                Text {
                    id: key
                    text: modelData
                    height: 18
                    width:  list.width / 2 - 30
                    clip: true
                }
                Rectangle {
                    color: "lightgrey"
                    width: list.width / 2 - 30
                    height: key.height
                    border.width: 1
                    clip: true
                    Text {
                        anchors.centerIn: parent
                        text: main.fonts[modelData]
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: main.pickFont(modelData)
                    }
                }
                Rectangle {
                    color: "lightgrey"
                    height: key.height
                    width: 40
                    border.width: 1
                    Text {
                        anchors.margins: 10
                        anchors.centerIn: parent
                        text: "reset"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: main.resetFont(modelData)
                    }
                }

            }
        }
        RowLayout {
            id: buttonRow
            CheckBox {
                id: alternativeSearchAlgorithmCheck
                text: "Use alternative font match"
            }
            CheckBox {
                id: keepFigmaFontCheck
                text: "Keep Figma font names"
            }
            Button {
                text: "Remove all mappings"
                onClicked: main.removeAllMappings()
            }
        }
    }
}
