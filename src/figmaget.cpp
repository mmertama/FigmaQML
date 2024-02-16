#include "figmaget.h"
#include "figmadata.h"
#include "functorslot.h"
#include "downloads.h"
#include "utils.h"
#include <QQmlEngine>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImageReader>
#include <QImageWriter>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QAbstractEventDispatcher>
#include <memory>


#include <QThread>

using namespace std::chrono_literals;

constexpr auto NOT_FOUND_ERR = "%1 not found \"%2\"";
constexpr auto TIMEOUT_ERR = "%1 timeout \"%2\"";

constexpr auto TimeoutTime = 60 * 1000;

constexpr auto ImageRetry = 60 * 1000;

enum Format {
    None = 0, JPEG, PNG
};

const QLatin1String StreamId("FQ03");

// otherwise id can conflict
QString asTimeoutId(const QString& id) {
    return id + "_timeout";
}

#ifdef QT_DEBUG
static QSet<QString> FetchFailedDebug;
#endif

/*
#define FOO  RAII_ r{[fn = __FUNCTION__](){qDebug() << "FOO Out - " << fn; }}; qDebug() << "FOO: In - " << __FUNCTION__
#define FOOBAR ; RAII_ r{[fn = __FUNCTION__, ln = __LINE__ ](){qDebug() << "FOO ouT - " << fn << ln; }}; qDebug() << "FOO: iN - " << __FUNCTION__ << __LINE__
*/

FigmaGet::FigmaGet(QObject *parent) : FigmaProvider(parent),
    m_accessManager(new QNetworkAccessManager(this)),
    m_timeout{new Timeout(this)},
    m_error{new Execute(this)},
    m_downloads(new Downloads(this)),
    m_images(new FigmaData),
    m_renderings(new FigmaData),
    m_nodes(new FigmaData) {

     qmlRegisterUncreatableType<FigmaGet>("FigmaGet", 1, 0, "FigmaGet", "");

    QObject::connect(this, &FigmaGet::projectTokenChanged, this, &FigmaGet::reset);

    QObject::connect(m_downloads, &Downloads::cancelled, this, [this]() {
         m_checksum = 0;
     });

     QObject::connect(&m_callTimer, &QTimer::timeout, this, &FigmaGet::doCall, Qt::QueuedConnection);

     QObject::connect(this, &FigmaGet::error, [this](const QString&) {
         cancel();
         m_connectionState = State::Error;
         m_images->clean(false);
         m_renderings->clean(false);
         m_nodes->clean(false);
     });

     //cancel -> downloads cancel -> error -> cancel loop
     //QObject::connect(m_downloads, &Downloads::cancelled, this, [this]() {
     //    emit error("Downloads cancel");
     //});

     QObject::connect(this, &FigmaGet::replyComplete, this, &FigmaGet::replyCompleted, Qt::QueuedConnection);

     QObject::connect(m_accessManager, &QNetworkAccessManager::finished, this, &FigmaGet::doFinished, Qt::UniqueConnection);

     QObject::connect(this, &FigmaGet::error, this, [this](const QString& err_str) {
         if(m_lastError)
             m_lastError(err_str);
         m_lastError = nullptr;
     });

     QObject::connect(this, &FigmaGet::imageRetrieved, this, &FigmaGet::onRetrievedImage, Qt::QueuedConnection);
     QObject::connect(this, &FigmaGet::nodeRetrieved, this, &FigmaGet::onRetrievedNode, Qt::QueuedConnection);

     m_accessManager->setAutoDeleteReplies(true);
}

void FigmaGet::onRetrievedImage(const QString& imageRef) {
    assert(FetchFailedDebug.find(imageRef) == FetchFailedDebug.end());
    if(m_images->contains(imageRef)) {
        if(!m_images->isEmpty(imageRef)) {
            emit imageReady(imageRef, m_images->data(imageRef), m_images->format(imageRef));
        } else {
#ifdef  QT_DEBUG
            FetchFailedDebug.insert(imageRef);
#endif
            m_images->setError(imageRef);
            emit error(QString("Image cannot be retrieved \"%1\"").arg(imageRef));
        }
    }
    else if(m_renderings->contains(imageRef)) {
        if(!m_renderings->isEmpty(imageRef)) {
            emit renderingReady(imageRef, m_renderings->data(imageRef), m_renderings->format(imageRef));
        } else {
            m_renderings->setError(imageRef);
            emit error(QString("Rendering cannot be retrieved \"%1\"").arg(imageRef));
        }
    }
}

