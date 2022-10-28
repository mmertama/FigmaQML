#include "figmaget.h"
#include "figmadata.h"
#include "functorslot.h"
#include "downloads.h"
#include "common.h"
#include <QQmlEngine>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QImageReader>
#include <QImageWriter>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QAbstractEventDispatcher>
#include <memory>
#include <array>

#include <QThread>

using namespace std::chrono_literals;

#define IMAGE_TIMEOUT

#ifdef IMAGE_TIMEOUT
constexpr auto ImageTimeout = 60 * 1000;
#endif

#ifdef ASSERT_NESTED
int _nested_count = 0;
#endif

constexpr auto ImageRetry = 60 * 1000;

enum Format {
    None = 0, JPEG, PNG
};

const QLatin1String StreamId("FQ03");

class RAII_ {
public:
    using Deldelegate = std::function<void()>;
    RAII_(const Deldelegate& d) : m_d(d) {}
    ~RAII_() {m_d();}
private:
    Deldelegate m_d;
};

extern int _nested_count;
#define NESTED_START qDebug() << " nested in" << _nested_count; if(++_nested_count > 1) std::abort();
#define NESTED_END qDebug() << " nested out" << _nested_count; if(--_nested_count < 0) std::abort();

#define FOO  RAII_ r{[fn = __FUNCTION__](){qDebug() << "FOO Out - " << fn; }}; qDebug() << "FOO: In - " << __FUNCTION__
#define FOOBAR ; RAII_ r{[fn = __FUNCTION__, ln = __LINE__ ](){qDebug() << "FOO ouT - " << fn << ln; }}; qDebug() << "FOO: iN - " << __FUNCTION__ << __LINE__

#define NoData {QByteArray(), 0}

/*
void FigmaGet::foobared(QString str) {
    qDebug() << "Foo" << str
             << m_callTimer.remainingTime() << m_callTimer.isActive()
             << static_cast<bool>(m_foobar) << m_callTimer.timerId()
             << (this->thread() == m_callTimer.thread()->currentThread())
             << (this->signalsBlocked() || m_callTimer.signalsBlocked());
    if(m_callTimer.remainingTime() > 0)
        emit foobar("again");
}
*/
/*
void FigmaGet::timerEvent(QTimerEvent* event)
{
    qDebug() << "FOO timerEvent" << event->timerId();
}
*/

FigmaGet::FigmaGet(const QString& /*dataDir*/, QObject *parent) : FigmaProvider(parent),
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
         m_images->clean();
         m_renderings->clean();
         m_nodes->clean();
     });

     //cancel -> downloads cancel -> error -> cancel loop
     //QObject::connect(m_downloads, &Downloads::cancelled, this, [this]() {
     //    emit error("Downloads cancel");
     //});

     //QObject::connect(this, &FigmaGet::foobar, this, &FigmaGet::foobared, Qt::QueuedConnection);
     QObject::connect(this, &FigmaGet::replyComplete, this, &FigmaGet::replyCompleted, Qt::QueuedConnection);

     QObject::connect(m_accessManager, &QNetworkAccessManager::finished, this, &FigmaGet::doFinished, Qt::UniqueConnection);

     QObject::connect(this, &FigmaGet::error, this, &FigmaGet::onError, Qt::QueuedConnection);

     m_accessManager->setAutoDeleteReplies(true);
     //startTimer(1000);
}

void FigmaGet::onError(const QString& errStr)
{
    if(m_lastError)
        m_lastError(errStr);
    m_lastError = nullptr;
}

bool foobar = false;

void FigmaGet::doFinished(QNetworkReply* rep)
{
    FOO << rep;
    if(rep->error() == QNetworkReply::NoError) { // error handled after this
        auto& reply_data = m_replies[rep];
        auto data = std::get<std::shared_ptr<QByteArray>>(reply_data);
        *data += rep->readAll();
        Q_ASSERT(rep->isFinished());
        std::get<FinishedFunction>(reply_data)();
        m_replies.remove(rep);
    }
}

void FigmaGet::retrieveImage(const QString& id,  FigmaData* target, const QSize& maxSize) {
    Q_ASSERT(maxSize.width() > 0 && maxSize.height() > 0);
    FOO;
    queueCall([this, id, target, maxSize]() {
        FOOBAR;
        return doRetrieveImage(id, target, maxSize);
    });
}

 void FigmaGet::requestRendering(const QString& imageId) {
     FOO;
     if(!imageId.isEmpty()) {
        m_rendringQueue.append(imageId);
     }
     queueCall([this](){
         return FigmaGet::doRequestRendering();
     });
 }


