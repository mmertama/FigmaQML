#include "downloads.h"
#include <QNetworkReply>
#include <QQmlEngine>
#include <QTimer>




Downloads::Downloads(QObject* parent) : QObject(parent) {
    qmlRegisterUncreatableType<Downloads>("FigmaGet", 1, 0, "Downloads", "");
    QObject::connect(this, &Downloads::cancelled, this, [this]() {
        if(m_progresses.empty())
            return;
        m_progresses.clear();
        const auto clone =  m_progresses;
        for(auto& [r, v] : clone) {
           r->abort();
        }
    }, Qt::QueuedConnection);
}

bool Downloads::downloading() const {
    return !m_progresses.empty();
}

void Downloads::cancel() {
   emit cancelled();
}

void Downloads::reset() {
    m_progresses.clear();
    m_past = 0;
    m_bytesReceived = 0;
    m_bytesTotal = 0;
    emit bytesTotalChanged();
    emit bytesReceivedChanged();
    emit downloadsChanged();
    emit downloadingChanged();
}

void Downloads::setProgress(QNetworkReply* reply, qint64 bytesReceived, qint64 bytesTotal) {
    if(m_progresses.find(reply) != m_progresses.end()) {
        auto& r = m_progresses[reply];
        if(std::get<0>(r) != bytesReceived) {
            std::get<0>(r) = bytesReceived;
            emit bytesReceivedChanged();
        }
        if(std::get<1>(r) != bytesTotal) {
            std::get<1>(r) = bytesTotal;
            emit bytesTotalChanged();
        }
    } else {
        if(reply) {
            QObject::connect(reply, &QNetworkReply::destroyed, this, &Downloads::onDestroy);
            m_progresses.emplace(reply, std::tuple<qint64, qint64, NetworkFunction>{bytesReceived, bytesTotal, nullptr});
            emit downloadsChanged();
        } else {
            m_bytesReceived += bytesReceived;
            m_bytesTotal += bytesTotal;
        }
        emit bytesTotalChanged();
        emit bytesReceivedChanged();
        emit downloadStart();
        emit downloadingChanged();
    }
}

void Downloads::onDestroy(QObject* thisItem) {
    const auto r = static_cast<QNetworkReply*>(thisItem);
    m_bytesReceived += std::get<0>(m_progresses[r]);
    m_bytesTotal += std::get<1>(m_progresses[r]);
    m_progresses.erase(r);
    ++m_past;
    emit downloadEnd();
    emit downloadingChanged();
    emit downloadsChanged();
}

void Downloads::monitor(QNetworkReply* reply, const NetworkFunction &f) {
    if(!reply)
        return;
    if(m_progresses.find(reply) == m_progresses.end()) {
        QObject::connect(reply, &QNetworkReply::destroyed, this, &Downloads::onDestroy);
        m_progresses.emplace(reply, std::tuple<qint64, qint64, NetworkFunction>{0, 0, f});
        emit downloadsChanged();
    } else
        std::get<NetworkFunction>(m_progresses[reply]) = f;
}

NetworkFunction Downloads::monitored(QNetworkReply* reply) {
    return std::get<NetworkFunction>(m_progresses[reply]);
}

qint64 Downloads::bytesReceived() const {
    qint64 sum = 0;
    for(const auto& [r, v] : m_progresses) {
        sum += std::get<0>(v);
    }
    return sum + m_bytesReceived;
}

qint64 Downloads::bytesTotal() const {
    qint64 sum = 0;
    for(const auto& [r, v] : m_progresses) {
        sum += std::get<1>(v);
    }
    return sum + m_bytesTotal;
}

int Downloads::downloads() const {
    return m_progresses.size() + m_past;
}

int Downloads::activeDownloads() const {
    return m_progresses.size();
}
