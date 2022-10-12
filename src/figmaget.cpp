#include "figmaget.h"
#include "figmadata.h"
#include "downloads.h"
#include <QQmlEngine>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <memory>
#include <array>

#define IMAGE_TIMEOUT

#ifdef IMAGE_TIMEOUT
constexpr auto ImageTimeout = 80 * 1000;
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

#define NoData {QByteArray(), 0}

FigmaGet::FigmaGet(const QString& dataDir, QObject *parent) : QObject(parent),
    m_accessManager(new QNetworkAccessManager(this)),
    m_downloads(new Downloads(this)),
    m_images(new FigmaData),
    m_renderings(new FigmaData),
    m_nodes(new FigmaData) {
     qmlRegisterUncreatableType<FigmaGet>("FigmaGet", 1, 0, "FigmaGet", "");

     QObject::connect(this, &FigmaGet::projectTokenChanged, this, &FigmaGet::reset);

     QObject::connect(m_downloads, &Downloads::cancelled, this, [this]() {
         m_checksum = 0;
     });
     QObject::connect(this, &FigmaGet::requestRendering, this, [this](const QString& id) {
         if(!id.isEmpty()) {
            m_rendringQueue.append(id);
         }
         quequeCall([this](){
             return FigmaGet::doRequestRendering();
         });
     });
     QObject::connect(this, &FigmaGet::retrieveImage, this, [this](const QString& id, FigmaData* target, const QSize& maxSize) {
         Q_ASSERT(maxSize.width() > 0 && maxSize.height() > 0);
         quequeCall([this, id, target, maxSize]() {
             return doRetrieveImage(id, target, maxSize);
         });
     });
     QObject::connect(this, &FigmaGet::populateImages, this, [this]() {
         quequeCall([this](){
             return doPopulateImages();
         });
     });
     QObject::connect(this, &FigmaGet::retrieveNode, this, [this](const QString& id) {
         quequeCall([this, id]() {
             doRetrieveNode(id);
             return nullptr;
         });
     });

     QObject::connect(&m_timer, &QTimer::timeout, this, [this]() {
         if(m_callQueue.isEmpty())
             m_timer.stop();
         else {
             const auto call = m_callQueue.dequeue();
             auto reply = call();
             m_downloads->monitor(reply, call);
         }
     });

     QObject::connect(this, &FigmaGet::error, [this](const QString&) {
         m_connectionState = State::Error;
         m_images->clean();
         m_renderings->clean();
         m_nodes->clean();
     });

     QObject::connect(m_downloads, &Downloads::cancelled, this, [this]() {
         emit error("Downloads cancel");
     }, Qt::QueuedConnection);
}


bool FigmaGet::store(const QString& filename, unsigned flags, const QVariantMap& imports) const {
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
    m_downloads->reset();
    m_images->clear();
    m_renderings->clear();
    m_nodes->clear();
}

void FigmaGet::cancel() {
    m_downloads->cancel();
}

void FigmaGet::quequeCall(const NetworkFunction& call) {
    m_callQueue.enqueue(call);
    if(!m_timer.isActive())
        m_timer.start(m_throttle);
}

QByteArray FigmaGet::data() const {
    return m_data;
}

