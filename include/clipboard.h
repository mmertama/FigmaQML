#ifndef CLIPBOARD_H
#define CLIPBOARD_H


#include <QClipboard>
#include <QGuiApplication>

class Clipboard : public QObject {
    Q_OBJECT
public:
    explicit Clipboard(QObject* parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE void copy(const QString& text) const {
        QGuiApplication::clipboard()->setText(text);
    }
};

#endif // CLIPBOARD_H
