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
    Qul::Signal<void(SignalString element, SignalString value)> valueChanged;
    void applyValue(SignalString element, SignalString value) {valueChanged(element, value);}
    std::string getView(int index) const {return elements[index];}
    int viewCount() const {return elements.size();}
    const std::vector<std::string> elements {/*element_declarations*/};
};

