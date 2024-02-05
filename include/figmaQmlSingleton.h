
#ifndef QMLSINGLETON_H
#define QMLSINGLETON_H

#include <figmaqml.h>
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
    // The FigmaQML app does not contain that much view change logic - this is mostly just a suggestion
    Q_PROPERTY(QString currentView READ currentView NOTIFY currentViewChanged FINAL);

    Q_PROPERTY(int viewCount READ viewCount NOTIFY viewCountChanged)

    void setView(int index)  {
        if(index < viewCount()) {
            const auto el = m_fqml.elements();
            m_fqml.setCurrentCanvas(el[index].toMap()["canvas"].toInt());
            m_fqml.setCurrentElement(el[index].toMap()["element"].toInt());
        }
    }

    FigmaQmlSingleton() = delete;

    Q_INVOKABLE void applyValue(const QString& element, const QString& value) {
        emit valueChanged(element, value);
    }

    Q_INVOKABLE QString view(int index) {
        if(index < viewCount()) {
            const auto el = m_fqml.elements();
            return el[index].toMap()["element_name"].toString();
        }
        return {};
    }
    FigmaQmlSingleton(FigmaQml& fqml) : m_fqml(fqml) {
        QObject::connect(&m_fqml, &FigmaQml::elementsChanged, this, &FigmaQmlSingleton::viewCountChanged);
        QObject::connect(&m_fqml, &FigmaQml::currentCanvasChanged, this, &FigmaQmlSingleton::currentViewChanged);
        QObject::connect(&m_fqml, &FigmaQml::currentElementChanged, this, &FigmaQmlSingleton::currentViewChanged);
        QObject::connect(&m_fqml, &FigmaQml::currentElementChanged, this, [this]() {
            applyExternalLoaders(m_fqml.property("flags").toUInt() & FigmaQml::LoaderPlaceHolders);
        });
    }

    int viewCount() const {
        return m_fqml.elements().size();
    }

    QString currentView() const {
        return
            m_fqml.elementName();
    }
signals:
    void valueChanged(const QString& element, const QString& value);
    void eventReceived(const QString& element, const QString& event);
    void viewCountChanged();
    void currentViewChanged();
    void setSource(const QString& element, const QUrl& source);
    void setSourceComponent(const QString& element, const QQmlComponent& sourceComponent);
    void sourceLoaded(const QString& element);
    void sourceError(const QString& element);
    void viewLoaded(const QString& view);
private:
    // Propagates loader source change accross UI where loaders can than capture it

    void applyExternalLoaders(bool usePlaceHolder) {
        for(const auto& [bytes, name] : m_fqml.externalLoaders()) {
            if(usePlaceHolder) {
                emit setSource(name, QUrl("qrc:///LoaderPlaceHolder.qml"));
            } else {
                const auto component_name = FigmaQml::validFileName("placeholder_" + name + FIGMA_SUFFIX);
                m_fqml.writeQmlFile(component_name, bytes, m_fqml.makeHeader());
                emit setSource(name, component_name + ".qml");
            }
        }
    }
private:
     FigmaQml& m_fqml;
};

QML_DECLARE_TYPE(FigmaQmlSingleton)

#endif // QMLSINGLETON_H
