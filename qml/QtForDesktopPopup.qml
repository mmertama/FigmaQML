import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

Dialog {
    id: dialog
    title: "Qt Export"

    background: Rectangle {
            color: "lightgray"
    }
    property color textBg: "white"
    property color textBgBorder: "gray"

    property alias saveAsApp : saveAsApp.checked
    property list<int> views

    signal saveRequest;

    onClosed: {
        figmaQml.qulInfoStop();
    }

    function _elements() : list<int> {
        let els = []
        for(let i = 1; i < included_views.count; ++i) {  // starting from 1, 0 is current
            els.push(included_views.getIndex(i));
        }
        return els;
    }

    readonly property var params: {
        'qtDir': qtDir.text
    }

    component MyInput : Input {
        color: dialog.textBg
        borderColor: dialog.textBgBorder
        minTextWidth: 300
    }

    Settings {
           id: settings
           property alias qtDirValue: qtDir.text
       }

    ColumnLayout {
        Text {text: "Qt DIR";font.weight: Font.Medium}
        MyInput {
            id: qtDir
            text: "/opt/Qt"
            buttonText: "Select..."
            Layout.preferredWidth: parent.width
            onClicked: {
                folderDialog.title = "Select a Qt Dir"
                folderDialog.target = this
                folderDialog.open()
            }
        }

        Text {text: "Included views"; font.weight: Font.Medium}
        RowLayout {
            Layout.preferredWidth: parent.width
            spacing: 10
            IncludeList {
                id: included_views
                Layout.preferredHeight: Math.max(contentHeight, 100)
                Layout.preferredWidth: qtDir.textRectWidth
            }
            Button {
                Layout.alignment: Qt.AlignTop
                text: "Add view..."
                onClicked: included_views.show_add()
            }
        }
    }

    onVisibleChanged: {
        included_views.init_view()
    }


    footer: Row {
        DialogButtonBox {
            Button {
                text: qsTr("Execute...")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }

            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Save...")
                onClicked: {
                    dialog.views = dialog._elements();
                    dialog.saveRequest();
                }
            }


            onAccepted: {
                dialog.views = dialog._elements();
                dialog.done(Dialog.Accepted)
            }

            onRejected: dialog.done(Dialog.Rejected)

            }
        CheckBox {
            id: saveAsApp
            text: "Save as app"
        }
    }

    FileDialog {
        id: fileDialog
        property var target
        onAccepted: {
            target.text = selectedFile.toString().substring(7) // 7 == "qrc.. blaablaa
        }
    }

    FolderDialog {
        id: folderDialog
        property var target
        onAccepted: {
            target.text = selectedFolder.toString().substring(7)
        }
    }
}
