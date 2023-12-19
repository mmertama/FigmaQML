#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QTextStream>

#define STRINGIFY0(x) #x
#define STRINGIFY(x) STRINGIFY0(x)

template <typename Last>
QString toStr(const Last& last) {
    QString str;
    QTextStream stream(&str);
    stream << last;
    return str;
}

template <typename First, typename ...Rest>
QString toStr(const First& first, Rest&&... args) {
    QString str;
    QTextStream stream(&str);
    stream << first << " ";
    return str + toStr(args...);
}

class RAII_ {
public:
    using Deldelegate = std::function<void()>;
    RAII_(const Deldelegate& d) : m_d(d) {}
    ~RAII_() {m_d();}
private:
    Deldelegate m_d;
};


#ifndef QT5
inline QByteArray& operator+=(QByteArray& ba, const QString& qstr) {
    ba += qstr.toUtf8();
    return ba;
}
#endif

#endif // UTILS_H