void FigmaGet::onRetrievedNode(const QString& nodeId) {

     if(!m_nodes->isEmpty(nodeId)) {
         emit nodeReady(m_nodes->data(nodeId));
     } else {
         m_nodes->setError(nodeId);
         emit error(QString("Node cannot be retrieved \"%1\"").arg(nodeId));
     }
}

bool FigmaGet::isReady() {

    return m_callQueue.isEmpty() && m_timeout->pending() == 0;
}

void FigmaGet::doFinished(QNetworkReply* rep)
{
    if(rep->error() == QNetworkReply::NoError) { // error handled after this
        auto& reply_data = m_replies[rep];
        auto data = std::get<std::shared_ptr<QByteArray>>(reply_data);
        *data += rep->readAll();
        Q_ASSERT(rep->isFinished());
        std::get<FinishedFunction>(reply_data)();
        m_replies.remove(rep);
    }
}

void FigmaGet::retrieveImage(const Id& id,  FigmaData* target, const QSize& maxSize) {
    assert(FetchFailedDebug.find(id.id) == FetchFailedDebug.end());
    Q_ASSERT(maxSize.width() > 0 && maxSize.height() > 0);
    queueCall([this, id, target, maxSize]() {
        return doRetrieveImage(id, target, maxSize);
    });
}

 void FigmaGet::requestRendering(const Id& imageId) {

     if(!imageId.isEmpty()) {
        m_rendringQueue.append(imageId.id);
     }
     queueCall([this, imageId](){
         return FigmaGet::doRequestRendering(imageId);
     });
 }



void FigmaGet::retrieveNode(const Id& id) {
     queueCall([this, id]() {
         doRetrieveNode(id);
         return nullptr;
     });
 }

bool FigmaGet::store(const QString& filename, unsigned flags, const QVariantMap& imports) {
#ifdef Q_OS_WINDOWS
    QFile file(filename.startsWith('/') ? filename.mid(1) : filename);
#else
    QFile file(filename);
#endif
    if(file.open(QIODevice::WriteOnly)) {
        QDataStream stream(&file);
        if(!write(stream, flags, imports)) {
            emit error("Store failed " + filename);
            return false;
        }
    } else {
      emit error("Store error: " + file.errorString() + " "  + filename);
      return false;
    }
    return true;
}

FigmaGet::~FigmaGet() {
}

bool FigmaGet::restore(const QString& filename) {
#ifdef Q_OS_WINDOWS
    QFile file(filename.startsWith('/') ? filename.mid(1) : filename);
#else
    QFile file(filename);
#endif
    if(file.open(QIODevice::ReadOnly)) {
        QDataStream stream(&file);
        if(!read(stream)) {
            emit error("Restore failed on " + filename);
            return false;
        }
        if(stream.status() != QDataStream::Ok) {
            emit error("Restore file corrupted, " + filename);
            return false;
        }
    } else {
        emit error("Restore error: " + file.errorString() + " "  + filename);
        return false;
      }
    return true;
}

bool FigmaGet::write(QDataStream& stream, unsigned flags, const QVariantMap& imports) const {

    stream << QString(StreamId);
    stream << m_projectToken;
    stream << m_data;
    stream << m_checksum;
    stream << flags;
    stream << imports;


    m_images->write(stream);
    m_renderings->write(stream);
    m_nodes->write(stream);
    return stream.status() == QDataStream::Ok;
}

bool FigmaGet::read(QDataStream& stream) {

    reset();
    QString streamid;
    stream >> streamid;

    if(streamid  != StreamId)
        return false;

    stream >> m_projectToken;
    emit projectTokenChanged();

    stream >> m_data;
    stream >> m_checksum;
    unsigned flags;
    stream >> flags;

    QVariantMap imports;
    stream >> imports;

    m_images->read(stream);
    m_renderings->read(stream);
    m_nodes->read(stream);

    emit restored(flags, imports);
    return stream.status() == QDataStream::Ok;
}

void FigmaGet::reset() {
    m_timeout->reset();
    m_callTimer.stop();
    m_downloads->reset();
    m_images->clear();
    m_renderings->clear();
    m_nodes->clear();
    m_callQueue.clear();
    m_rendringQueue.clear();
    m_replies.clear();
    m_lastError = nullptr;
}

