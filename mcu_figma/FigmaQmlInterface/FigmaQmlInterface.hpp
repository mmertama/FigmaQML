#pragma once

#include <string>
#include <vector>
#include <qul/singleton.h>
#include <qul/signal.h>



#define BUG_199 // There is some issue (bug?) in Qt for MCU and cmake definitions are not always working!

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
     * @brief event
     */
    Qul::Signal<void(SignalString element, SignalString value)> event;

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
        if(index >= 0 && index < elements.size()) {
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
    std::string view(int index) const {return index < elements.size() ? elements[index] : std::string{};}

    /**
     * @brief elements, static and sole
     */
    static inline const std::vector<std::string> elements {/*element_declarations*/};

    FigmaQmlSingleton() : currentView{elements[0]}, viewCount{static_cast<int>(elements.size())} {}
};

