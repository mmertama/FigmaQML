import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import FigmaQml
import FigmaGet
import QtQuick.Dialogs
import FigmaQmlInterface




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
            updater.completed(isUpdated)
            if(isUpdated)
                updater.stop(); // stop updating as its slow and annoyingly interferces app using, fix that if online-mode is a good idea
        }
    }

    Connections {
        target: figmaQml

        function onBusyChanged() {
            if(figmaQml.isValid && !figmaQml.busy) {
                updater.completed(true)
            }
        }

        function onFontLoaded(font) {
            fontDialog.currentFont = font
        }

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
        function onQulInfo(qulInfo, level) {
            if(!consolePopup.opened)
                consolePopup.open()
            consolePopup.level = level;
            consolePopup.text += qulInfo;
            //console.log(qulInfo);
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
        property bool updating: false
        property int currentElement: 0
        property int currentCanvas: 0
        function completed(isUpdated) {
            if(isUpdated) {
                canvasSpinner.value = updater.currentCanvas;
                elementSpinner.value = updater.currentElement;
                console.log("Frestore", currentCanvas, currentElement)
            }
        }
        onUpdatingChanged: {
            if(updating) {
                currentCanvas = canvasSpinner.value;
                currentElement = elementSpinner.value
                console.log("Save", currentCanvas, currentElement)
            }
        }
        onTriggered: {
            if(!updating && !figmaDownload.downloading && !figmaQml.busy) {
                updating = true;
                figmaGet.update()
            }
        }
        onRunningChanged: {
            if(!running) {
                updating = false;
            }
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
                opacity: enabled ? 1 : 0.3
            }
            MenuItem {
                enabled: figmaQml && figmaQml.isValid
                visible: !(has_qul &&(figmaQml.flags & FigmaQml.QulMode))
                text: "Export Qt Desktop..."
                onTriggered: qtForDesktopPopup.open();
                height: visible ? implicitHeight : 0
                opacity: enabled ? 1 : 0.3
            }
            MenuItem {
                enabled: figmaQml && figmaQml.isValid
                visible: has_qul && (figmaQml.flags & FigmaQml.QulMode)
                text: "Export Qt for MCU..."
                onTriggered: qtForMCUPopup.open();
                height: visible ? implicitHeight : 0
                opacity: enabled ? 1 : 0.3
            }
            MenuItem {
                enabled: figmaQml && figmaQml.isValid
                text: "Dump all QMLs..."
                onTriggered: saveAllQMLs();
                opacity: enabled ? 1 : 0.3
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
                opacity: enabled ? 1 : 0.3
            }
            MenuItem {
                enabled: figmaQml && figmaQml.isValid
                text: "SendValue..."
                onTriggered: send_set_value.open()
                opacity: enabled ? 1 : 0.3
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
                text: "Help..."
                onTriggered: {
                    if(!Qt.openUrlExternally("https://mmertama.github.io/FigmaQML/docs")) {
                        errorNote.text = "Cannot open help";
                    }
                }
            }
            MenuItem {
                text: "Exit"
                onTriggered: Qt.exit(0);
            }
        }
        // too much work
        //TextField {
        //    PlaceholderText: "Search"
        //    onTextChanged: {
        //        tabs.currentItem.
        //    }
        //}
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
                text: !updater.running ? "  Update  " : "Cancel update"
                enabled: figmaGet.projectToken.length > 0
                         && figmaGet.userToken.length > 0
                onClicked: !updater.running ? updater.start() : updater.stop()
                contentItem: Label {
                    anchors.fill: parent
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    text: connectButton.text
                    opacity: connectButton.enabled ? 1.0 : 0.3// is this working? in UI sense?
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
                            text: "Antialiasing Shapes"
                            checked: figmaQml.flags & FigmaQml.AntialiasingShapes
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.AntialiasingShapes
                                else
                                    figmaQml.flags &= ~FigmaQml.AntialiasingShapes
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
                        QtCheckBox {
                            text: "Qt for MCU"
                            visible: has_qul
                            checked: figmaQml.flags & FigmaQml.QulMode
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.QulMode
                                else
                                    figmaQml.flags &= ~FigmaQml.QulMode
                                figmaQml.reset(false, false, false, false); // on change keep nothing
                            }
                        }
                        QtCheckBox {
                            text: "Static QML"
                            checked: figmaQml.flags & FigmaQml.StaticCode
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.StaticCode
                                else
                                    figmaQml.flags &= ~FigmaQml.StaticCode
                            }
                        }
                        QtCheckBox {
                            text: "Cyan background"
                            checked: false
                            onCheckedChanged: {
                                if(checked)
                                    container.color = "cyan"
                                else
                                    container.color = palette.active.window
                            }
                        }
                        QtCheckBox {
                            text: "No Gradients"
                            visible: has_qul
                            checked: figmaQml.flags & FigmaQml.NoGradients
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.NoGradients
                                else
                                    figmaQml.flags &= ~FigmaQml.NoGradients
                            }
                        }
                        QtCheckBox {
                            text: "Loader placeholders"
                            visible: has_qul
                            checked: figmaQml.flags & FigmaQml.LoaderPlaceHolders
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.LoaderPlaceHolders
                                else
                                    figmaQml.flags &= ~FigmaQml.LoaderPlaceHolders
                            }
                        }
                        QtCheckBox {
                            text: "Render placeholders"
                            visible: has_qul
                            checked: figmaQml.flags & FigmaQml.RenderLoaderPlaceHolders
                            onCheckedChanged: {
                                if(checked)
                                    figmaQml.flags |= FigmaQml.RenderLoaderPlaceHolders
                                else
                                    figmaQml.flags &= ~FigmaQml.RenderLoaderPlaceHolders
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
                contentItem: Label {
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
                                    color: palette.mid
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
                contentItem: Label {
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
                                    color: palette.mid
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
            text: sourceChooser.text === elementName ? figmaQml.sourceCode : figmaQml.componentSourceCode(sourceChooser.text);
            wrapMode: TextEdit.Wrap
            leftPadding: 5

            FontMetrics {
                id: fontMetrics
                font: qmlText.font
                readonly property real numWidth: advanceWidth('0'.repeat(Math.log10(qmlText.lineCount) + 1))
            }
            linePredessor: Label {
                font: qmlText.font
                color: "gray"
                text: lineIndex + 1
                width: fontMetrics.numWidth
            }
        }
        BigText {
            id: figmaSource
            anchors.fill: parent
            text: jsonChooser.text == documentName ? figmaQml.prettyData(figmaGet.data) : figmaQml.prettyData(figmaQml.componentObject(jsonChooser.text));
            wrapMode: TextEdit.WordWrap
        }
        Item {
            clip: true
            anchors.fill: parent
            visible: qmlButton.checked
            Rectangle {
                id: container
                color: palette.active.window
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
                    console.assert(figmaview);
                    console.assert(zoomSlider);
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

                    if(!figmaQml.element || figmaQml.element.length === 0) {
                        errorNote.text = "No element to create!";
                        console.log(figmaQml.isValid);
                        console.log(figmaQml.canvasCount);
                        console.log(figmaQml.elementCount);
                        console.log(figmaQml.sourceCode);
                        return;
                    }

                    try {
                        comp = Qt.createComponent(figmaQml.element, this);
                        if(comp) {
                            const ctor = function() {
                                if (comp.status === Component.Ready) {
                                   figmaview = comp.createObject(container);
                                   figmaQml.componentLoaded(figmaQml.currentCanvas, figmaQml.currentElement);
                                   return true;
                                }
                                if (comp.status === Component.Error) {
                                    console.log(comp.errorString())
                                    throw(comp.errorString());
                                }
                                return false;
                            }
                            if(!ctor())
                                comp.statusChanged.connect(ctor);
                        } else {
                            if(!figmaQml.element || figmaQml.element.length === 0) {
                                errorNote.text = "No element to create!";
                                console.log(figmaQml.isValid);
                                console.log(figmaQml.canvasCount);
                                console.log(figmaQml.elementCount);
                                console.log(figmaQml.sourceCode);
                                return;
                            }
                            errorNote.text = "Cannot create component \"" + figmaQml.element + "\"";
                        }
                    } catch (error) {
                        //There is a reason line numbers wont match, and therefore we try to load a sourceCode
                        error = sourceCodeError(error, container);
                        let errors = "Label {text:\"Error loading a Figma item\";}\n"

                        console.debug("Catch error on create:", error)

                        if(error.qmlErrors) {
                            for (let i = 0; i < error.qmlErrors.length; i++) {
                                errors += "Column {\Label {text:\"" + "line: "
                                        + error.qmlErrors[i].lineNumber  + "\";}\n"
                                        + "Text {text:\"column: "
                                        + error.qmlErrors[i].columnNumber + "\";}\n"
                                        + "Text {text:\"file: "
                                        + fileName(error.qmlErrors[i].fileName) + "\";}\n"
                                        + "Text {text:'message: "
                                        + error.qmlErrors[i].message.split('').map(c=>'\\x' + c.charCodeAt(0).toString(16)).join('') + "';}\n}\n"
                            }
                        } else {
                           errors += "Label {text: \"Unknown error:" + error.replace(/"/g, '\\"') +"\" }";
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
                propagateComposedEvents: true
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
        color: palette.midlight
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
            Label {
                text: documentName
            }
            Label {
                text: canvasName
            }
            Label {
                text: elementName
            }
            Label {
                id: info
            }
            Label {
                id: warning
                color: "red"
                font.bold: true
            }
        }
    }


    function storeFile() {
        if(isWebAssembly) {
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
        if(isWebAssembly) {
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
                    Rectangle {
                        anchors.fill: parent
                        color: "black"
                        radius: 90
                        z: -1
                    }
                    Layout.preferredWidth: 128
                    Layout.preferredHeight: 128
                    pathSize: Qt.size(256, 256)
                    paths: [
                        {fillColor: "black", path: "M142 142h20v10h-10v10h-10v8h-10v-18h10v-10ZM94 142h20v10h10v18h-10v-8h-10v-10H94v-10ZM132 114h10v10h-10v-10ZM114 114h10v10h-10v-10Z"},
                        {fillColor: "fuchsia", path: "M114 86h28v8h10v30h10v18h28v28h-10v10h-18v10h-20v10h66v8h-8v10h-48v-10h-48v10H56v-10h-8v-8h66v-10H94v-10H76v-10H66v-28h28v-18h10V94h10v-8ZM132 28h30v10h8v10h10v28h-10v10h-8v8h-10V48h-10V38h-10V28ZM94 28h30v10h-10v10h-10v46H94v-8h-8V76H76V48h10V38h8V28Z"},

                       ]
                }
            }
            Label {
            text: "FigmaQML, Markus Mertama 2020 - 2024"
            }
            Label {
            text: "Version: " + figmaQmlVersionNumber;
            }
        }
    }

    TokensPopup {
        id: tokens
        anchors.centerIn: main.contentItem
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
            fileSaveDialog.saveAll();
        }
    }

    function saveCurrentQML(asMcu, asApp, elements) {
        if(isWebAssembly) {
            //if(!figmaQml.saveCurrentQMLZipped(
            //            figmaQml.validFileName(documentName),
            //            figmaQml.validFileName(canvasName)))
            errorNote.text = "QML save failed";
        } else {
            fileSaveDialog.saveCurrent(asMcu, asApp, elements);
        }
    }


    FolderDialog {
        id: fileSaveDialog
        title: isSaveAll ? qsTr("Save All QMLs into") : qsTr("Save QML into")
        currentFolder: figmaQml.documentsLocation
        acceptLabel: isSaveAll ? "Save All" : "Save"
        property bool isSaveAll: false
        property list<int> elements
        property bool asApp: false
        property bool asMcu: false
        onAccepted: {
            let path = currentFolder.toString();
            path = path.replace(/^(file:\/\/)/,"");
            path = path.replace(/^(\/(c|C):\/)/, "C:/");
            path += "/" + figmaQml.validFileName(documentName) + "/" + figmaQml.validFileName(canvasName)
            let save_ok = isSaveAll ? figmaQml.saveAllQML(path) : figmaQml.saveQML(asMcu, path, asApp, elements);
            if(!save_ok) {
                errorNote.text = "Cannot save to \"" + path + "\""
            }
        }
        function saveAll() {
            isSaveAll = true
            open();
        }
        function saveCurrent(amcu, aapp, aelements) {
            asMcu = amcu;
            asApp = aapp;
            elements = aelements;
            isSaveAll = false;
            open();
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

    QtForMCUPopup {
        id: qtForMCUPopup
        anchors.centerIn: parent
        onSaveRequest: saveCurrentQML(true, saveAsApp, views)
        onAccepted: figmaQml.executeQul(params, views);

    }

    QtForDesktopPopup {
        id: qtForDesktopPopup
        anchors.centerIn: parent
        onSaveRequest: saveCurrentQML(false, saveAsApp, views)
        onAccepted: figmaQml.executeApp(params, views);
    }

    Dialog {
        id: consolePopup
        property alias text: textArea.text
        property int level: 0
        anchors.centerIn: Overlay.overlay
        height: 500
        width: 700
        onLevelChanged: {
            switch(level) {
            case 0: textArea.color = "red"; break;
            case 1: textArea.color = "yellow"; break;
            case 2: textArea.color = "green"; break;
            case 3: textArea.color = "lightgray"; break;
            default: textArea.color = "lightgray"; break;
            }
        }
        ScrollView {
            anchors.fill: parent
            TextArea {
                id: textArea
                color: "lightgrey"
                background: Rectangle { color: "black"; }
                property bool _at_end: true
                function _last_line_start() {
                    for(let i = textArea.length - 1; i > 0; --i)
                        if(text[i] === '\n')
                            return i + 1;
                    return 0;
                }
                onCursorPositionChanged: {
                    _at_end = cursorPosition >= _last_line_start();
                }
                onTextChanged: {
                    if (_at_end) // move to end only if was not scrolled elsewhere
                        textArea.cursorPosition = _last_line_start(); // to last
                }
            }
        }
        standardButtons: Dialog.Close
    }

    // this function may invoked from C++ side if upon 1st start
    function openTokens() {
        tokens.open();
    }

    Dialog {
        id: send_set_value
        modal: Qt.NonModal
        title: qsTr("Request applyValue")
        contentItem: Item {
            implicitWidth: 400
            implicitHeight: 100
            ColumnLayout {
                TextField {
                   id: send_element
                   Layout.fillWidth: true
                   placeholderText: qsTr("element")
                }
                TextField {
                   id: send_value
                   Layout.fillWidth: true
                   placeholderText: qsTr("value")
                }
            }
        }
        footer: Row {
            DialogButtonBox {
                Button {
                    text: qsTr("Ok")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                }
                Button {
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
                }
                onAccepted: {
                    FigmaQmlSingleton.applyValue(send_element.text, send_value.text)
                    send_set_value.done(Dialog.Accepted)
                }
                onRejected: send_set_value.done(Dialog.Rejected)

                }
        }
    }

    Connections {
        target: FigmaQmlSingleton
        /*
        * The standard Qt Quick offers a new QML syntax to define signal handlers as a function within a Connections type. Qt Quick Ultralite does not support this feature yet, and it does not warn if you use it.
        * https://doc.qt.io/QtForMCUs-2.5/qtul-known-issues.html#connection-known-issues
        */
        onValueChanged: {
            console.log("FigmaQmlSingleton - onValueChanged:", element, value);
        }

        onEventReceived: {
            console.log("FigmaQmlSingleton - Event:", element, event);
        }

        onSetSource: {
            console.log("FigmaQmlSingleton - setSource:", element, source);
        }

        onSetSourceComponent: {
            console.log("FigmaQmlSingleton - setSourceComponent:", element, sourceComponent);
        }
    }

}