void FigmaGet::cancel() {

    const auto replies = m_replies.keys();
    for(auto& r : replies) {
        m_replies.remove(r);
        r->close();
    }
    m_downloads->cancel();
}

void FigmaGet::doCall() {
    if(m_callQueue.isEmpty()) {

        m_callTimer.stop();
    }
    else {

        const auto call = m_callQueue.dequeue();
        auto reply = call();
        m_downloads->monitor(reply, call);
    }
}

void FigmaGet::queueCall(const NetworkFunction& call) {

    m_callQueue.enqueue(call);
#ifndef NO_THROTTLED_CALL
    if(!m_callTimer.isActive()) {
        m_callTimer.start(m_throttle);
    }
#else
    doCall();
#endif
}

QByteArray FigmaGet::data() const {

    return m_data;
}

void FigmaGet::setError(const Id& imageRef, const QString& reason) {
    QString type;
    switch (imageRef.type) {
    case IdType::IMAGE:
        m_images->setError(imageRef.id);
        type = "Image";
        break;
    case IdType::RENDERING:
        m_renderings->setError(imageRef.id);
        type = "Rendering";
        break;
    case IdType::NODE:
        m_nodes->setError(imageRef.id);
        type = "Node";
        break;
    }
    emit error(QString(reason).arg(type, imageRef.id));
}



void FigmaGet::setTimeout(const std::shared_ptr<QMetaObject::Connection>& connection, const Id& id) {
    m_timeout->set(id.id, TimeoutTime, [this, id, connection]() {
                if(connection.use_count() > 1)
                    setError(id, TIMEOUT_ERR);
            });
}

void FigmaGet::setTimeout(QNetworkReply* reply, const Id& id) {
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(reply, &QNetworkReply::finished, this, [id, this](){
        m_timeout->cancel(id.id);
    });
    m_timeout->set(id.id, TimeoutTime, [this, id, connection]() {

                if(connection.use_count() > 1)
                    setError(id, TIMEOUT_ERR);
            });
}

std::tuple<int, int, int> FigmaGet::cacheInfo() const {
    return {m_images->size(), m_renderings->size(), m_nodes->size()};
}


void FigmaGet::getImage(const QString& imageRef, const QSize& maxSize) {

    assert(FetchFailedDebug.find(imageRef) == FetchFailedDebug.end());

    Q_ASSERT(maxSize.width() > 0 && maxSize.height() > 0);
    Q_ASSERT(!imageRef.isEmpty());

    if(!m_images->contains(imageRef)) {
        auto connection = std::make_shared<QMetaObject::Connection>();
        const auto tid = Id{asTimeoutId(imageRef), IdType::IMAGE};
        setTimeout(connection, tid);
        *connection = QObject::connect(this, &FigmaGet::imagesPopulated, [this, connection, imageRef, maxSize, tid]() {
            m_timeout->cancel(tid.id);
            QObject::disconnect(*connection);
            if(m_images->contains(imageRef)) {
                getImage(imageRef, maxSize);
            } else {
                setError({imageRef, IdType::IMAGE}, NOT_FOUND_ERR);
            }
        });
        if(!m_populationOngoing) //just wait population
            populateImages();
        return;
    }

    if(!m_images->isEmpty(imageRef)) {
        emit imageReady(imageRef, m_images->data(imageRef), m_images->format(imageRef));
        return;
    }

    if(!m_images->setPending(imageRef)) {
        return; // already waiting for a fetch
    }

     retrieveImage({imageRef, IdType::IMAGE}, m_images.get(), maxSize);
}



