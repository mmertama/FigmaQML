#ifndef PROVIDERS_H
#define PROVIDERS_H

#include<QObject>

class FigmaGet;

//using ImageProvider = std::function<std::pair<QByteArray, int> (const QString&, bool isRendering, const QSize&)>;
//using NodeProvider = std::function<QByteArray (const QString&)>;

class ImageProvider : public QObject {
    Q_OBJECT
public:
    ImageProvider(FigmaGet& fg);
    void request(const QString& imageRef, const bool isRendering, const QSize& size, const QVariant& data);
signals:
    void available(const QByteArray& bytes, const int mime, const QString& imageRef, const QVariant& data);
};

class NodeProvider : public QObject {
   Q_OBJECT
public:
     NodeProvider(FigmaGet& fg);
};


#endif // PROVIDERS_H
