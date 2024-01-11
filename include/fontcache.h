#ifndef FONTCACHE_H
#define FONTCACHE_H

#include <QMutex>
#include <QHash>
#include <QMutexLocker>

class FontCache {
public:
    void insert(const QString& key, const QString& value) {
        QMutexLocker lock(&m_mutex);
        if(m_fontMap.contains(key)) {
            m_fontMap[key] = value;
        } else {
            m_fontMap.insert(key, value);
        }
    }

    bool contains(const QString& key) const {
        QMutexLocker lock(&m_mutex);
        return m_fontMap.contains(key);
    }

    QString operator[](const QString& key) const {
        return m_fontMap[key];
    }

    QVector<QPair<QString, QString>> content() const {
        QMutexLocker lock(&m_mutex);
        QVector<QPair<QString, QString>> c;
        const auto keys = m_fontMap.keys();
        for(const auto& k : keys) {
            c.append({k, m_fontMap[k]});
        }
        return c;
    }
    void clear() {
        QMutexLocker lock(&m_mutex);
        m_fontMap.clear();
    }
private:
     QHash<QString, QString> m_fontMap;
     mutable QMutex m_mutex;
};


#endif // FONTCACHE_H