std::pair<QByteArray, int> FigmaGet::getImage(const QString &imageRef, const QSize& maxSize) {
    Q_ASSERT(maxSize.width() > 0 && maxSize.height() > 0);
    Q_ASSERT(!imageRef.isEmpty());
    if(!m_images->contains(imageRef)) {
        bool err = false;
        QEventLoop loop;
#ifdef IMAGE_TIMEOUT
        QTimer::singleShot(ImageTimeout, &loop, [this, imageRef]() {
            emit error("Timeout A on image " + imageRef);
        });
#endif
        QObject::connect(this, &FigmaGet::error, &loop, [&loop, &err](const QString& errorStr) {
            qDebug() << errorStr;
            err = true;
            loop.quit();
        });

        QObject::connect(this, &FigmaGet::imagesPopulated, &loop, &QEventLoop::quit);
        if(!m_populationOngoing) { //just wait population
            emit populateImages();
        }
        loop.exec();
        if(err) {
            emit error(QString("Image is not available \"%1\"").arg(imageRef));
            return NoData;
        }
    }

    if(!m_images->contains(imageRef)) {
        emit error(QString("Image not found \"%1\"").arg(imageRef));
        return NoData;
    }

    if(m_images->isEmpty(imageRef)) {
        QEventLoop loop;
    #ifdef IMAGE_TIMEOUT
         QTimer::singleShot(ImageTimeout, &loop, [this, imageRef]() {
             emit error("Timeout B on image " + imageRef);
         });
    #endif
        QObject::connect(this, &FigmaGet::error, &loop, [&loop](const QString&) {
            loop.quit();
        });

        //We trust here that first
        QObject::connect(this, &FigmaGet::imageRetrieved, &loop, [this, &loop, imageRef](const QString& image) {
            if(imageRef == image) {
                //m_images->commit(imageRef);
                loop.quit();
            }
        });

        if(m_images->setPending(imageRef)) {
            emit retrieveImage(imageRef, m_images.get(), maxSize);
        }
        loop.exec();
        if(m_images->isEmpty(imageRef)) {
            emit error(QString("Image cannot be retrieved \"%1\"").arg(imageRef));
            return NoData;
        }
    }
    return {m_images->data(imageRef), m_images->format(imageRef)};
}



QNetworkReply* FigmaGet::doRetrieveImage(const QString& id, FigmaData *target, const QSize &maxSize) {
    QNetworkRequest request;
    const QUrl uri = target->url(id);
    if(!uri.isValid()) {
        emit error(QString("Url not valid \"%1\"").arg(uri.toString()));
        return nullptr;
    }

    request.setUrl(uri);
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());
    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    monitorReply(reply, bytes);

    QObject::connect(reply, &QNetworkReply::finished,
                     this, [reply, this, bytes, target, maxSize, id]() {

        QBuffer imageBuffer(bytes.get(), this);
        QImageReader imageReader(&imageBuffer);
        const auto format = imageReader.format();
        if(!(format == "png" || format == "jpeg" || format == "jpg")) {
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
        reply->deleteLater();
        emit imageRetrieved(id);
    });
    return reply;
}

QNetworkReply* FigmaGet::doPopulateImages() {

    m_populationOngoing = true;
    RAII_([this](){m_populationOngoing = false;});

    QNetworkRequest request;
    request.setUrl(QUrl("https://api.figma.com/v1/files/" + m_projectToken + "/images"));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    monitorReply(reply, bytes);

    QObject::connect(reply, &QNetworkReply::finished,
                     this, [reply, this, bytes]() {
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(*bytes, &err);
        if(err.error != QJsonParseError::NoError) {
           emit error(QString("Json error: %1 at %2")
                    .arg(err.errorString())
                    .arg(err.offset));
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
        reply->deleteLater();
        emit imagesPopulated();
    });
    return reply;
}

std::pair<QByteArray, int> FigmaGet::getRendering(const QString& imageId) {
    if(!m_renderings->contains(imageId)) {
        bool err = false;
        QEventLoop loop;
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

        emit requestRendering(imageId);
        loop.exec();

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
        QEventLoop loop;
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
                emit retrieveImage(imageId, m_renderings.get(),
                              QSize(std::numeric_limits<int>::max(),
                                    std::numeric_limits<int>::max()));
          }
        loop.exec();

        if(m_renderings->isEmpty(imageId)) {
            emit error(QString("Rendering cannot be retrieved \"%1\"").arg(imageId));
            return NoData;
        }
    }
    return {m_renderings->data(imageId), m_renderings->format(imageId)};
}

QNetworkReply* FigmaGet::doRequestRendering() {
    if(m_rendringQueue.isEmpty())
        return nullptr;

    QNetworkRequest request;
    const QStringList params{
        "ids=" + m_rendringQueue.join(','),
        "use_absolute_bounds=true"
    };
    m_rendringQueue.clear();

    request.setUrl(QUrl("https://api.figma.com/v1/images/" + m_projectToken + "?" + params.join('&')));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);
    std::shared_ptr<QByteArray> bytes(new QByteArray);

    monitorReply(reply, bytes);

    QObject::connect(reply, &QNetworkReply::destroyed, this, [this](QObject*) {
           if(!m_rendringQueue.isEmpty()) {
               emit requestRendering(QString());
           }
    });

    QObject::connect(reply, &QNetworkReply::finished,
                     this, [reply, this, bytes]() {
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(*bytes, &err);
        if(err.error != QJsonParseError::NoError) {
           emit error(QString("Json error: %1 at %2")
                    .arg(err.errorString())
                    .arg(err.offset));
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
        reply->deleteLater();
    });
    return reply;
}