QNetworkReply* FigmaGet::doRetrieveImage(const Id& id, FigmaData *target, const QSize &maxSize) {
    assert(FetchFailedDebug.find(id.id) == FetchFailedDebug.end());
    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, false);
    const QUrl uri = target->url(id.id);

    qDebug() << "doRetrieveImage" << enumToString(id.type) << id.id << id.isEmpty() << uri;

    if(!uri.isValid()) {
        setError(id, "%1 %2" + QString(" Url not valid \"%1\"").arg(uri.toString()));
        return nullptr;
    }

    request.setUrl(uri);
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());
    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    const auto finished = [this, bytes, target, maxSize, id]() {
        QBuffer imageBuffer(bytes.get(), this);
        QImageReader imageReader(&imageBuffer);
        const auto format = imageReader.format();
        if(!(format == "png" || format == "jpeg" || format == "jpg")) {
            setError(id, "%1 %2" + QString("format not supported \"%1\"").arg(QString(format)));
            return;
        }

            if(maxSize.width() < std::numeric_limits<int>::max() || maxSize.height() < std::numeric_limits<int>::max()) {
#ifdef DUMP_IMAGE
#pragma message("DUMP_IMAGE is defined, Do dump for every rendering...")
            const auto dumpImage = imageReader.read();
            dumpImage.save("figma_+ " + id + "." + imageReader.format());
#endif
            if(imageReader.size().width() > maxSize.width() || imageReader.size().height() > maxSize.height()) {

                const auto image = imageReader.read();
                const auto scaled = image.scaled(maxSize, Qt::KeepAspectRatio);
                bytes->clear();

                QBuffer buffer(bytes.get(), this);
                if(scaled.isNull() || !buffer.open(QIODevice::WriteOnly)) {
                    setError(id, "%1 %2" + QString(" cannot be resized to %2x%3").arg(maxSize.width(), maxSize.height()));
                    return;
                }

                QImageWriter writer(&buffer, format);
                if(!writer.write(scaled)) {
                    setError(id, "%1 %2" + QString(" cannot be resized %2")
                                   .arg(writer.errorString()));
                    return;
                }
                buffer.close();
            }
        }
        if(target->isEmpty(id.id) && m_connectionState == State::Loading) {  //there CAN be multiple requests within multithreaded, but we use only first
            Q_ASSERT(format == "png" || format == "jpeg");
            target->setBytes(id.id, *bytes, format == "png" ? PNG : JPEG);
        }
        assert(FetchFailedDebug.find(id.id) == FetchFailedDebug.end());
        emit imageRetrieved(id.id);
    };

    setTimeout(reply, id);
    monitorReply(reply, bytes, finished);
    return reply;
}

