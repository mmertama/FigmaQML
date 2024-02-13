#ifndef QULINFO_H
#define QULINFO_H

#include <QObject>
#include <QString>
#include <QSerialPortInfo>
#include <QHash>
#include <memory>
#include "execute_utils.h"

class QulInfo: public ExecuteInfo {
    Q_OBJECT
public:
    static QulInfo* instance(const FigmaQml& figmaQml);
    bool connect(const QSerialPortInfo& info);
    ~QulInfo();
private:
    QulInfo(const FigmaQml& figmaQml, QObject* parent = nullptr);
    class Private;
    friend class Private;
    std::unique_ptr<Private> m_private;
};

#endif // QULINFO_H
