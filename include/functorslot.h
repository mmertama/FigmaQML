#ifndef FUNCTORSLOT_H
#define FUNCTORSLOT_H

#include <QObject>
#include <QTimer>
#include <QHash>

class Execute : public QObject {
    Q_OBJECT
public:
    explicit Execute(QObject* parent = nullptr) : QObject(parent) {}
    Execute& operator=(const std::function<void ()>& fn) {Q_ASSERT(!mFn); Q_ASSERT(fn); mFn = fn; return *this;}
public slots:
    void execute() {Q_ASSERT(mFn); mFn();}
private:
    std::function<void ()> mFn = nullptr;
};

class Timeout : public QObject {
    Q_OBJECT
public:
    explicit Timeout(QObject* parent = nullptr) : QObject(parent) {}

    int pending() const {
        return mTimers.size();
    }

    void set(const QString& id, int ms, const std::function<void ()>& fn) {
        Q_ASSERT(!mTimers.contains(id));
        Q_ASSERT(fn);
        auto t = new QTimer(this);
        mTimers.insert(id, {fn, t});
        t->setSingleShot(true);
        t->start(ms);
        QObject::connect(t, &QTimer::timeout, this, [this, id](){
            auto t = std::get<QTimer*>(mTimers[id]);
            t->deleteLater();
            mTimers.remove(id);
        });
    }

    void cancel(const QString& id) {
        Q_ASSERT(mTimers.contains(id));
        auto t = std::get<QTimer*>(mTimers[id]);
        t->stop();
        t->deleteLater();
        mTimers.remove(id);
        if(mTimers.isEmpty())
            emit purged();
    }

    void reset() {
        const auto keys = mTimers.keys();
        for(const auto& k : keys) {
            cancel(k);
        }
    }
signals:
    void purged();
private:
    QHash<QString, std::tuple<std::function<void ()>, QTimer*>> mTimers;
};


#endif // FUNCTORSLOT_H