void FigmaGet::populateImages() {
     FOO;
     queueCall([this](){
         FOOBAR;
         return doPopulateImages();
     });
 }


void FigmaGet::retrieveNode(const QString& id) {
     queueCall([this, id]() {
         doRetrieveNode(id);
         return nullptr;
     });
 }

bool FigmaGet::store(const QString& filename, unsigned flags, const QVariantMap& imports) const {
    FOO;
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
    FOO;
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
    FOO;
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
    FOO;
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
    FOO;
    m_downloads->reset();
    m_images->clear();
    m_renderings->clear();
    m_nodes->clear();
}

void FigmaGet::cancel() {
    FOO;
    const auto replies = m_replies.keys();
    for(auto& r : replies) {
        m_replies.remove(r);
        r->close();
    }
    m_downloads->cancel();
}

void FigmaGet::doCall() {
    FOO << m_callQueue.size();
    if(m_callQueue.isEmpty()) {
        FOOBAR;
        m_callTimer.stop();
    }
    else {
        FOOBAR;
        const auto call = m_callQueue.dequeue();
        auto reply = call();
        m_downloads->monitor(reply, call);
    }
}

void FigmaGet::queueCall(const NetworkFunction& call) {
    FOO;
    m_callQueue.enqueue(call);
#ifndef NO_THROTTLED_CALL
    qDebug()
            << "FOO: queue"
            << m_callTimer.isActive()
            << m_throttle
            << m_callTimer.isSingleShot()
            << m_callTimer.remainingTime();

    if(!m_callTimer.isActive()) {
        m_callTimer.start(m_throttle);
    }
#else
    doCall();
#endif
}

QByteArray FigmaGet::data() const {
    FOO;
    return m_data;
}

bool FigmaGet::ensurePopulated(const QString &imageRef) {
    FOO;
    if(!m_images->contains(imageRef)) {
        FOOBAR;
        QEventLoop loop; NESTED_START
#ifdef IMAGE_TIMEOUT
        m_timeout->set(ImageTimeout, [this, imageRef]() {
                            FOOBAR;
                            emit error("Timeou on populate image " + imageRef);
                        });
        RAII_ cancelTimeout([this](){m_timeout->cancel();});
#endif
        m_lastError = [&loop](const QString& errorStr) {
            qDebug() << "Error: " << errorStr;
            loop.quit();
        };

        QObject::connect(this, &FigmaGet::imagesPopulated, &loop, &QEventLoop::quit, Qt::QueuedConnection);

        if(!m_populationOngoing) { //just wait population
            populateImages();
        }

        qDebug() << "FOO:" << "loooping";

        loop.exec(); NESTED_END
        while(loop.processEvents());

        qDebug() << "FOO:" << "looop off";
    }
    return m_images->contains(imageRef);
}



std::pair<QByteArray, int> FigmaGet::getImage(const QString &imageRef, const QSize& maxSize) {
    FOO;
    static bool get_one = false;
    if(get_one)
        return NoData;
    get_one = true;

    Q_ASSERT(maxSize.width() > 0 && maxSize.height() > 0);
    Q_ASSERT(!imageRef.isEmpty());

    QEventLoop p0;
    QTimer::singleShot(300, &p0, [&p0](){qDebug() << "p0"; p0.quit();});
    p0.exec();

    foobar = true;

    if(!ensurePopulated(imageRef)) {
        FOOBAR;
        emit error(QString("Image not found \"%1\"").arg(imageRef));
        return NoData;
    }

    QEventLoop p1;
    QTimer::singleShot(300, &p1, [&p1](){qDebug() << "p1"; p1.quit();});
    p1.exec();

    if(m_images->isEmpty(imageRef)) {
        FOOBAR;

        if(m_images->setPending(imageRef)) {
            QEventLoop loop; NESTED_START
        #ifdef IMAGE_TIMEOUT
            m_timeout->set(ImageTimeout, [this, imageRef]() {
                                FOOBAR;
                                emit error("Timeout on get image " + imageRef);
                            });
            RAII_ cancelTimeout([this](){m_timeout->cancel();});
        #endif

            m_lastError = [&loop](const QString& errorStr) {
                qDebug() << "Error: " << errorStr;
                loop.quit();
            };


            //We trust here that first
            QObject::connect(this, &FigmaGet::imageRetrieved, &loop, [&loop, imageRef](const QString& image) {
                FOOBAR;
                if(imageRef == image) {
                    loop.quit();
                }
            });

            retrieveImage(imageRef, m_images.get(), maxSize);
            loop.exec(); NESTED_END
            while(loop.processEvents());
        }

        if(m_images->isEmpty(imageRef)) {
            FOOBAR;
            emit error(QString("Image cannot be retrieved \"%1\"").arg(imageRef));
            return NoData;
        }
    }
    return {m_images->data(imageRef), m_images->format(imageRef)};
}



