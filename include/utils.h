#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QTextStream>

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

#ifndef QT5
inline QByteArray& operator+=(QByteArray& ba, const QString& qstr) {
    ba += qstr.toUtf8();
    return ba;
}
#endif

#endif // UTILS_H
