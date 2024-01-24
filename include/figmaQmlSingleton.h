
#ifndef QMLSINGLETON_H
#define QMLSINGLETON_H

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <functional>

/**
 * @brief The FigmaQmlSingleton class
 *
 * Fallback for generated code - no real functionality
 *
 */



class FigmaQmlSingleton : public QObject {
    Q_OBJECT
public:
    FigmaQmlSingleton() = default;
    Q_INVOKABLE void applyValue(const QString& element, const QString& value) {
        emit valueChanged(element, value);
    }
signals:
    void valueChanged(const QString& element, const QString& value);
};

QML_DECLARE_TYPE(FigmaQmlSingleton)

#endif // QMLSINGLETON_H