QNetworkReply* FigmaGet::doRetrieveImage(const QString& id, FigmaData *target, const QSize &maxSize) {
    FOO;
    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, false);
    const QUrl uri = target->url(id);
    if(!uri.isValid()) {
        FOOBAR;
        emit error(QString("Url not valid \"%1\"").arg(uri.toString()));
        return nullptr;
    }

    request.setUrl(uri);
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());
    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    const auto finished = [reply, this, bytes, target, maxSize, id]() {
        FOOBAR << reply;
        QBuffer imageBuffer(bytes.get(), this);
        QImageReader imageReader(&imageBuffer);
        const auto format = imageReader.format();
        if(!(format == "png" || format == "jpeg" || format == "jpg")) {
            FOOBAR;
            emit error(QString("Image format not supported \"%1\"").arg(QString(format)));
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
                    emit error(QString("Image cannot be resized \"%1\" to %2x%3").arg(id).arg(maxSize.width()).arg(maxSize.height()));
                    return;
                }
                FOOBAR;
                QImageWriter writer(&buffer, format);
                if(!writer.write(scaled)) {
                    emit error(QString("Image cannot be resized \"%1\" %2")
                                   .arg(id)
                                   .arg(writer.errorString()));
                    return;
                }
                buffer.close();
            }
        }
        if(target->isEmpty(id) && m_connectionState == State::Loading) {  //there CAN be multiple requests within multithreaded, but we use only first
            Q_ASSERT(format == "png" || format == "jpeg");
            target->setBytes(id, *bytes, format == "png" ? PNG : JPEG);
        }
        emit imageRetrieved(id);
    };

    monitorReply(reply, bytes, finished);
    return reply;
}

QNetworkReply* FigmaGet::doPopulateImages() {
    FOO;

    m_populationOngoing = true;

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, false);

    request.setUrl(QUrl("https://api.figma.com/v1/files/" + m_projectToken + "/images"));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    const auto finished = [reply, this, bytes]() {
        FOOBAR << reply;
        RAII_([this](){m_populationOngoing = false;});
        if(bytes->isEmpty()) {
           qDebug() << "foo" << reply;
           for(const auto& r : m_replies.keys()) {
               qDebug() << "bar" << r;
               qDebug() << "foobar" <<
                           std::get<std::shared_ptr<QByteArray>>(m_replies[r])->size();
           }
           emit error("Error on populate - no data");
           return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(*bytes, &err);
        if(err.error != QJsonParseError::NoError) {
               emit error(QString("Error on populate - JSON: %1 at %2")
                        .arg(err.errorString())
                        .arg(err.offset));
            qDebug() << "Json - size:" << bytes->size() << "dump: " << *bytes;
            return;
        }
        const auto obj = doc.object();
        if(obj["error"].toBool()) {
            emit error(QString("Error on populate %1").arg(obj["status"].toString()));
        } else {
            const auto meta = obj["meta"].toObject();
            const auto images = meta["images"].toObject();
            for(const auto& key : images.keys()) {
                if(!m_images->contains(key))
                    m_images->insert(key, images[key].toString());
            }
        }
        emit imagesPopulated();
    };

    monitorReply(reply, bytes, finished);
    return reply;
}