QNetworkReply* FigmaGet::populateImages() {


    m_populationOngoing = true;

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    request.setUrl(QUrl("https://api.figma.com/v1/files/" + m_projectToken + "/images"));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    const auto finished = [this, bytes]() {
        RAII_([this](){m_populationOngoing = false;});
        if(bytes->isEmpty()) {
           //qDebug() << "foo" << reply;
           //const auto keys = m_replies.keys();
           emit error("Error on populate - no data");
           return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(*bytes, &err);
        if(err.error != QJsonParseError::NoError) {
               emit error(QString("Error on populate - JSON: %1 at %2")
                        .arg(err.errorString())
                        .arg(err.offset));
            //qDebug() << "Json - size:" << bytes->size() << "dump: " << *bytes;
            return;
        }
        const auto obj = doc.object();
        if(obj.contains("err")) {
            Q_ASSERT(0); // todo
        }
        else if(obj["error"].toBool()) {
            emit error(QString("Error on populate %1").arg(obj["status"].toString()));
        } else {
            const auto meta = obj["meta"].toObject();
            const auto images = meta["images"].toObject();
            for(const auto& key : images.keys()) {
                if(!m_images->contains(key)) {
                    m_images->insert(key);
                    m_images->setUrl(key, images[key].toString());
                }
            }
        }
        emit imagesPopulated();
    };

    monitorReply(reply, bytes, finished);
    return reply;
}

void FigmaGet::getRendering(const QString& imageId) {
    /*if(m_renderings->contains(imageId)) {
    qDebug() << "getRendering" << imageId
             <<   m_renderings->isEmpty(imageId)
             <<   m_renderings->isError(imageId)
             <<   m_renderings->isPending(imageId);
    } else  qDebug() << "getRendering" << imageId << "N/A";
    */

    if(!m_renderings->contains(imageId)) {
        auto connection = std::make_shared<QMetaObject::Connection>();
        const auto tid = Id{asTimeoutId(imageId), IdType::RENDERING};
        m_renderings->insert(imageId);
        setTimeout(connection, tid);
        *connection = QObject::connect(this, &FigmaGet::imageRendered, [this, connection, imageId, tid](const QString&) {
            m_timeout->cancel(tid.id);
            QObject::disconnect(*connection);
            if(m_renderings->contains(imageId)) {
                getRendering(imageId);
            } else {
               setError({imageId, IdType::RENDERING}, NOT_FOUND_ERR);
            }
        });
        requestRendering({imageId, IdType::RENDERING});
        return;
    }

    if(!m_renderings->isEmpty(imageId)) {
        emit renderingReady(imageId, m_renderings->data(imageId), m_renderings->format(imageId));
        return;
    }

    // skip if rendering has occured. TODO if there should be some retries
    if(m_renderings->isError(imageId) || !m_renderings->setPending(imageId)) {
        return; // already waiting for a fetch
    }

     retrieveImage({imageId, IdType::RENDERING}, m_renderings.get(),
                  QSize(std::numeric_limits<int>::max(),
                        std::numeric_limits<int>::max()));
}


QNetworkReply* FigmaGet::doRequestRendering(const Id& id) {

    if(m_rendringQueue.isEmpty())
        return nullptr;

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    const QStringList params{
        "ids=" + m_rendringQueue.join(','),
        "use_absolute_bounds=true"
    };
    m_rendringQueue.clear();

    request.setUrl(QUrl("https://api.figma.com/v1/images/" + m_projectToken + "?" + params.join('&')));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());
    request.setHeader(QNetworkRequest::ContentLengthHeader, 0);

    auto reply = m_accessManager->get(request);
    std::shared_ptr<QByteArray> bytes(new QByteArray);


    QObject::connect(reply, &QNetworkReply::destroyed, this, [this](QObject*) {
           if(!m_rendringQueue.isEmpty()) {
               requestRendering({QString(), IdType::RENDERING}); // type has no meaning
           }
    });


    QObject::connect(reply, &QNetworkReply::errorOccurred, this, [id](const auto err) {
        qDebug() << "doRequestRendering" << id.id << enumToString(id.type)  << " error" << err;
    });

    const auto finished =  [this, bytes, id]() {
        if(bytes->isEmpty()) {
           setError(id, "%1 \"%2\" Error - no data");
           return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(*bytes, &err);
        if(err.error != QJsonParseError::NoError) {
           setError(id, "%1 \"%2\"" + QString("Error on rendering - JSON: %1 at %2")
                    .arg(err.errorString(), err.offset));
            qDebug() << "JSON - size:" << bytes->size() << "dump: " << (bytes ? *bytes : "N/A");
            return;
        }
        const auto obj = doc.object();
        if(obj["error"].toBool()) {
            setError(id, "%1 \"%2\"" + QString("Status %1").arg(obj["status"].toString()));
        } else {
            const auto renderings = obj["images"].toObject();
            for(const auto& key : renderings.keys()) {
                if(renderings[key].toString().isEmpty()) {
                    setError(id, "%1 \"%2\"" + QString("Invalid URL key:\"%1\"").arg(key));
                    break;
                }
                m_renderings->setUrl(key, renderings[key].toString());
                emit imageRendered(key);
            }
        }
    };
    setTimeout(reply, id);
    monitorReply(reply, bytes, finished);
    return reply;
}

void FigmaGet::replyCompleted(const std::shared_ptr<QByteArray>& bytes) {
    const auto checksum = qChecksum(bytes->constData(), bytes->length());
    if(checksum != m_checksum || m_connectionState == State::Error) {
        m_connectionState = State::Loading;
        m_downloads->reset();
        m_downloads->setProgress(nullptr, bytes->length(), bytes->length());
        m_checksum = checksum;
        m_data.swap(*bytes);
        emit dataChanged();
        emit updateCompleted(true);
    } else {
         emit updateCompleted(false);
    }
}

void FigmaGet::update() {


    if(m_downloads->downloading()) {
        emit updateCompleted(false);
        return;
    }

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    const QStringList params{
        {"geometry=paths"}
    };
    request.setUrl(QUrl("https://api.figma.com/v1/files/" + m_projectToken + QChar('?') + params.join('&')));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    std::shared_ptr<QByteArray> bytes(new QByteArray);
    auto reply = m_accessManager->get(request);

    const auto finished =  [reply, this, bytes]() {
        reply->deleteLater();
        QObject::connect(reply, &QObject::destroyed, this, [this, bytes] (QObject*) { //since added after downloads, this is called after
            emit replyComplete(bytes);
        });
    };

    monitorReply(reply, bytes, finished, m_checksum == 0);

}

void FigmaGet::documentCreated() {

    m_connectionState = State::Complete;
}

void FigmaGet::getNode(const QString &id) {

    if(!m_nodes->contains(id)) {
        const QStringList params{
            "ids=" + id,
            "geometry=paths"
        };
        m_nodes->insert(id);
        m_nodes->setUrl(id, "https://api.figma.com/v1/files/" + m_projectToken + "/nodes?" + params.join('&'));
    }

    if(!m_nodes->isEmpty(id)) {
        emit nodeReady(m_nodes->data(id));
        return;
    }

    if(m_nodes->setPending(id))
        return; // already on its way

    retrieveNode({id, IdType::NODE});
}

void FigmaGet::doRetrieveNode(const Id& id) {


    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    const auto uri = m_nodes->url(id.id);
    request.setUrl(uri);
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    const auto finished = [this, bytes, id] () {
        if(m_connectionState == State::Loading)
            m_nodes->setBytes(id.id, *bytes);
        emit nodeRetrieved(id.id);
    };

    setTimeout(reply, id);
    monitorReply(reply, bytes, finished);
}


void FigmaGet::replyReader() {

    const auto replies = m_replies.keys();
    for(auto& reply : replies)
    {
        if(reply->bytesAvailable()) {
            auto data = std::get<std::shared_ptr<QByteArray>>(m_replies[reply]);
            *data += reply->readAll();
            //qDebug() << "F00" << reply << data->size();
        }
    }
}

void FigmaGet::onReplyError(QNetworkReply::NetworkError err) {
    const auto keys = m_replies.keys();
    const auto it = std::find_if(keys.begin(), keys.end(), [err](const auto r) {
        return r->error() == err;
    });
    if(it == keys.end())
        return;
    const auto reply = *it;
    if(err == QNetworkReply::UnknownContentError || err == QNetworkReply::ProtocolInvalidOperationError) { //Too Many Requests
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const auto code = statusCode.isValid() ? statusCode.toInt() : -1;
        if(code == 429 || code == 400) {
            emit m_downloads->tooManyRequests();
            const auto failedCall = m_downloads->monitored(reply);
            QTimer::singleShot(ImageRetry, this, [this, failedCall]() { //figma doc says about one minute
                queueCall(failedCall);
            });
        }  else {
            emit error("HTTP error: " + reply->errorString());
        }
    } else {
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        emit error("Network error: " + QString::number(err) + ", "  + reply->errorString());
        qDebug() << "Network error: " <<  QString::number(err) <<  reply->errorString()
                << reply->request().url()
                << reply->request().rawHeaderList()
                 << statusCode;
    }
    m_replies.remove(reply);
}

void FigmaGet::monitorReply(QNetworkReply* reply,
                            const std::shared_ptr<QByteArray>& bytes,
                            const std::function<void ()>& finalize,
                            bool showProgress) {
    Q_ASSERT(!m_replies.contains(reply));
    m_replies.insert(reply, {bytes, finalize});

    QObject::connect(reply, &QNetworkReply::errorOccurred, this, &FigmaGet::onReplyError, Qt::QueuedConnection);

#ifndef NO_SSL
    QObject::connect(reply, &QNetworkReply::sslErrors,
                     this, [this, reply](const QList<QSslError>&) {
        emit error(reply->errorString());
        reply->deleteLater();
    });
#endif

    if(showProgress) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, this, [reply, this] (qint64 bytesReceived, qint64 bytesTotal) {
            m_downloads->setProgress(reply, bytesReceived, bytesTotal);
        });
    }

    QObject::connect(reply, &QNetworkReply::readyRead, this, &FigmaGet::replyReader, Qt::QueuedConnection);
}

Downloads* FigmaGet::downloadProgress() {

    return m_downloads;
}


std::optional<std::tuple<QByteArray, int>> FigmaGet::cachedImage(const QString& imageRef) {
    if(!m_images->contains(imageRef) ||
            m_images->isEmpty(imageRef) ||
            m_images->isPending(imageRef))
        return std::nullopt;
    return std::make_optional(std::make_tuple(m_images->data(imageRef), m_images->format(imageRef)));
}

std::optional<std::tuple<QByteArray, int>> FigmaGet::cachedRendering(const QString& figmaId) {
    if(!m_renderings->contains(figmaId) ||
            m_renderings->isEmpty(figmaId) ||
            m_renderings->isPending(figmaId))
        return std::nullopt;
    return std::make_optional(std::make_tuple(m_renderings->data(figmaId), m_renderings->format(figmaId)));
}

std::optional<QByteArray> FigmaGet::cachedNode(const QString& figmaId) {
    if(!m_nodes->contains(figmaId) ||
            m_nodes->isEmpty(figmaId) ||
            m_nodes->isPending(figmaId))
        return std::nullopt;
    return std::make_optional(m_nodes->data(figmaId));
}
