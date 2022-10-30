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
    auto keys() const {
        QStringList lst;
        std::transform(m_data.begin(), m_data.end(), std::back_inserter(lst), [](const auto& p){return p.first;});
        return lst;
    }
    void insert(const K& k, const V& v) {
        m_index.insert(k, m_data.size());
        m_data.append({k, v});
    }
    auto size() const {
        return m_index.size();
    }
    auto begin() {
        return m_data.begin();
    }
    auto end() {
        return m_data.end();
    }
    auto begin() const {
        return m_data.cbegin();
    }
    auto end() const {
        return m_data.cend();
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
