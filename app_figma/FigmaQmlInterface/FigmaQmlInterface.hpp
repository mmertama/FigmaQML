#pragma once

#include <QObject>
#include <QQmlComponent>

#define REGISTER_FIGMAQML_SINGLETON FigmaQmlSingleton _sc; qmlRegisterSingletonInstance("FigmaQmlInterface", 1, 0, "FigmaQmlSingleton", &_sc);

class FigmaQmlSingleton : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString currentView MEMBER m_currentView NOTIFY currentViewChanged)
    Q_PROPERTY(int viewCount READ viewCount() CONSTANT)
public:

    int viewCount() const  {return static_cast<int>(m_elements.size());}

    static FigmaQmlSingleton* instance() {
        static FigmaQmlSingleton* inst = nullptr;
        if(!inst) {
            inst = new FigmaQmlSingleton();
        }
        return inst;
    };

signals:
   void valueChanged(const QString& element, const QString&  value);
   void setSource(const QString& element, const QString& source);
   void setSourceComponent(const QString& element, const QQmlComponent* sourceComponent);
   void sourceLoaded(const QString& element);
   void viewLoaded(const QString& view);
   void sourceError(const QString& element);
   void eventReceived(const QString& element, const QString& event);
   void intReceived(const QString& key, int value);
   void realReceived (const QString& key, double value);
   void stringReceived(const QString& key, const QString& value);
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
public: // for tooling access
    FigmaQmlSingleton() : m_currentView{m_elements.empty() ? QString{} : m_elements[0]} {}
private:
    /**
     * @brief elements, static and sole
     */
    const std::vector<QString> m_elements {/*element_declarations*/};
    QString m_currentView;
};

