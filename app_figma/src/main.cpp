#include <QtGui>
#include <QtQml>

#include "FigmaQmlInterface/FigmaQmlInterface.hpp"

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    registerFigmaQmlSingleton(engine);
    engine.load("qrc:/qml/main.qml");
    return app.exec();
}
