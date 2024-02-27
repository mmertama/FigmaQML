import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs


ListView {
    id: included_views

    readonly property int count: included_list_items.count
    function getIndex(index: int) {
        return included_list_items.get(index)['index'];
    }

    ListModel {
        id: included_list_items
    }

    function show_add() {
        included_views_add.open()
    }

    model: included_list_items
    delegate: ItemDelegate {
        text: content
        width: included_views.width
        onDoubleClicked: {
            if(included_views.currentIndex > 0) // current cannot be removed
                included_list_items.remove(included_views.currentIndex)
        }
    }

    function init_view() {
        included_list_items.clear()
        let el = _get_current_element();
        if(el >= 0) {
            included_list_items.append(_format_included_view(el));
        }
        console.log("included_views", el);
        forceLayout();
        positionViewAtBeginning();
    }

    function _format_included_view(index : int) {
        const str =
                (figmaQml.elements[index]["canvas"] + 1)
                + '-' +
                (figmaQml.elements[index]["element"] + 1)
                + ' ' +
               figmaQml.elements[index]["canvas_name"]
                + '/' +
                figmaQml.elements[index]["element_name"];
        return {content: str, index: index};
    }

    function _get_current_element() {
        for(let i = 0; i < figmaQml.elements.length; ++i) {
            if(figmaQml.currentCanvas === figmaQml.elements[i]["canvas"] &&
                figmaQml.currentElement === figmaQml.elements[i]["element"])
                    return i;
        }
        return -1;
    }

    function add_view (index: int) {
        included_list_items.append(_format_included_view(index));
        forceLayout();
        positionViewAtEnd();
    }

    Dialog {
        id: included_views_add
        title: qsTr("Add View")
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: included_views.width
        ComboBox {
            id: included_views_add_combox
            width: parent.width * 0.9
            textRole: 'content'
            ListModel {
                id: included_list_add_items
            }
            model: included_list_add_items
            delegate: ItemDelegate {
                width: included_views_add.width
                text: content
            }
        }
        onAboutToShow: {
            included_list_add_items.clear()
            for(let i = 0; i < figmaQml.elements.length; ++i) {
                const line = included_views._format_included_view(i);
                for(let j = 0; j < included_list_items.count; ++j) {
                    let item = included_list_items.get(i);
                    if(!item || line.content !== item.content) {
                        included_list_add_items.append(line)
                    }
                }
            }
        }
        onAccepted: {
            // Qt 6.4 and 6.6 behaves quite differently here!
            if(included_views_add_combox.currentValue)
                included_views.add_view(included_views_add_combox.currentValue.index)
            else {
                 included_views.add_view(included_list_add_items.get(included_views_add_combox.currentIndex).index)
            }
        }
    }
}