std::pair<QByteArray, int> FigmaGet::getRendering(const QString& imageId) {
    FOO;
    if(!m_renderings->contains(imageId)) {
        bool err = false;
        QEventLoop loop; NESTED_START
#ifdef IMAGE_TIMEOUT
         QTimer::singleShot(ImageTimeout, &loop, [this, imageId]() {
             emit error("Timeout A on rendering " + imageId);
         });
#endif
        QObject::connect(this, &FigmaGet::error, &loop, [&loop, &err](const QString&) {
            err = true;
            loop.quit();
        });

        QObject::connect(this, &FigmaGet::imageRendered, &loop, [&loop, imageId](const QString& image) {
            if(imageId == image)
                loop.quit();
        });

        requestRendering(imageId);
        loop.exec(); NESTED_END
        while(loop.processEvents());


        if(err) {
            emit error(QString("Rendering not available \"%1\"").arg(imageId));
            return NoData;
        }
    }

    if(!m_renderings->contains(imageId)) {
        emit error(QString("Rendering not found \"%1\"").arg(imageId));
        return NoData;
    }

    if(m_renderings->isEmpty(imageId)) {
        QEventLoop loop; NESTED_START
    #ifdef IMAGE_TIMEOUT
         QTimer::singleShot(ImageTimeout, &loop, [this, imageId]() {
             emit error("Timeout B on rendering " + imageId);
         });
    #endif
        QObject::connect(this, &FigmaGet::error, &loop, [&loop](const QString&) {
            loop.quit();
        });

        QObject::connect(this, &FigmaGet::imageRetrieved, &loop, [this, &loop, imageId](const QString& image) {
            if(imageId == image) {
           //     m_renderings->commit(imageId);
                loop.quit();
                }
            });
          if( m_renderings->setPending(imageId)) {
                retrieveImage(imageId, m_renderings.get(),
                              QSize(std::numeric_limits<int>::max(),
                                    std::numeric_limits<int>::max()));
          }

        loop.exec(); NESTED_END
        while(loop.processEvents());

        if(m_renderings->isEmpty(imageId)) {
            emit error(QString("Rendering cannot be retrieved \"%1\"").arg(imageId));
            return NoData;
        }
    }
    return {m_renderings->data(imageId), m_renderings->format(imageId)};
}

QNetworkReply* FigmaGet::doRequestRendering() {
    FOO;
    if(m_rendringQueue.isEmpty())
        return nullptr;

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, false);

    const QStringList params{
        "ids=" + m_rendringQueue.join(','),
        "use_absolute_bounds=true"
    };
    m_rendringQueue.clear();

    request.setUrl(QUrl("https://api.figma.com/v1/images/" + m_projectToken + "?" + params.join('&')));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);
    std::shared_ptr<QByteArray> bytes(new QByteArray);


    QObject::connect(reply, &QNetworkReply::destroyed, this, [this](QObject*) {
           if(!m_rendringQueue.isEmpty()) {
               requestRendering(QString());
           }
    });

    const auto finished =  [reply, this, bytes]() {
        FOOBAR << reply;
        if(bytes->isEmpty()) {
           emit error("Error on rendering - no data");
           return;
        }
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(*bytes, &err);
        if(err.error != QJsonParseError::NoError) {
           emit error(QString("Error on rendering - JSON: %1 at %2")
                    .arg(err.errorString())
                    .arg(err.offset));
            qDebug() << "JSON - size:" << bytes->size() << "dump: " << *bytes;
            return;
        }
        const auto obj = doc.object();
        if(obj["error"].toBool()) {
            emit error(QString("Error on rendering %1").arg(obj["status"].toString()));
        } else {
            const auto renderings = obj["images"].toObject();
            for(const auto& key : renderings.keys()) {
                if(renderings[key].toString().isEmpty()) {
                    emit error(QString("Invalid URL key:\"%1\"").arg(key));
                    break;
                }
                m_renderings->insert(key, renderings[key].toString());
                emit imageRendered(key);
            }
        }
    };

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
    FOO;

    if(m_downloads->downloading()) {
        emit updateCompleted(false);
        return;
    }

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, false);

    const QStringList params{
        {"geometry=paths"}
    };
    request.setUrl(QUrl("https://api.figma.com/v1/files/" + m_projectToken + QChar('?') + params.join('&')));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    std::shared_ptr<QByteArray> bytes(new QByteArray);
    auto reply = m_accessManager->get(request);

    const auto finished =  [reply, this, bytes]() {
        FOOBAR << reply;
        reply->deleteLater();
        QObject::connect(reply, &QObject::destroyed, this, [this, bytes] (QObject*) { //since added after downloads, this is called after
            emit replyComplete(bytes);
        });
    };

    monitorReply(reply, bytes, finished, m_checksum == 0);

}

