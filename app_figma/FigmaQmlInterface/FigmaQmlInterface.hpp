#pragma once

#include <QObject>
#include <QQmlComponent>
#include <QQmlApplicationEngine>

/**
 * @brief Singleton to access FigmaQml properties and communicate between views.
 * 
 */
class FigmaQmlSingleton : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    /// Name of currently active view
    Q_PROPERTY(QString currentView MEMBER m_currentView NOTIFY currentViewChanged)
    /// Number of views
    Q_PROPERTY(int viewCount READ viewCount() CONSTANT)
public:
    /**
     * @brief Number of available views
     * 
     * @return int 
     */
    int viewCount() const  {return static_cast<int>(m_elements.size());}

    /**
     * @brief Static to fetch this instance from engine
     * 
     * @param engine 
     * @return FigmaQmlSingleton* 
     */
    static inline FigmaQmlSingleton* instance(QQmlEngine& engine);

signals:
   /// @brief emitted on applyValue
   /// @param element 
   /// @param value
   void valueChanged(const QString& element, const QString& value);
   /// @brief emitted for asLoader source change 
   /// @param element 
   /// @param source 
   void setSource(const QString& element, const QString& source);
   /// @brief emitted for asLoader component change
   /// @param element 
   /// @param sourceComponent 
   void setSourceComponent(const QString& element, const QQmlComponent* sourceComponent);
   /// @brief emitted on asLoader has changed
   /// @param element 
   void sourceLoaded(const QString& element);
   /// @brief emitted on view has changed
   /// @param view 
   void viewLoaded(const QString& view);
   /// @brief emitted on asLoader component error
   /// @param element 
   void sourceError(const QString& element);
   /// @brief emitted on onClick
   /// @param element 
   /// @param event 
   void eventReceived(const QString& element, const QString& event);
   /// @brief emitted on integer value is passed to view
   /// @param key 
   /// @param value 
   void intReceived(const QString& key, int value);
   /// @brief emitted on real value is passed to view
   /// @param key 
   /// @param value 
   void realReceived (const QString& key, double value);
   /// @brief emitted on string value is passed to view
   /// @param key 
   /// @param value 
   void stringReceived(const QString& key, const QString& value);
   /// @brief emitted on object value is passed to view (QML JS object)
   /// @param key 
   /// @param value 
   void objectReceived(const QString& key, const QVariantMap& value);
   /// @brief emitted on array value is passed to view (QML JS array)
   /// @param key 
   /// @param value 
   void arrayReceived(const QString& key, const QVariantList& value);
   /// @brief emitted when current view has loaded and changed
   void currentViewChanged();
public:
    /**
     * @brief applyValue
     * @param element
     * @param value
     */
    Q_INVOKABLE void applyValue(const QString& element, const QString& value) {emit valueChanged(element, value);}

    /**
     * @brief setView
     * @param index
     */
    Q_INVOKABLE bool setView(int index) {
        if(index >= 0 && index < static_cast<int>(m_elements.size())) {
            const auto view = m_elements[index];
            if(view != m_currentView) {
                m_currentView = view;
                emit currentViewChanged();
            }
            return true;
        }
        return false;
    }

    /**
     * @brief view
     * @param index
     * @return
     */
    Q_INVOKABLE QString view(int index) const {return index < static_cast<int>(m_elements.size()) ? m_elements[index] : QString{};}

    /**
     * Mediator functions - see https://doc.qt.io/QtForMCUs-2.5/qml-qtquick-loader.html
     * ...but more flexible as key value enables address unlike just a value exchange
     **/

    /** @brief sendInt
     * @param key
     * @param value
     */
    Q_INVOKABLE void sendInt(const QString& key, int value) {emit intReceived(key, value);}

    /**
     * @brief intReceived
     */


    /**
     * @brief sendReal
     * @param key
     * @param value
     */
    Q_INVOKABLE void sendReal(const QString& key, double value) {emit realReceived(key, value);}

    /**
     * @brief sendString
     * @param key
     * @param value
     */
    Q_INVOKABLE void sendString(const QString& key, const QString& value) {emit stringReceived(key, value);}

    /**
     * @brief sendObject
     * @param key
     * @param value
     */
    Q_INVOKABLE void sendObject(const QString& key, const QVariantMap& value) {emit objectReceived(key, value);}

    /**
     * @brief sendArray
     * @param key
     * @param value
     */
    Q_INVOKABLE void sendArray(const QString& key, const QVariantList& value) {emit arrayReceived(key, value);}

public: // for tooling access
    FigmaQmlSingleton() : m_currentView{m_elements.empty() ? QString{} : m_elements[0]} {}
private:
    /**
     * @brief elements, static and sole
     */
    const std::vector<QString> m_elements {/*element_declarations*/};
    QString m_currentView;
};

namespace FigmaQmlInterface {
    /**
     * @brief Register FigmaQmlSingleton into engine's context as a singleton
     * 
     * @param engine 
     */
inline
void registerFigmaQmlSingleton(QQmlApplicationEngine& engine) {
    qmlRegisterSingletonType<FigmaQmlSingleton>("FigmaQmlInterface", 1, 0, "FigmaQmlSingleton", [](QQmlEngine *, QJSEngine *) {
        return new FigmaQmlSingleton();
    });
    engine.addImportPath(":/");
}
}

inline FigmaQmlSingleton* FigmaQmlSingleton::instance(QQmlEngine& engine) {
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    const auto id = qmlTypeId("FigmaQmlInterface", 1, 0, "FigmaQmlSingleton");
    auto* singleton = engine.singletonInstance<FigmaQmlSingleton*>(id);
#else
    auto* singleton = engine.singletonInstance<FigmaQmlSingleton*>("FigmaQmlInterface", "FigmaQmlSingleton");
#endif
    return singleton;
}

