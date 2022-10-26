#ifndef ORDEREDMAP_H
#define ORDEREDMAP_H

#include<QStringList>
#include<QHash>
#include<QVector>

template <typename K, typename V>
class OrderedMap {
public:
    const V& operator[](const K& k) const {
        return m_data[m_index[k]].second;
    }
    QStringList keys() const {
        QStringList lst;
        std::transform(m_data.begin(), m_data.end(), std::back_inserter(lst), [](const auto& p){return p.first;});
        return lst;
    }
    void insert(const K& k, const V& v) {
        m_index.insert(k, m_data.size());
        m_data.append({k, v});
    }
    int size() const {
        return m_index.size();
    }
    typename QVector<QPair<K, V>>::Iterator begin() {
        return m_data.begin();
    }
    typename QVector<QPair<K, V>>::Iterator end() {
        return m_data.end();
    }
    void clear() {
        m_data.clear();
        m_index.clear();
    }
private:
    QVector<QPair<K, V>> m_data;
    QHash<K, int> m_index;
};


#endif // ORDEREDMAP_H
