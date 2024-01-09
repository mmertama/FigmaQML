pragma Singleton
import QtQuick

/**

  For compatiblity with Qt for MCU

*/


QtObject {
    //property int putter: 10
    //property var onSetValue
    signal setValue(string context, string value)
    function requestValue(context, value) {
        setValue(context, value)
    }
//    function onSetValue( context,  value) {
//
//    }
}

