#ifndef FUNCTORSLOT_H
#define FUNCTORSLOT_H

#include <QObject>
#include <QTimer>

class Execute : public QObject {
    Q_OBJECT
public:
    explicit Execute(QObject* parent = nullptr) : QObject(parent) {}
    Execute& operator=(const std::function<void ()>& fn) {Q_ASSERT(!mFn); mFn = fn; return *this;}
public slots:
    void execute() {Q_ASSERT(mFn); mFn(); mFn = nullptr;}
private:
    std::function<void ()> mFn = nullptr;
};

class Timeout : public QObject {
    Q_OBJECT
public:
    explicit Timeout(QObject* parent = nullptr) : QObject(parent), mTimer(new QTimer(this)) {
        mTimer->setSingleShot(true);
        QObject::connect(mTimer, &QTimer::timeout, this, &Timeout::onTimeout, Qt::QueuedConnection);
    }
    void set(int ms, const std::function<void ()>& fn) {
        Q_ASSERT(!mTimer->isActive());
        mFn = fn;
        mTimer->start(ms);
    }
    void cancel() {
        mTimer->stop();
    }
private slots:
    void onTimeout() {
        mFn();
    }
private:
    std::function<void ()> mFn = nullptr;
    QTimer* mTimer;
};


#endif // FUNCTORSLOT_H
