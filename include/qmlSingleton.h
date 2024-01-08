#ifndef QMLSINGLETON_H
#define QMLSINGLETON_H

#include <QObject>
#include <QString>
#include <QQmlEngine>

/**
 * @brief The FigmaQmlSingleton class
 *
 * Fallback for generated code - no real functionality
 *
 */

class FigmaQmlSingleton : public QObject {
    Q_OBJECT
public:
    FigmaQmlSingleton() : QObject(nullptr) {}
    Q_INVOKABLE void requestValue(const QString& element, const QString& value) {emit setValue(element, value);}
signals:
    void setValue(const QString& element, const QString& value);
};

#endif // QMLSINGLETON_H
