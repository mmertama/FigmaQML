import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import FigmaQml

Popup {
    id: main
    modal: true
    focus: true
    width: buttonRow.width + 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
    property var fonts
    property var fontDialog
    signal pickFolder
    signal pickFont(string fontName, string familyName)
    property alias fontFolder: fontFolderText.text
    property alias alternativeSearchAlgorithm: alternativeSearchAlgorithmCheck.checked
    property alias keepFigmaFont: keepFigmaFontCheck.checked
    property bool removeMappings: false
    function set(key, family) {
        console.log(main.fonts[key], "->", family);
        main.fonts[key] = family;
        list.model = []
        list.model = main.fonts
    }
    onClosed: {
        figmaQml.setSignals(false); //this potentially make tons of new data-parsing requests, so we just block
        if(main.alternativeSearchAlgorithm)
            figmaQml.flags |= FigmaQml.AltFontMatch
        else
            figmaQml.flags &= ~FigmaQml.AltFontMatch

        if(main.keepFigmaFont)
            figmaQml.flags |= FigmaQml.KeepFigmaFontName
        else
            figmaQml.flags &= ~FigmaQml.KeepFigmaFontName

        figmaQml.setSignals(true);

        if(main.removeMappings) {
            figmaQml.resetFontMappings();
        } else {
            for(const k in main.fonts) {
                figmaQml.setFontMapping(k, main.fonts[k]);
            }
            figmaQml.refresh();
        }
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
                        onClicked: main.pickFont(modelData, main.fonts[modelData])
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
                        onClicked: {
                            main.fonts[modelData] = figmaQml.nearestFontFamily(modelData,
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
