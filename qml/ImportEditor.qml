import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14

Popup {
     id: main
     anchors.centerIn: main.contentItem
     modal: true
     focus: true
     width: 350
     closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
     property var imports
     property var setDefault: null
     contentItem: Column {
         ListView {
             id: list
             height: 200
             width: 300
             model: Object.keys(main.imports)
             clip: true
             property double r1:  0
             property double r2:  0
             property var checked: new Array(model.length)
             delegate:  Row {
                spacing: 10
                TextEdit {
                    id: import_file
                    text: modelData
                    property string initialText: text
                    onTextChanged: {
                        if(initialText != text) {
                            if(!(text in main.imports)) {
                                main.imports[text] = main.imports[initialText];
                                delete main.imports[initialText]
                                initialText = text
                                list.r1 = Math.max(list.r1, this.width)
                                this.width = Qt.binding(()=>list.r1) //we can to bind only after
                            }
                        }
                    }
                    Component.onCompleted:{
                        list.r1 = Math.max(list.r1, this.width)
                        this.width = Qt.binding(()=>list.r1) //we can to bind only after
                    }
                }
                TextEdit {
                    id: import_version
                    text: main.imports[modelData]
                    onTextChanged: {
                        main.imports[import_file.text] = text
                        list.r1 = Math.max(list.r1, this.width)
                        this.width = Qt.binding(()=>list.r1) //we can to bind only after
                        }
                    Component.onCompleted: {
                        list.r2 = Math.max(list.r2, this.width)
                        this.width = Qt.binding(()=>list.r2) //we can to bind only after
                    }
                }
                CheckBox {
                    id: checkbox
                    height: import_version.height
                    onCheckedChanged: {
                        list.checked[index] = checked
                    }
                    indicator: Rectangle {
                        implicitHeight: checkbox.height
                        implicitWidth: checkbox.height
                        color: "transparent"
                        border.color: "black"
                        radius: 20
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 5
                            radius: 20
                            color: "black"
                            visible: checkbox.checked
                        }
                    }
                }
             }
             ScrollBar.vertical: ScrollBar {}
        }
         Row {
             Button {
                 text: "Append"
                 onClicked: {
                     main.imports["Library"] = "1.0";
                     list.model = Object.keys(main.imports)
                 }
             }
             Button {
                 text: "Remove"
                 onClicked: {
                     const checked = list.checked //let's make copies as bindings have side effects
                     for(let i = checked.length - 1; i >= 0; i--) {
                         if(checked[i]) {
                             const copy = list.model
                             copy.splice(i, 1)
                             list.model = copy
                             delete main.imports[Object.keys(main.imports)[i]]
                         }
                     }
                 }
             }
             Button {
                 text: "Default"
                 visible: main.setDefault
                 onClicked: {
                     list.model = [] //keep bindings in control
                     imports = main.setDefault()
                     list.model = Object.keys(main.imports)
                 }
             }
         }
     }
}
