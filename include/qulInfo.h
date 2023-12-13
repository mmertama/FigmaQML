#ifndef QULINFO_H
#define QULINFO_H

#include <QObject>
#include <QString>
#include <QSerialPortInfo>
#include <memory>

class FigmaQml;

class QulInfo: public QObject {
    Q_OBJECT
public:
    static QulInfo* instance(const FigmaQml& figmaQml);
    bool connect(const QSerialPortInfo& info);
    ~QulInfo();
signals:
    void information(const QString& info);
private:
    QulInfo(QObject* parent = nullptr);
    class Private;
    friend class Private;
    std::unique_ptr<Private> m_private;
};

#endif // QULINFO_H
