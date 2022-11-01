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
    property var model
    property var fontDialog
    signal pickFolder
    signal pickFont(string fontName, string familyName)
    property alias fontFolder: fontFolderText.text
    property alias alternativeSearchAlgorithm: alternativeSearchAlgorithmCheck.checked
    property alias keepFigmaFont: keepFigmaFontCheck.checked
    property bool removeMappings: false
    property var fonts : Object.keys(model)
    property bool switcher: false
    function set(key, family) {
       console.log(key, "->", family);
        main.model[key] = family;
        switcher = true
        main.fonts = Object.keys(main.model)
        switcher = false
    }

    contentItem: ColumnLayout {
        Button {
            id: fontFolderText
            visible: !isWebAssembly
            Layout.alignment: Qt.AlignRight
            width: list.width
            Component.onCompleted: {
                if(text.length === 0) {
                    text =  "Choose font folder";
                }
            }
            onClicked: main.pickFolder()
        }
        Button {
            visible: isWebAssembly
            Layout.alignment: Qt.AlignRight
            width: list.width
            text: "import font"
            onClicked: figmaQml.importFontFolder()
        }
        Text {
            id: placeHolder
            visible: fonts.length === 0
            text: "No QML file with fonts available"
            height: list.height
            width: list.width
        }
        ListView {
            visible: !placeHolder.visible
            id: list
            height: 400
            width:  parent.width
            model: main.fonts
            clip: true
            delegate: RowLayout {
                spacing: 10
                Text {
                    id: key
                    text: modelData
                    Layout.preferredHeight: 18
                    Layout.preferredWidth:  list.width / 2 - 30
                    clip: true
                }
                Rectangle {
                    color: "lightgrey"
                    Layout.preferredWidth: list.width / 2 - 30
                    Layout.preferredHeight: key.height
                    border.width: 1
                    clip: true
                    Text {
                        id: name
                        anchors.centerIn: parent
                        text: main.model[main.switcher ? "" : modelData]
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: main.pickFont(modelData, main.model[modelData])
                    }
                }
                Rectangle {
                    color: "lightgrey"
                    Layout.preferredHeight: key.height
                    Layout.preferredWidth: 40
                    Layout.alignment: Qt.AlignRight
                    border.width: 1
                    Text {
                        anchors.margins: 10
                        anchors.centerIn: parent
                        text: "reset"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            main.model[modelData] = figmaQml.nearestFontFamily(modelData,
                                                                              main.alternativeSearchAlgorithm);
                        }
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
                onClicked: {
                    main.removeMappings = true;
                    main.close();
                }
            }
        }
    }
}
