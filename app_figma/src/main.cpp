#include <QtGui>
#include <QtQml>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine("qrc:/qml/main.qml");
    return app.exec();
}
