import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import Qt.labs.settings // remove this after 6.7 released (Ubuntu 20.04 is not needed anymore for Qt for MCU)

Dialog {
    id: dialog
    title: "Qt For MCU Export"

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
        'qtDir': qtDir.text,
        'qulVer': qulVer.text,
        'qulPlatform': qulPlatform.text,
        'qtLicense': qtLicense.text,
        'platformTools': platformTools.text,
        'platformHardwareValue': hwSelection.currentText
    }

    Settings {
           id: settings
           property alias qtDirValue: qtDir.text
           property alias qulVerValue: qulVer.text
           property alias qulPlatformValue: qulPlatform.text
           property alias qtLicenseValue: qtLicense.text
           property alias platformToolsValue: platformTools.text
           property alias platformHardwareValue: hwSelection.currentIndex
       }

    component MyInput : Input {
        color: dialog.textBg
        borderColor: dialog.textBgBorder
        minTextWidth: 300
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

        Text {
            text: "Qul Version"
        }
        MyInput {
            id: qulVer
            text: "2.6.0"
        }

        Text {text: "Qul Platform";font.weight: Font.Medium}
        MyInput {
            id: qulPlatform
            text: "STM32F769I-DISCOVERY-baremetal"
        }

        Text {text: "Qt License";font.weight: Font.Medium}
        MyInput {
            id: qtLicense
            text: "./qt-license.txt"
            buttonText: "Select..."
            Layout.preferredWidth: parent.width
            onClicked: {
                fileDialog.title = "Select a Qt License file"
                fileDialog.target = this
                fileDialog.open()
            }
        }

        Text {text: "Platfrom Hardware"; font.weight: Font.Medium}
        ComboBox {
            id: hwSelection
            model: [qsTr("Not spesified")].concat(figmaQml.supportedQulHardware)
            Layout.preferredWidth: parent.width
        }


        Text {text: "Platform tools";font.weight: Font.Medium}
        MyInput {
            id: platformTools
            text: "         "
            buttonText: "Select..."
            Layout.preferredWidth: parent.width
            onClicked: {
                folderDialog.title = "Select platform tools folder"
                folderDialog.target = this
                folderDialog.open()
            }
        }


        Text {text: "Included views"; font.weight: Font.Medium}
        RowLayout {
            Layout.preferredWidth: parent.width
            IncludeList {
                id: included_views
                Layout.preferredHeight: Math.max(contentHeight, 100)
                Layout.preferredWidth: parent.width - 120
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
