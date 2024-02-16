#ifndef FIGMADATA_H
#define FIGMADATA_H

#include <QString>
#include <QHash>
#include <QDataStream>
#include <QMutex>
#include <tuple>

//TODO: Change to QReadWriteLock - for perf?
#define MUTEX_LOCK(m) QMutexLocker _l(&m);

class FigmaData {
    enum class State {Empty, Pending, Error, Committed};
public:
    bool contains(const QString& key) const {
        MUTEX_LOCK(m_mutex);
        return m_data.contains(key);
    }

    bool isEmpty(const QString& key) const {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        return m_data[key].state != State::Committed;
    }

    bool isError(const QString& key) const {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        return m_data[key].state == State::Error;
    }

    QByteArray data(const QString& key) const {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data[key].state == State::Committed);
        Q_ASSERT(m_data.contains(key));
        return m_data[key].data;
    }

    int format(const QString& key) const {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        return m_data[key].format;
    }

    void insert(const QString& key) {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(!m_data.contains(key));
        m_data.insert(key, {{}, {}, 0, State::Empty});
    }

    void setUrl(const QString& key, const QString& url) {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        Q_ASSERT(m_data[key].url.isEmpty());
        Q_ASSERT(m_data[key].state != State::Error);
        m_data[key].url = url;
    }

    bool isPending(const QString& key) const {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        Q_ASSERT(m_data[key].state != State::Error);
        return m_data[key].state == State::Pending;
    }

    QString url(const QString& key) const {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        Q_ASSERT(m_data[key].state != State::Error);
        return m_data[key].url;
    }
    //Atomic get and set
    bool setPending(const QString& key) {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        const auto wasPending = m_data[key].state == State::Pending;
        if(wasPending)
            return false;
        Q_ASSERT(m_data[key].state == State::Empty);
        m_data[key].state = State::Pending;
        return true;
    }

    void setError(const QString& key) {
        MUTEX_LOCK(m_mutex);
        Q_ASSERT(m_data.contains(key));
        Q_ASSERT(m_data[key].state != State::Committed);
        m_data[key].state = State::Error;
    }

    void setBytes(const QString& key, const QByteArray& bytes, int meta =  0) {
        Q_ASSERT(m_data[key].state == State::Pending);
        m_data[key].data = bytes;
        m_data[key].format = meta;
        m_data[key].state = State::Committed;
    }
    QStringList keys() const {
        MUTEX_LOCK(m_mutex);
        return m_data.keys();
    }

    void clean(bool clean_errors) {
        MUTEX_LOCK(m_mutex);
        for(auto& e : m_data)
            if(e.state != State::Committed && (clean_errors || e.state != State::Error)) {
                e.state = State::Empty;
            }

    }

    void clear(){
        m_data.clear();
    }

    int size() const {
        return m_data.size();
    }

    void write(QDataStream& stream) const {
        const int size = std::accumulate(m_data.begin(), m_data.end(), 0, [](const auto &a, const auto& c){return c.state != State::Committed ? a : a + 1;});
        stream << size;
        const auto keys = m_data.keys();
        for(const auto& key : keys) {
            if(m_data[key].state == State::Committed) {
                stream
                        << key
                        << m_data[key].url
                        << m_data[key].data
                        << m_data[key].format
                        << m_data[key].state;
            }
        }
    }

    void read(QDataStream& stream) {
        int size;
        stream >> size;
        clear();
        for(int i = 0; i < size; i++) {
            QString key;
            QString d1;
            QByteArray d2;
            int format;
            State state;
            stream >> key;
            stream >> d1;
            stream >> d2;
            stream >> format;
            stream >> state;
            m_data.insert(key, {d1, d2, format, state});
        }
    }
private:
    struct Data {
        QString url;
        QByteArray data;
        int format;
        State state;
    };
    QHash <QString, Data > m_data;
    mutable QMutex m_mutex;
};


#endif // FIGMADATA_H
