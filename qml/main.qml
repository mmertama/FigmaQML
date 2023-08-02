import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import FigmaQml
import FigmaGet
import QtQuick.Dialogs


ApplicationWindow {
    id: main
    visible: true
    width: 1280
    height: 1024
    title: qsTr("FigmaQML")

    readonly property string canvasName: (figmaQml && figmaQml.isValid ? (figmaQml.canvasName.length > 0 ?  figmaQml.canvasName : "canvas") : "")
    readonly property string documentName:  (figmaQml && figmaQml.isValid ? (figmaQml.documentName.length > 0 ?  figmaQml.documentName : "figma") : "")
    readonly property string elementName:  (figmaQml && figmaQml.isValid ? (figmaQml.elementName.length > 0 ?  figmaQml.elementName : "figmaItem") : "")

    Timer {
        id: delayedEvents
        property var timedEvents: []
        repeat: false
        running: false
        onTriggered: {
            const data = timedEvents.shift();
            data.fn();
            if(timedEvents.length > 0) {
                delayedEvents.interval = timedEvents[0].ms;
                delayedEvents.start();
            }
        }
    }

    function after(ms, fn) {
        delayedEvents.timedEvents.push({ms:ms, fn:fn});
        if(!delayedEvents.running) {
            delayedEvents.interval = ms;
            delayedEvents.start();
        }

    }

    function sourceCodeError(error, container) {
        let item = null;
        try {
            item = Qt.createQmlObject(figmaQml.sourceCode, container, figmaQml.element)
        } catch(err) {
            return err;
        }
        if(item)
            item.destroy();
        return error;
    }

    function fileName(filename) {
       return filename.substring(filename.lastIndexOf('/') + 1);
    }



    Connections {
        target: figmaGet
        function onError(errorString) {
            console.log(errorString);
            updater.stop();
            errorNote.text = errorString;
        }
        function onUpdateCompleted(isUpdated) {
            updater.updating = false;
        }
    }

    Connections {
        target: figmaQml
        function onError(errorString) {
            updater.stop();
            errorNote.text = errorString;
            console.log(errorString);
        }
        function onWarning(warningString) {
            console.log(warningString);
            warning.text = warningString;
        }
        function onInfo(infoString) {
            console.log(infoString);
            info.text = infoString;
        }
        function onSourceCodeChanged() {
            warning.text = "";
            info.text = "";
            tooManyRequestsNote.visible = false;
            container.create();
        }

        function onTakeSnap(pngFile, canvasIndex, elementIndex) {
            const snapFunction = function() {
                qmlButton.checked = true
                main.after(800, function() {
                    const filename = pngFile.endsWith('.png') ? pngFile : pngFile + '.png';
                    container.figmaview.grabToImage(function(r) {
                        try {
                        r.saveToFile(filename);
                        }
                        catch(error) {
                            console.log(error, "when capturing to", filename);
                        }
                        figmaQml.snapped();
                    }, Qt.size(1024, 1024));
                });
            };
            if(figmaQml.currentCanvas === canvasIndex && figmaQml.currentElement === elementIndex && container.figmaview) {
                snapFunction();
            } else {
                container.onFigmaviewChanged.connect(snapFunction);
            }
        }
        function onWasmRestored(name, file_name) {
            let path = restoreDialog.currentFile.toString();
            if(!figmaGet.restore(file_name)) {
                errorNote.text = "Cannot restore from \"" + name + "\""
            } else
                info.text = name + " restored";
        }
    }


    Connections {
        target: figmaDownload
        function onTooManyRequests() {
            console.log("too many requests");
            tooManyRequestsNote.visible = true;
        }
        function onBytesReceivedChanged() {tooManyRequestsNote.visible = false;}
        function onDownloadingChanged() {tooManyRequestsNote.visible = false;}
        function onCancelled() {tooManyRequestsNote.visible = false;}
    }



    Timer {
        id: updater
        interval: 1000 * 5
        repeat: true
        triggeredOnStart: true
        property bool  updating: false
        onTriggered: {
            if(!updating && !figmaDownload.downloading && !figmaQml.busy) {
                updating = true;
                figmaGet.update()
            }
        }
        onRunningChanged: {
            if(!running)
                updating = false;
        }
    }

    Item {
        Shortcut {
            sequence: StandardKey.Copy
            onActivated: {
                if (tabs.currentItem == figmaSourceButton)
                    clipboard.copy(figmaSource.text)
                else if (tabs.currentItem == qmlSourceButton)
                    clipboard.copy(qmlText.text)
            }
        }
    }

    menuBar: MenuBar {
        id: menubar
        Menu {
            id: menu
            title: "File"
            MenuItem {
                enabled: !updater.running && !figmaDownload.downloading
                text: "Tokens..."
                onTriggered: tokens.open()
            }

            MenuItem {
                enabled: figmaQml && figmaQml.isValid
                text: "Export all QMLs..."
                onTriggered: saveAllQMLs();
            }
            MenuItem {
                text: "Edit imports..."
                onTriggered: imports.open();
            }
            MenuItem {
                text: "Fonts..."
                onTriggered: fontMap.open();
            }
            MenuItem {
                enabled: figmaQml && figmaQml.isValid
                text: "Store..."
                onTriggered: storeFile();
            }
            MenuItem {
                text: "Restore..."
                onTriggered: restoreFile();
            }
            MenuItem {
                text: "About..."
                onTriggered: about.open();
            }
            MenuItem {
                text: "Exit"
                onTriggered: Qt.exit(0);
            }
        }
    }

    header: Row {
            id: controls
            spacing: 2
            enabled: figmaQml && !figmaQml.busy
            TabBar {
                id: tabs
                TabButton {
                    id: qmlSourceButton
                    checked: true
                    width: 100
                    text: "Source"
                }
                TabButton {
                    id: figmaSourceButton
                    width: 100
                    text: "Figma"
                }
                TabButton {
                    id: qmlButton
                    width: 100
                    text: "QtQuick"
                    }
            }
            Item {
                width: 200
                height: 10
            }
            Button {
                id: connectButton
                text: !updater.running ? "Connect " : "Disconnect"
                enabled: figmaGet.projectToken.length > 0
                         && figmaGet.userToken.length > 0
                onClicked: !updater.running ? updater.start() : updater.stop()
                contentItem: Text {
                    anchors.fill: parent
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    text: connectButton.text
                    font.strikeout: !connectButton.enabled // is this working? in UI sense?
                }
            }
            RowButton {
                text: "Settings"
                delegate: Rectangle {
                    height: childrenRect.height
                    width: childrenRect.width
                    layer.enabled: true
                    /*layer.effect:  Loader { //This does not work - rething later how to do compatible drop shadows
                           asynchronous: false
                           Component.onCompleted: {
                               setSource("Qt5DropShadow.qml", {horizontalOffset: 5,
                                                           verticalOffset: 5});
                               if(status == Loader.Error)
                                   setSource("Qt6DropShadow.qml", {horizontalOffset: 5,
                                                               verticalOffset: 5});

                           console.assert(status != Loader.Ready, "Cannot load dropshadow")
                           }
                       }
                       */
                    Column {
                        QtCheckBox {
                            text: "Break Booleans"
                            checked: figmaQml.flags & FigmaQml.BreakBooleans
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.BreakBooleans
                                else
                                    figmaQml.flags &= ~FigmaQml.BreakBooleans
                            }
                        }
                        QtCheckBox {
                            text: "Antialize Shapes"
                            checked: figmaQml.flags & FigmaQml.AntializeShapes
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.AntializeShapes
                                else
                                    figmaQml.flags &= ~FigmaQml.AntializeShapes
                            }
                        }
                        QtCheckBox {
                            text: "Embed images"
                            checked: figmaQml.flags & FigmaQml.EmbedImages
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.EmbedImages
                                else
                                    figmaQml.flags &= ~FigmaQml.EmbedImages
                            }
                        }
                        Repeater {
                            model:  [
                               /* {"Shapes":  FigmaQml.PrerenderShapes},
                                {"Groups":  FigmaQml.PrerenderGroups},
                                {"Components":  FigmaQml.PrerenderComponets},*/
                                {"Render view":  FigmaQml.PrerenderFrames}
                            ]
                            QtCheckBox {
                                text: Object.keys(modelData)[0]
                                width: 200
                                checked: figmaQml.flags & modelData[text]
                                onCheckedChanged: {
                                    if(figmaQml) {
                                        if(checked)
                                            figmaQml.flags |=  modelData[text];
                                        else
                                            figmaQml.flags &= ~modelData[text];
                                    }
                                }
                            }
                        }
                    }
                }
            }
            SpinBox {
                id: canvasSpinner
                visible: tabs.currentItem !== figmaSourceButton
                enabled: figmaQml && figmaQml.canvasCount > 0
                from: 1
                to: figmaQml ? figmaQml.canvasCount : 1
                onValueChanged: {
                    if (figmaQml) {
                        figmaQml.currentCanvas = value - 1
                        elementSpinner.value = 1
                    }
                }
                onEnabledChanged: {
                    value = this.from;
                }
            }
            SpinBox {
                id: elementSpinner
                visible: tabs.currentItem !== figmaSourceButton
                enabled: figmaQml.elementCount > 0
                from: 1
                to: figmaQml.elementCount
                onValueChanged: {
                    figmaQml.currentElement = value - 1
                }
            }
            Slider {
                id: zoomSlider
                visible: tabs.currentItem == qmlButton
                from: 0.2
                to: 5
                value: 1
            }
            RowButton {
                id: sourceChooser
                text: elementName
                visible: tabs.currentItem === qmlSourceButton && elementName.length > 0
                property var content: [elementName].concat(figmaQml.components)
                width:  333
                contentItem: Text {
                    anchors.fill: parent
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    text: sourceChooser.text
                }
                delegate: ListView {
                            model: sourceChooser.content
                            width:  sourceChooser.width
                            height: qmlText.height
                            clip: true
                            layer.enabled: true
                            /*layer.effect: Loader { //This does not work - rething later how to do compatible drop shadows
                                Component.onCompleted: {
                                    setSource("Qt5DropShadow.qml", {
                                                                     transparentBorder: false,
                                                                     horizontalOffset: 8,
                                                                     verticalOffset: 8
                                                                  });
                                    if(status == Loader.Error)
                                        setSource("Qt6DropShadow.qml", {
                                                                     transparentBorder: false,
                                                                     horizontalOffset: 8,
                                                                     verticalOffset: 8
                                                                  });

                                console.assert(status != Loader.Ready, "Cannot load dropshadow")
                                }
                            }*/

                            delegate: RadioButton {
                                width:  sourceChooser.width
                                text: modelData
                                checked: index == 0
                                background: Rectangle {
                                    anchors.fill: parent
                                    color: "#cccccc"
                                }
                                onCheckedChanged: {
                                    if(checked)
                                        sourceChooser.text = modelData
                                }
                            }
                            ScrollBar.vertical: ScrollBar {}
                            Component.onCompleted: {
                               qmlText.interactive = Qt.binding(()=>!visible)
                            }
                        }
                }
            RowButton {
                id: jsonChooser
                text: documentName
                visible: tabs.currentItem === figmaSourceButton && documentName.length > 0
                property var content: [documentName].concat(figmaQml.components)
                width:  333
                contentItem: Text {
                    anchors.fill: parent
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    text: jsonChooser.text
                }
                delegate: ListView {
                            model: jsonChooser.content
                            width:  jsonChooser.width
                            height: figmaSource.height
                            clip: true
                            layer.enabled: true
                            /*layer.effect: Loader { //This does not work - rething later how to do compatible drop shadows
                                Component.onCompleted: {
                                    setSource("Qt5DropShadow.qml", {
                                                                     transparentBorder: false,
                                                                     horizontalOffset: 8,
                                                                     verticalOffset: 8
                                                                  });
                                    if(status == Loader.Error)
                                        setSource("Qt6DropShadow.qml", {
                                                                     transparentBorder: false,
                                                                     horizontalOffset: 8,
                                                                     verticalOffset: 8
                                                                  });

                                console.assert(status != Loader.Ready, "Cannot load dropshadow")
                                }
                            }*/
                            delegate: RadioButton {
                                width:  jsonChooser.width
                                text: modelData
                                checked: index == 0
                                background: Rectangle {
                                    anchors.fill: parent
                                    color: "#cccccc"
                                }
                                onCheckedChanged: {
                                    if(checked)
                                        jsonChooser.text = modelData
                                }
                            }
                            ScrollBar.vertical: ScrollBar {}
                            Component.onCompleted: {
                               figmaSource.interactive = Qt.binding(()=>!visible)
                            }
                        }
                }
            }

    Item {
        height: main.height - controls.height - footer.height - menubar.height
        width: main.width
        property int currenItem: tabs.currentIndex
        function _setVisible() {
            for(let c = 0; c < children.length; c++) {
                children[c].visible = (c === currenItem);
            }
        }
        onCurrenItemChanged: _setVisible();
        Component.onCompleted: _setVisible();
        BigText {
            id: qmlText
            anchors.fill: parent
          //  visible: qmlSourceButton.checked
            text: sourceChooser.text === elementName ? figmaQml.sourceCode : figmaQml.componentSourceCode(sourceChooser.text);
            wrapMode: TextEdit.Wrap
            leftPadding: 5
            FontMetrics {
                id: fontMetrics
                font: qmlText.font
                readonly property real numWidth: advanceWidth('0'.repeat(Math.log10(qmlText.lineCount) + 1))
            }
            linePredessor: Text {
                font: qmlText.font
                color: "gray"
                text: lineIndex + 1
                width: fontMetrics.numWidth
            }
        }
        BigText {
            id: figmaSource
            anchors.fill: parent
         //   visible: jsonButton.checked
            text: jsonChooser.text == documentName ? figmaQml.prettyData(figmaGet.data) : figmaQml.prettyData(figmaQml.componentData(jsonChooser.text));
            wrapMode: TextEdit.WordWrap
        }
        Item {
            clip: true
            anchors.fill: parent
            visible: qmlButton.checked
            Item {
                id: container
                anchors.fill: parent
                width: parent.width
                height: parent.height
                x: 0
                y: 0
                property Item figmaview
                function centrify() {
                    if(figmaview) {
                        figmaview.x = (width - figmaview.width) / 2
                        figmaview.y = (height - figmaview.height) / 2
                    }
                }
                onWidthChanged: {
                    centrify();
                }
                onHeightChanged: {
                    centrify();
                }

                onFigmaviewChanged: {
                    centrify()
                    figmaview.scale = Qt.binding(()=>zoomSlider.value);
                }

                function create() {
                    if (figmaview) {
                        figmaview.destroy()
                    }
                   /* const source =  "import \"file:///"
                                 + figmaQml.qmlDir
                                 + "/qml\"\n\n"
                                 + figmaQml.element; */
                    let comp = null;
                    try {
                        comp = Qt.createComponent(figmaQml.element);
                        if(comp) {
                            const ctor = function() {
                                if (comp.status === Component.Ready) {
                                   figmaview = comp.createObject(container);
                                   figmaQml.componentLoaded(figmaQml.currentCanvas, figmaQml.currentElement);
                                   return true;
                                }
                                if (comp.status === Component.Error) {
                                    throw(comp.errorString());
                                }
                                return false;
                            }
                            if(!ctor())
                                comp.statusChanged.connect(ctor);
                        } else {
                            errorNote.text = "Cannot create component \"" + figmaQml.element + "\"";
                        }
                    } catch (error) {
                        //There is a reason line numbers wont match, and therefore we try to load a sourceCode
                        error = sourceCodeError(error, container);
                        let errors = "Text {text:\"Error loading figma item\";}\n"

                        console.debug("Catch error on create:", error)

                        if(error.qmlErrors) {
                            for (let i = 0; i < error.qmlErrors.length; i++) {
                                errors += "Column {\nText {text:\"" + "line: "
                                        + error.qmlErrors[i].lineNumber  + "\";}\n"
                                        + "Text {text:\"column: "
                                        + error.qmlErrors[i].columnNumber + "\";}\n"
                                        + "Text {text:\"file: "
                                        + fileName(error.qmlErrors[i].fileName) + "\";}\n"
                                        + "Text {text:'message: "
                                        + error.qmlErrors[i].message.split('').map(c=>'\\x' + c.charCodeAt(0).toString(16)).join('') + "';}\n}\n"
                            }
                        } else {
                           errors += "Text {text: \"Unknown error:" + error.replace(/"/g, '\\"') +"\" }";
                        }
                        let content = "import QtQuick 2.14\n Column {\n" + errors + "}\n";
                        try {
                        figmaview = Qt.createQmlObject(
                                    content,
                                    container, "Debug info");
                        } catch (error) {
                            print ("Error loading QML : ")
                            for (let i = 0; i < error.qmlErrors.length; i++) {
                                print("lineNumber: " + error.qmlErrors[i].lineNumber)
                                print("columnNumber: " + error.qmlErrors[i].columnNumber)
                                print("content: " + content)
                                print("message: " + error.qmlErrors[i].message)
                            }
                        }
                        updater.stop();
                        if(typeof source !== 'undefined')
                            console.log(source); //throw out the code
                        else
                            console.debug("Source not created")
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                drag.target: container.figmaview
                drag.axis: Drag.XAndYAxis
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: figmaQml && figmaQml.busy
        width: 200
        height: 200
    }

    Button {
      anchors.centerIn: parent
      text: "Interrupt"
      visible: figmaQml && figmaQml.busy
      onClicked: {
          updater.stop();
          figmaQml.cancel();
          figmaDownload.cancel();
      }
  }


    footer:  Rectangle {
        id: footer
        color: "#cccccc"
        height: footerRow.height
        width: parent.width
        Row {
            id: footerRow
            spacing: 20
            Label {
                text: figmaDownload ? ("Bytes received: " + figmaDownload.bytesReceived): "--"
            }
            Label {
                text: figmaDownload ? ("Downloads: " + figmaDownload.downloads): "__"
            }
            Text {
                text: documentName
            }
            Text {
                text: canvasName
            }
            Text {
                text: elementName
            }
            Text {
                id: info
            }
            Text {
                id: warning
                color: "red"
                font.bold: true
            }
        }
    }


    function storeFile() {
        if(isWebAssmbly()) {
            const temp_store_file = "/tmp/stored_figma.figmaqml";
            if(!figmaGet.store(temp_store_file, figmaQml.flags, figmaQml.imports)) {
                errorNote.text = "Cannot store data";
            }
            if(!figmaQML.store(documentName + ".figmaqml", temp_store_file)) {
                errorNote.text = "Store not done";
            }
        } else {
            storeDialog.open();
        }
    }


    FileDialog {
        id: storeDialog
        title: "Store"
        property string name:  documentName + ".figmaqml"
        currentFile: "file:///" + encodeURIComponent(name)
        currentFolder: figmaQml.documentsLocation
        nameFilters: [ "QML files (*.figmaqml)", "All files (*)" ]
        fileMode: FileDialog.SaveFile
        onAccepted: {
            let path = storeDialog.currentFile.toString();
            path = path.replace(/^(file:\/\/)/,"");
            if(!figmaGet.store(path, figmaQml.flags, figmaQml.imports)) {
                errorNote.text = "Cannot store to \"" + path + "\""
            } else
                info.text = path + " saved";
        }
    }


    function restoreFile() {
        if(isWebAssmbly()) {
            figmaQML.restore();
        } else {
            restoreDialog.open();
        }
    }

    FileDialog {
        id: restoreDialog
        title: "Restore"
        currentFolder: figmaQml.documentsLocation
        nameFilters: [ "QML files (*.figmaqml)", "All files (*)" ]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            let path = restoreDialog.currentFile.toString();
            path = path.replace(/^(file:\/\/)/,"");
            if(!figmaGet.restore(path)) {
                errorNote.text = "Cannot restore from \"" + path + "\""
            } else
                info.text = path + " restored";
        }
    }

    Popup {
        id: about
        anchors.centerIn: main.contentItem
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        contentItem: ColumnLayout {
            width: parent.width
            RowLayout {
                Layout.alignment: Qt.AlignCenter
                Logo {
                    Layout.preferredWidth: 77
                    Layout.preferredHeight: 32
                    pathSize: Qt.size(157, 63.3)
                    paths: [
                        {fillColor: "#244c91", path: "M69.13,38.82c1.5.19,2,.89,2,1.58,0,1.12-.94,1.91-4.43,1.91a10.76,10.76,0,0,1-3.8-.5,1.8,1.8,0,0,1-1.19-1.68H48.26c0,6.91,9.42,7.9,18,7.9,12.56,0,18.18-2.7,18.18-8.42,0-4-3.49-6.16-9.11-6.85L62,31.12c-1.11-.13-1.47-.56-1.47-1.22,0-1,1.15-1.68,4.48-1.68a10.61,10.61,0,0,1,3.48.49c1,.33,1.71.93,1.71,1.88h13c-.4-5-5.58-7.7-18.17-7.7-9.19,0-16.35,1.65-16.35,7.73,0,3.29,2.25,5.93,7.72,6.62Z"},
                        {fillColor: "#244c91", path: "M112.57,47.29h13.66L129,41.82h10.56l2.78,5.47H156l-13.84-23.6H126.41Zm25.31-9.13h-7.16l3.52-7.33h.09Z"},
                        {fillColor: "#244c91", path: "M1,11.09A10.13,10.13,0,1,1,11.13,21.22,10.12,10.12,0,0,1,1,11.09"},
                        {fillColor: "#244c91", path: "M18.27,47.29L21.01 47.29 21.01 32.02 30.96 47.29 44.23 47.29 44.23 23.76 30.15 23.76 30.15 38.33 20.22 23.76 18.27 23.76 18.27 47.29z"},
                        {fillColor: "#244c91", path: "M6.38,23.73H15.61V47.3H6.38V23.73Z"},
                        {fillColor: "#244c91", path: "M94.14,47.29L108.25 47.29 108.25 27.37 120.3 27.37 120.3 23.71 82.19 23.71 82.19 27.37 94.14 27.36 94.14 47.29z"}
                       ]
                }
                Logo {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    pathSize: Qt.size(800, 800)
                    paths: [
                        {fillColor: "red", path: "M287,123c32-66,65-99,131-99c72,0,131,59,131,132c0,132-131,264-263,396C156,420,24,288,24,156c0-72,58-132,131-132 C221,24,254,57,287,123h0H287z"}
                       ]
                }
                Logo {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 39
                    pathSize: Qt.size(462, 339)
                    paths: [
                        {fillColor: "#41cd52", path: "M 160 87 C 170 86 181 86 191 90 C 199 93 205 99 209 107 C 214 119 216 132 218 145 C 219 162 219 179 216 196 C 215 206 212 216 205 224 C 198 232 188 236 178 236 C 167 237 155 236 146 231 C 140 228 135 224 132 218 C 129 211 127 203 126 196 C 122 171 121 146 126 122 C 128 113 132 104 138 97 C 144 91 152 88 160 87 Z"},
                        {fillColor: "#ffffff", path: "M 122 71 C 137 61 156 58 174 58 C 190 59 208 62 222 72 C 232 79 239 90 244 101 C 251 120 253 141 253 161 C 253 181 252 201 245 220 C 241 233 233 245 221 253 C 229 266 238 279 246 292 C 236 297 225 301 215 306 C 207 292 198 278 189 263 C 178 265 166 265 154 264 C 140 262 125 258 115 248 C 106 241 101 231 97 221 C 92 203 90 185 90 166 C 90 147 91 127 97 108 C 101 93 109 79 122 71 Z"},
                        {fillColor: "#ffffff", path: "M 294 70 C 304 70 315 70 325 70 C 325 84 325 98 325 112 C 339 112 353 112 366 112 C 366 121 366 130 365 140 C 352 140 338 140 325 140 C 325 163 325 186 325 209 C 325 215 325 221 328 227 C 330 232 335 233 340 233 C 348 233 356 233 365 232 C 365 241 366 249 366 257 C 349 260 332 264 316 258 C 309 256 302 252 299 245 C 294 233 294 220 294 208 C 294 185 294 162 294 140 C 286 140 279 140 271 140 C 271 130 271 121 271 112 C 279 112 286 112 294 112 C 294 98 294 84 294 70 Z"},
                        {fillColor: "#41cd52", path: "M 63 0 L 462 0 L 462 274 C 440 296 419 317 397 339 L 0 339 L 0 63 C 21 42 42 21 63 0 Z"}
                    ]
                }
                Logo {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    pathSize: Qt.size(800, 800)
                    paths: [
                        {fillColor: "red", path: "M287,123c32-66,65-99,131-99c72,0,131,59,131,132c0,132-131,264-263,396C156,420,24,288,24,156c0-72,58-132,131-132 C221,24,254,57,287,123h0H287z"}
                       ]
                }
                Logo {
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 75
                    pathSize: Qt.size(200, 300)
                    paths: [
                        {fillColor: "#0acf83", path: "M50 300c27.6 0 50-22.4 50-50v-50H50c-27.6 0-50 22.4-50 50s22.4 50 50 50z"},
                        {fillColor: "#a259ff", path: "M0 150c0-27.6 22.4-50 50-50h50v100H50c-27.6 0-50-22.4-50-50z"},
                        {fillColor: "#f24e1e", path: "M0 50C0 22.4 22.4 0 50 0h50v100H50C22.4 100 0 77.6 0 50z"},
                        {fillColor: "#ff7262", path: "M100 0h50c27.6 0 50 22.4 50 50s-22.4 50-50 50h-50V0z"},
                        {fillColor: "#1abcfe", path: "M200 150c0 27.6-22.4 50-50 50s-50-22.4-50-50 22.4-50 50-50 50 22.4 50 50z"}
                    ]
                }
            }
            Label {
            text: "FigmaQML, Markus Mertama 2020 - 2022"
            }
            Label {
            text: "Version: " + figmaQmlVersionNumber;
            }
        }
    }

    Popup {
        id: tokens
        anchors.centerIn: main.contentItem
        modal: true
        focus: true
        width: 350
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside
        contentItem: ColumnLayout {
            TextField {
                id: user_token_text
                Layout.fillWidth: true
                placeholderText:  "Figma User Token"
                Component.onCompleted: text = figmaGet.userToken
                onTextEdited: figmaGet.userToken = text
            }
             TextField {
                id: project_token_text
                Layout.fillWidth: true
                placeholderText: "Figma Project Token"
                Component.onCompleted: text = figmaGet.projectToken
                onTextEdited: figmaGet.projectToken = text
            }
        }
    }

    ImportEditor {
        id: imports
        anchors.centerIn: main.contentItem
        imports: figmaQml.imports
        setDefault: ()=>figmaQml.defaultImports()
        onAboutToHide: {
            const copy = JSON.parse(JSON.stringify(this.imports));
            figmaQml.imports = copy
        }
    }

    FontMap {
        id: fontMap
        anchors.centerIn: main.contentItem
        model: figmaQml.fonts
        fontFolder: figmaQml.fontFolder
        fontDialog: fontDialog
        onPickFolder: fontFolderDialog.open()
        alternativeSearchAlgorithm: figmaQml.flags & FigmaQml.AltFontMatch
        keepFigmaFont: figmaQml.flags & figmaQml.KeepFigmaFontName
        onPickFont: function (key, family) {
            fontDialog.request(key, family)
        }
        onClosed: {
            figmaQml.setSignals(false); //this potentially make tons of new data-parsing requests, so we just block
            if(fontMap.alternativeSearchAlgorithm)
                figmaQml.flags |= FigmaQml.AltFontMatch
            else
                figmaQml.flags &= ~FigmaQml.AltFontMatch

            if(fontMap.keepFigmaFont)
                figmaQml.flags |= FigmaQml.KeepFigmaFontName
            else
                figmaQml.flags &= ~FigmaQml.KeepFigmaFontName

            figmaQml.setSignals(true);

            if(fontMap.removeMappings) {
                figmaQml.resetFontMappings();
            } else {
                for(const i in fontMap.fonts) {
                    const k = fontMap.fonts[i]
                    figmaQml.setFontMapping(k, fontMap.model[k]);
                }
            }
            figmaQml.refresh();
        }

    }

    Connections {
        target: figmaQml
        function onFontLoaded(font) {
            fontDialog.currentFont = font
        }
    }

    /*
    Connections {
        id: fontDialog
        target: figmaQml
        property string key
        function request(fontKey, family) {
            key = fontKey;
            console.log("fontKey - open", family);
            figmaQml.showFontDialog(family);
            console.log("fontKey - done", family);
        }
        function onFontAdded(fontFamily) {
            console.log("onFontAdded", fontFamily);
            fontMap.set(key, fontFamily)
        }
    }
    */

    FontDialog {
        id: fontDialog
        property string key
        function request(fontKey, family) {
            key = fontKey;
            currentFont.family = family;
            open();
        }
        onCurrentFontChanged: {
            console.log(currentFont);
        }
        onAccepted: {
            fontMap.set(key, fontDialog.selectedFont.family)
        }
    }

    function saveAllQMLs() {
        if(isWebAssembly) {
            if(!figmaQml.saveAllQMLZipped(
                        figmaQml.validFileName(documentName),
                        figmaQml.validFileName(canvasName)))
                errorNote.text = "QML save failed";
        } else {
            fileAllDialog.open();
        }
    }

    FolderDialog {
        id: fileAllDialog
        title: "Save All QMLs into"
        //currentFolder: "file:///" + encodeURIComponent(documentName)
        currentFolder: figmaQml.documentsLocation
        acceptLabel: "Save All"
        onAccepted: {
            let path = fileAllDialog.currentFolder.toString();
            path = path.replace(/^(file:\/\/)/,"");
            path = path.replace(/^(\/(c|C):\/)/, "C:/");
            path += "/" + figmaQml.validFileName(documentName) + "/" + figmaQml.validFileName(canvasName)
            if(!figmaQml.saveAllQML(path)) {
                errorNote.text = "Cannot save to \"" + path + "\""
            }
        }
    }

    FolderDialog {
        id: fontFolderDialog
        title: "Add font search folder"
        currentFolder: figmaQml.fontFolder.length > 0 ? "file:///" +
                                                 encodeURIComponent(figmaQml.fontFolder) :
                                                 figmaQml.documentsLocation
        acceptLabel: "Select folder"
        onAccepted: {
            let path = fontFolderDialog.currentFolder.toString();
            path = path.replace(/^(file:\/\/)/,"");
            path = path.replace(/^(\/(c|C):\/)/, "C:/");
            figmaQml.fontFolder = path
        }
    }

    MessageDialog {
        id: tooManyRequestsNote
        buttons: MessageDialog.Ok
        visible: false
        text: "It looks like there is too many requests..., prepare for long wait or interrupt"
    }


    MessageDialog {
        id: errorNote
        buttons: MessageDialog.Ok
        visible: text.length > 0
        onButtonClicked: text = ""
    }

    // this function may invoked from C++ side if upon 1st start
    function openTokens() {
        tokens.open();
    }
}
