
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

class AttachedSignal : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
public:
    explicit AttachedSignal(QObject* parent = nullptr) : QObject(parent) {}
signals:
    void setValue(const QString& element, const QString& value);
};

class FigmaQmlSingleton : public QObject {
    Q_OBJECT
    QML_ATTACHED(AttachedSignal)
    QML_SINGLETON
    QML_ELEMENT
public:
    FigmaQmlSingleton() = default;
    FigmaQmlSingleton(QObject *parent) : QObject(parent) {Q_ASSERT(parent);}
    Q_INVOKABLE void requestValue(const QString& element, const QString& value) {
        auto attached = qobject_cast<AttachedSignal*>(qmlAttachedPropertiesObject<AttachedSignal>(this));
        emit attached->setValue(element, value);
    }
    static AttachedSignal* qmlAttachedProperties(QObject *object) {
        return new AttachedSignal(object);
    } ;
};

QML_DECLARE_TYPE(FigmaQmlSingleton)
QML_DECLARE_TYPEINFO(FigmaQmlSingleton, QML_HAS_ATTACHED_PROPERTIES)

#endif // QMLSINGLETON_H
