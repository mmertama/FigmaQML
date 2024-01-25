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
        width: parent.width
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
        standardButtons: Dialog.Ok | Dialog.Cancel
        ComboBox {
            id: included_views_add_combox
            ListModel {
                id: included_list_add_items
            }
            model: included_list_add_items
            delegate: ItemDelegate {
                text: content
            }
        }
        onAboutToShow: {
            included_list_add_items.clear()
            for(var i = 0; i < figmaQml.elements.length; ++i) {
                included_list_add_items.append(included_views._format_included_view(i))
            }
        }
        onAccepted: {
            included_views.add_view(included_views_add_combox.currentIndex)
        }
    }
}