void FigmaGet::update() {
    if(m_downloads->downloading()) {
        emit updateCompleted(false);
        return;
    }

    QNetworkRequest request;
    const QStringList params{
        {"geometry=paths"}
    };
    request.setUrl(QUrl("https://api.figma.com/v1/files/" + m_projectToken + QChar('?') + params.join('&')));
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    std::shared_ptr<QByteArray> bytes(new QByteArray);
    auto reply = m_accessManager->get(request);

    monitorReply(reply, bytes, m_checksum == 0);

    QObject::connect(reply, &QNetworkReply::finished,
                     this, [reply, this, bytes]() {
        reply->deleteLater();
        QObject::connect(reply, &QObject::destroyed, this, [this, bytes] (QObject* o) { //since added after downloads, this is called after
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
        });
    });
}

void FigmaGet::documentCreated() {
    m_connectionState = State::Complete;
}

QByteArray FigmaGet::getNode(const QString &id) {
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

    QEventLoop loop;
    QObject::connect(this, &FigmaGet::nodeRetrieved, [&id, &loop](const QString& nodeid) {
        if(id == nodeid)
            loop.quit();
    });

    if(m_nodes->setPending(id))
        emit retrieveNode(id);
    //m_nodes->onPending(id, [this](const QString& id, const QString& url,  QByteArray* target) {
    //    emit retrieveNode(QUrl(url), id, target);
    //});

    QObject::connect(this, &FigmaGet::error, &loop, [&loop](const QString&) {
        loop.quit();
    });

   loop.exec();

   return m_nodes->isEmpty(id) ? QByteArray() : m_nodes->data(id);
}

void FigmaGet::doRetrieveNode(const QString& id) {

    QNetworkRequest request;
    const auto uri = m_nodes->url(id);
    request.setUrl(uri);
    request.setRawHeader("X-Figma-Token", m_userToken.toLatin1());

    auto reply = m_accessManager->get(request);

    std::shared_ptr<QByteArray> bytes(new QByteArray);

    monitorReply(reply, bytes);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, [this, &loop, reply] () {
        reply->deleteLater();
        loop.quit();
    });
#ifdef IMAGE_TIMEOUT
    QTimer::singleShot(ImageTimeout, &loop, [this, id]() {
        emit error("Timeout B on node " + id);
    });
#endif
    QObject::connect(this, &FigmaGet::error, &loop, [&loop, bytes](const QString&) {
        bytes->clear();
        loop.quit();
    });

    loop.exec();
    if(m_connectionState == State::Loading)
        m_nodes->setBytes(id, *bytes);
    emit nodeRetrieved(id);
}

void FigmaGet::monitorReply(QNetworkReply* reply, const std::shared_ptr<QByteArray>& bytes, bool showProgress) {
    QObject::connect(reply, &QNetworkReply::errorOccurred,
                     this, [this, reply](QNetworkReply::NetworkError err) {
        if(err == QNetworkReply::UnknownContentError) { //Too Many Requests
            const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            const auto code = statusCode.isValid() ? statusCode.toInt() : -1;
            if(code == 429) {
                emit m_downloads->tooManyRequests();
                const auto faildedCall = m_downloads->monitored(reply);
                QTimer::singleShot(ImageRetry, [this, faildedCall]() { //figma doc says about one minute
                    quequeCall(faildedCall);
                });
            }  else {
                emit error("HTTP error: " + reply->errorString());
            }
        } else {
            emit error("Network error: " + QString::number(err) + ", "  + reply->errorString());
        }
        reply->deleteLater();
    });


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

    QObject::connect(reply, &QNetworkReply::readyRead,
                     this, [reply, this, bytes]() {
        *bytes += reply->readAll();
    });
}

Downloads* FigmaGet::downloadProgress() {
    return m_downloads;
}


