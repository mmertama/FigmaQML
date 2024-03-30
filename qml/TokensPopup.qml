import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Controls

import Qt.labs.settings // remove this after 6.7 released (Ubuntu 20.04 is not needed anymore for Qt for MCU)


Popup {
    id: tokens
    modal: true
    focus: true
    width: 390
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
    Settings {
        property alias user_token_history: user_token_text.history
        property alias project_token_history: project_token_text.history
    }
    function _set_history(history_list: list<string>, value: string) {
        value = value.trim();
        for(let i = 0; i < history_list.length; ++i) {
            if(history_list[i] === value) {
                if(i === 0)
                    return;
                for(let j = i; j > 0; --j) {
                    history_list[j] = history_list[j - 1];
                }
                history_list[0] = value;
                return;
            }
        }
        history_list.unshift(value);
    }
    onAboutToHide: {
        _set_history(user_token_text.history, user_token_text.text)
        _set_history(project_token_text.history, project_token_text.text)
        figmaGet.userToken = user_token_text.text
        figmaGet.projectToken = project_token_text.text
    }
    contentItem: ColumnLayout {
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: user_token_text
                Layout.fillWidth: true
                property list<string> history
                placeholderText: qsTr("Figma User Token")
                Component.onCompleted: text = figmaGet.userToken
                function set_value(value: string) {
                    _set_history(user_token_text.history, value)
                    text = value
                }
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTr("History")
                onClicked: history_popup.show_history(
                               user_token_text.history,
                               function(value) {user_token_text.set_value(value);},
                               function(index) {user_token_text.history.splice(index, 1);})
            }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: project_token_text
                Layout.fillWidth: true
                property list<string> history
                placeholderText: qsTr("Figma Project Token")
                Component.onCompleted: text = figmaGet.projectToken
                function set_value(value: string) {
                    _set_history(project_token_text.history, value)
                    text = value
                }
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTr("History")
                onClicked: history_popup.show_history(
                               project_token_text.history,
                               function(value) {project_token_text.set_value(value);},
                               function(index) {project_token_text.history.splice(index, 1);})
            }
        }
    }
    Popup {
        id: history_popup
        width: 520
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        function show_history(history_list: list<string>, on_use, on_remove) {
            history.model = history_list;
            _list = history_list
            _onUse = on_use
            _onRemove = on_remove
            open()
        }
        property list<string> _list
        property var _onUse
        property var _onRemove
        contentItem: ColumnLayout {
            Label {
                 visible: history.model !== undefined  && history.model.length === 0
                 text: qsTr("There is no history")
            }
            Repeater {
                visible: model !== undefined && model.length > 0
                id: history
                RowLayout {
                    Label {
                        Layout.fillWidth: true
                        text: modelData
                    }
                    Button {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Use")
                        onClicked: {
                            history_popup._onUse(modelData);
                            history_popup.close();
                        }
                    }
                    Button {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Remove")
                        onClicked: {
                            history_popup._list.splice(index, 1);
                            history_popup._onRemove(index);
                            history.model = history_popup._list
                        }
                    }
                }
            }
        }
    }
}
