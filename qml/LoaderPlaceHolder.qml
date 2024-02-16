import QtQuick

Canvas {
    id: canvas
    opacity: 0.6
    property string name
    clip: true
    onPaint: {
        console.log("do paint canvas");
        var ctx = getContext("2d");
        const sz = 5;
        for(let y = 0; y < height / sz; ++y) {
            for(let x = 0; x < width / sz; ++x) {
                 ctx.fillStyle = (x & 1) ? ((y & 1) ? "black" : "red")  : ((y & 1) ? "red" : "black");
                 ctx.fillRect(x * sz, y * sz, sz, sz);
            }
        }
    }

    Text {
        anchors.centerIn: parent
        color: "yellow"
        text: qsTr("This is a placeholder" + (canvas.name.length > 0 ? " for " + canvas.name : ''));
    }


    Component.onCompleted:  {
        console.log("do request paint canvas");
        requestPaint();
    }
}
