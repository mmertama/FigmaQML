#include <QtGui>
#include <QtQml>

#include "FigmaQmlInterface/FigmaQmlInterface.hpp"

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    REGISTER_FIGMAQML_SINGLETON;
    QQmlApplicationEngine engine("qrc:/qml/main.qml");
    return app.exec();
}
