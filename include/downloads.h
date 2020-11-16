#ifndef DOWNLOADS_H
#define DOWNLOADS_H

#include <QObject>
#include <unordered_map>

class QNetworkReply;

using NetworkFunction = std::function <QNetworkReply* ()>;

class Downloads : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 bytesReceived READ bytesReceived NOTIFY bytesReceivedChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY bytesTotalChanged)
    Q_PROPERTY(int downloads READ downloads NOTIFY downloadsChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
public:
    Downloads(QObject* parent);
    Q_INVOKABLE void cancel();
    void reset();
    void setProgress(QNetworkReply* reply, qint64 bytesReceived, qint64 bytesTotal);
    qint64 bytesReceived() const;
    qint64 bytesTotal() const;
    int downloads() const;
    bool downloading() const;
    int activeDownloads() const;
    void monitor(QNetworkReply*, const NetworkFunction& f);
    NetworkFunction monitored(QNetworkReply*);
signals:
    void bytesReceivedChanged();
    void bytesTotalChanged();
    void downloadsChanged();
    void downloadStart();
    void downloadEnd();
    void downloadingChanged();
    void cancelled();
    void tooManyRequests();
private slots:
    void onDestroy(QObject*);
private:
    std::unordered_map<QNetworkReply*, std::tuple<qint64, qint64, NetworkFunction>> m_progresses;
    int m_past = 0;
    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = 0;
};

#endif // DOWNLOADS_H
