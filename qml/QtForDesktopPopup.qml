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

    signal saveRequest;

    onClosed: {
        figmaQml.qulInfoStop();
    }

    function elements() {
        let els = []
        for(let i = 1; i < included_views.count; ++i) {  // starting from 1, 0 is current
            els.push(included_views.getIndex(i));
        }
        return els;
    }

    readonly property var params: {
        'qtDir': qtDir.text
    }

    Settings {
           id: settings
           property alias qtDirValue: qtDir.text
       }

    component Input : RowLayout {
        id: row
        property string text
        property string buttonText
        property alias textRectWidth: rect.width
        signal clicked
        Layout.minimumWidth: rect.width + 10 + button.width + spacing
        Rectangle {
            id: rect
            color: dialog.textBg
            border.color: dialog.textBgBorder
            Layout.preferredWidth: input.width + 10
            Layout.preferredHeight: button.height - border.width * 2
            TextInput {
                id: input
                anchors.centerIn: parent
                text: row.text
                width: metrics.width
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

    ColumnLayout {

        Text {text: "Qt DIR";font.weight: Font.Medium}
        Input {
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
                enabled: hwSelection.currentIndex != 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }

            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Save...")
                onClicked: dialog.saveRequest();
            }


            onAccepted: dialog.done(Dialog.Accepted)
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
            target.text = selectedFile.toString().substring(7)
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
