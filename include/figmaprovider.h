#ifndef FIGMAPROVIDER_H
#define FIGMAPROVIDER_H

#include <QObject>
#include <QSize>
#include <limits>

class FigmaProvider : public QObject {
    Q_OBJECT;
public:
    FigmaProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual bool isReady() = 0;
    virtual std::optional<std::tuple<QByteArray, int>> cachedImage(const QString& imageRef) = 0;
    virtual std::optional<std::tuple<QByteArray, int>> cachedRendering(const QString& figmaId) = 0;
    virtual std::optional<QByteArray> cachedNode(const QString& figmaId) = 0;
    virtual void getImage(const QString& imageRef,
                                        const QSize& maxSize = QSize(std::numeric_limits<int>::max(),
                                                                     std::numeric_limits<int>::max())) = 0;
    virtual void getRendering(const QString& figmaId) = 0;
    virtual void getNode(const QString& figmaId) = 0;
    virtual std::tuple<int, int, int> cacheInfo() const = 0;
signals:
    void imageReady(const QString& imageRef, const QByteArray& bytes, int format);
    void renderingReady(const QString& figmaId, const QByteArray& bytes, int format);
    void nodeReady(const QString& figmaId);
};


class FigmaParserData {
public:
    virtual void parseError(const QString&, bool isFatal) = 0;
    virtual QByteArray imageData(const QString&, bool isRendering) = 0;
    virtual QByteArray nodeData(const QString&) = 0;
    virtual QString fontInfo(const QString&) = 0;
};


#endif // FIGMAPROVIDER_H