void FigmaGet::documentCreated() {
    FOO;
    m_connectionState = State::Complete;
}

QByteArray FigmaGet::getNode(const QString &id) {
    FOO;
    if(!m_nodes->contains(id)) {
        const QStringList params{
            "ids=" + id,
            "geometry=paths"
        };
        m_nodes->insert(id, "https://api.figma.com/v1/files/" + m_projectToken + "/nodes?" + params.join('&'));
    }

    if(!m_nodes->isEmpty(id)) {
        return m_nodes->data(id);
    }
    QEventLoop loop; NESTED_START
    QObject::connect(this, &FigmaGet::nodeRetrieved, [&id, &loop](const QString& nodeid) {
        if(id == nodeid)
            loop.quit();
    });

    if(m_nodes->setPending(id))
        retrieveNode(id);
    //m_nodes->onPending(id, [this](const QString& id, const QString& url,  QByteArray* target) {
    //    retrieveNode(QUrl(url), id, target);
    //});

    QObject::connect(this, &FigmaGet::error, &loop, [&loop](const QString&) {
        loop.quit();
    });

   loop.exec(); NESTED_END
   while(loop.processEvents());

   return m_nodes->isEmpty(id) ? QByteArray() : m_nodes->data(id);
}

void FigmaGet::doRetrieveNode(const QString& id) {
    FOO;

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::SynchronousRequestAttribute, false);

    const auto uri = m_nodes->url(id);
    request.setUrl(uri);
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    QEventLoop loop; NESTED_START
    const auto finished = [reply, &loop] () {
        FOOBAR << reply;
        loop.quit();
    };

    monitorReply(reply, bytes, finished);
#ifdef IMAGE_TIMEOUT
    QTimer::singleShot(ImageTimeout, &loop, [this, id]() {
        emit error("Timeout B on node " + id);
    });
#endif
    QObject::connect(this, &FigmaGet::error, &loop, [&loop, bytes](const QString&) {
        bytes->clear();
        loop.quit();
    });

    loop.exec(); NESTED_END
    while(loop.processEvents());

    if(m_connectionState == State::Loading)
        m_nodes->setBytes(id, *bytes);
    emit nodeRetrieved(id);
}


void FigmaGet::replyReader() {
    FOO;
    const auto replies = m_replies.keys();
    for(auto& reply : replies)
    {
        FOO << reply;
        if(reply->bytesAvailable()) {
            auto data = std::get<std::shared_ptr<QByteArray>>(m_replies[reply]);
            *data += reply->readAll();
            qDebug() << "F00" << reply << data->size();
        }
    }
}

void FigmaGet::onReplyError(QNetworkReply::NetworkError err)
{
    const auto keys = m_replies.keys();
    const auto it = std::find_if(keys.begin(), keys.end(), [err](const auto r) {
        return r->error() == err;
    });
    if(it == keys.end())
        return;
    const auto reply = *it;
    m_replies.remove(reply);
    if(err == QNetworkReply::UnknownContentError) { //Too Many Requests
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const auto code = statusCode.isValid() ? statusCode.toInt() : -1;
        if(code == 429) {
            emit m_downloads->tooManyRequests();
            const auto faildedCall = m_downloads->monitored(reply);
            QTimer::singleShot(ImageRetry, this, [this, faildedCall]() { //figma doc says about one minute
                queueCall(faildedCall);
            });
        }  else {
            emit error("HTTP error: " + reply->errorString());
        }
    } else {
        emit error("Network error: " + QString::number(err) + ", "  + reply->errorString());
    }
}

void FigmaGet::monitorReply(QNetworkReply* reply,
                            const std::shared_ptr<QByteArray>& bytes,
                            const std::function<void ()>& finalize,
                            bool showProgress) {
    FOO << reply;
    Q_ASSERT(!m_replies.contains(reply));
    m_replies.insert(reply, {bytes, finalize});
    Q_ASSERT(!reply->attribute(QNetworkRequest::SynchronousRequestAttribute).toBool());

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
    FOO;
    return m_downloads;
}


