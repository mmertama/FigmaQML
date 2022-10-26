#ifndef FIGMAGET_H
#define FIGMAGET_H

#include <QObject>
#include <QTime>
#include <QSize>
#include <QMutex>
#include <QTimer>
#include <QQueue>
#include <QNetworkReply>
#include <limits>
#include <memory>

class FigmaData;
class Downloads;
class Timeout;
class Execute;

class FigmaGet : public QObject {
    Q_OBJECT
    Q_PROPERTY(QByteArray data READ data NOTIFY dataChanged)
    Q_PROPERTY(QString userToken MEMBER m_userToken NOTIFY userTokenChanged)
    Q_PROPERTY(QString projectToken MEMBER m_projectToken NOTIFY projectTokenChanged)
    Q_PROPERTY(int throttle MEMBER m_throttle NOTIFY throttleChanged)
    using NetworkFunction = std::function <QNetworkReply* ()>;
public:
    explicit FigmaGet(const QString& dataDir, QObject *parent = nullptr);
    ~FigmaGet();
    Q_INVOKABLE void update();
    std::pair<QByteArray, int> getImage(const QString& imageRef,
                                        const QSize& maxSize = QSize(std::numeric_limits<int>::max(),
                                                                     std::numeric_limits<int>::max()));
    std::pair<QByteArray, int> getRendering(const QString& figmaId);
    QByteArray getNode(const QString& figmaId);
    QByteArray data() const;
    Downloads* downloadProgress();
    Q_INVOKABLE bool store(const QString& filename, unsigned flag, const QVariantMap& imports) const;
    Q_INVOKABLE bool restore(const QString& filename);
public slots:
    void reset();
    void cancel();
    void documentCreated();
signals:
    void dataChanged();
    void fetchingChanged(bool fetching);
    void error(const QString& errorString) const;
    void intervalChanged(int interval);
    void imagesPopulated();
    void imageRendered(const QString& figmaId);
    void imageRetrieved(const QString imageRef);
    void nodeRetrieved(const QString& nodeId);
    void projectTokenChanged();
    void userTokenChanged();
    void updateCompleted(bool isUpdated);
    void throttleChanged();
    void restored(unsigned flags, const QVariantMap& imports);
    void replyComplete(const std::shared_ptr<QByteArray>& bytes);
   // void requestFinished(QNetworkReply* reply);
  //  void foobar(QString str);
private:
    using FinishedFunction = std::function<void ()>;
    void monitorReply(QNetworkReply* reply, const std::shared_ptr<QByteArray>& bytes,
                      const FinishedFunction& finalize, bool showProgress = true);
    void queueCall(const NetworkFunction& call);
    QByteArray image(const QString& imageRef, const QByteArray& imageData) const;
    bool write(QDataStream& stream, unsigned flag, const QVariantMap& imports) const;
    bool read(QDataStream& stream);
 //   void foobared(QString str);
private slots:
     void replyCompleted(const std::shared_ptr<QByteArray>& bytes);
     void doCall();
     void doFinished(QNetworkReply* reply);
     void onError(const QString& errorStr);
     void onReplyError(QNetworkReply::NetworkError err);
     void replyReader();
private:
    QNetworkReply* doPopulateImages();
    QNetworkReply* doRequestRendering();
    void doRetrieveNode(const QString& id);
    QNetworkReply* doRetrieveImage(const QString& id,  FigmaData* target, const QSize& maxSize);
    bool ensurePopulated(const QString &imageRef);
    void retrieveImage(const QString& id,  FigmaData* target, const QSize& maxSize = QSize(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
    void requestRendering(const QString& imageId);
    void retrieveNode(const QString& id);
    void populateImages();
    //   void timerEvent(QTimerEvent *event) override;
private:
    enum class State {Loading, Complete, Error};
    QNetworkAccessManager* m_accessManager;
    Timeout* m_timeout;
    Execute* m_error;
    Downloads* m_downloads;
    QString m_projectToken;
    QString m_userToken;
    QByteArray m_data;
    unsigned m_checksum = 0;
    std::unique_ptr<FigmaData> m_images;
    std::unique_ptr<FigmaData> m_renderings;
    std::unique_ptr<FigmaData> m_nodes;
    std::atomic_bool m_populationOngoing = false;
    int m_throttle = 300; //Idea of throttle is collect requests into queue and bunches to reduce especially renderig requests
    QQueue<NetworkFunction> m_callQueue;
    QTimer m_callTimer;
    QStringList m_rendringQueue;
    State m_connectionState = State::Loading;
    QMap<QNetworkReply*, std::tuple<std::shared_ptr<QByteArray>, FinishedFunction>> m_replies;
    std::function<void (const QString&)> m_lastError = nullptr;

};

#endif // FIGMAGET_H
