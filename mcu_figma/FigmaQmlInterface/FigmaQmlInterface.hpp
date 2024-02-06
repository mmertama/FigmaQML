#pragma once

#include <string>
#include <vector>
#include <qul/singleton.h>
#include <qul/signal.h>


#ifndef BUG_199
#define BUG_199 // There is some issue (bug?) in Qt for MCU and cmake definitions are not always working!
#endif

/// https://bugreports.qt.io/browse/QTMCU-199, until fixed
#ifdef BUG_199
#include "qul/private/unicodestring.h"
using SignalString = Qul::Private::String;
#else
#include <string>
using SignalString = std::string;
#endif

class FigmaQmlSingleton : public Qul::Singleton<FigmaQmlSingleton> {
public:
    /**
     * @brief currentView
     */
    Qul::Property<std::string> currentView;

    /**
     * @brief viewCount
     */
    const Qul::Property<int> viewCount;

    /**
     * @brief valueChanged
     */
    Qul::Signal<void(SignalString element, SignalString value)> valueChanged;

    /**
     * @brief valueChanged
     */
    Qul::Signal<void(SignalString element, SignalString source)> setSource;

#ifdef QUL_CPP_HAS_SOURCE_COMPONENT // I cannot find C++ implementatation of Component for QUL
    /**
     * @brief valueChanged
     */
    Qul::Signal<void(SignalString element, const Qul::Object* sourceComponent)> setSourceComponent;
#endif

    /**
     * @brief valueChanged
     */
    Qul::Signal<void(SignalString element)> sourceLoaded;


    /**
     * @brief valueChanged
     */
    Qul::Signal<void(SignalString view)> viewLoaded;

    /**
     * @brief valueChanged
     */
    Qul::Signal<void(SignalString element)> sourceError;


    /**
     * @brief event
     */
    Qul::Signal<void(SignalString element, SignalString event)> eventReceived;

    /**
     * @brief applyValue
     * @param element
     * @param value
     */
    void applyValue(SignalString element, SignalString value) {valueChanged(element, value);}

    /**
     * @brief setView
     * @param index
     */
    bool setView(int index) {
        if(index >= 0 && index < static_cast<int>(elements.size())) {
            currentView.setValue(elements[index]);
            return true;
        }
        return false;
    }

    /**
     * @brief view
     * @param index
     * @return
     */
    std::string view(int index) const {return index < static_cast<int>(elements.size()) ? elements[index] : std::string{};}

    /**
     * Mediator functions - see https://doc.qt.io/QtForMCUs-2.5/qml-qtquick-loader.html
     * ...but more flexible as key value enables address unlike just a value exchange
     **/

    /** @brief sendInt
     * @param key
     * @param value
     */
    void sendInt(SignalString key, int value) {intReceived(key, value);}

    /**
     * @brief intReceived
     */
    Qul::Signal<void (SignalString key, int value)> intReceived;

    /**
     * @brief sendReal
     * @param key
     * @param value
     */
    void sendReal(SignalString key, double value) {realReceived(key, value);}

    /**
     * @brief realReceived
     */
    Qul::Signal<void (SignalString key, double value)> realReceived;

    /**
     * @brief sendString
     * @param key
     * @param value
     */
    void sendString(const std::string& key, const std::string& value) {stringReceived(key, value);}

    /**
     * @brief stringReceived
     */
    Qul::Signal<void (SignalString key, SignalString value)> stringReceived;

    /**
     * @brief elements, static and sole
     */
    static inline const std::vector<std::string> elements {/*element_declarations*/};

    FigmaQmlSingleton() : currentView{elements[0]}, viewCount{static_cast<int>(elements.size())} {}
};

