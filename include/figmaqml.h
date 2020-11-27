#ifndef FIGMACANVAS_H
#define FIGMACANVAS_H

#include <QObject>
#include <QVariantMap>
#include <QUrl>
#include <QVector>
#include <memory>
#include <optional>

class FigmaFileDocument;
class FigmaDataDocument;
class FontCache;

class FigmaQml : public QObject {
    Q_OBJECT
    Q_PROPERTY(QByteArray sourceCode READ sourceCode NOTIFY sourceCodeChanged)
    Q_PROPERTY(QUrl element READ element NOTIFY elementChanged)
    Q_PROPERTY(QVariantMap imports MEMBER m_imports NOTIFY importsChanged)
    Q_PROPERTY(unsigned flags MEMBER m_flags NOTIFY flagsChanged)
    Q_PROPERTY(int canvasCount READ canvasCount NOTIFY canvasCountChanged)
    Q_PROPERTY(int elementCount READ elementCount NOTIFY elementCountChanged)
    Q_PROPERTY(int currentElement READ currentElement WRITE setCurrentElement NOTIFY currentElementChanged)
    Q_PROPERTY(int currentCanvas READ currentCanvas WRITE setCurrentCanvas NOTIFY currentCanvasChanged)
    Q_PROPERTY(QString canvasName READ canvasName NOTIFY canvasNameChanged)
    Q_PROPERTY(QString elementName READ elementName NOTIFY elementNameChanged)
    Q_PROPERTY(QString documentName READ documentName NOTIFY documentNameChanged)
    Q_PROPERTY(int imageDimensionMax MEMBER m_imageDimensionMax NOTIFY imageDimensionMaxChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
    Q_PROPERTY(QString qmlDir READ qmlDir CONSTANT)
    Q_PROPERTY(QStringList components READ components NOTIFY componentsChanged)
    Q_PROPERTY(QVariantMap fonts READ fonts WRITE setFonts NOTIFY fontsChanged)
    Q_PROPERTY(QString fontFolder MEMBER m_fontFolder NOTIFY fontFolderChanged)
public:
    enum Flags {
        PrerenderShapes     = 0x2,
        PrerenderGroups     = 0x4,
        PrerenderComponets  = 0x8,
        PrerenderFrames     = 0x10,
        BreakBooleans       = 0x20,
        AntializeShapes     = 0x40,
        EmbedImages         = 0x80,
        Timed               = 0x100,
        QtFontMatch         = 0x200
    };
    Q_ENUM(Flags)
public:
    using ImageProvider = std::function<std::pair<QByteArray, int> (const QString&, bool isRendering, const QSize&)>;
    using NodeProvider = std::function<QByteArray (const QString&)>;
    FigmaQml(const QString& qmlDir, const QString& fontFolder, const ImageProvider& byteProvider, const NodeProvider& nodeProvider, QObject* parent = nullptr);
    ~FigmaQml();
    QByteArray sourceCode() const;
    QUrl element() const;
    int canvasCount() const;
    int elementCount() const;
    int currentElement() const;
    int currentCanvas() const;
    bool setCurrentElement(int current);
    bool setCurrentCanvas(int current);
    bool busy() const;
    QString canvasName() const;
    QString elementName() const;
    QString documentName() const;
    QString qmlDir() const;
    QStringList components() const;
    QVariantMap fonts() const;
    void setFonts(const QVariantMap& map);
    bool setBrokenPlaceholder(const QString& placeholder);
    bool isValid() const;
    void setFilter(const QMap<int, QSet<int>>& filter);
    void restore(int flags, const QVariantMap& imports);
    Q_INVOKABLE bool saveAllQML(const QString& folderName) const;
    Q_INVOKABLE void cancel();
    Q_INVOKABLE static QString validFileName(const QString& name);
    Q_INVOKABLE QByteArray componentSourceCode(const QString& name) const;
    Q_INVOKABLE QString componentData(const QString& name) const;
    Q_INVOKABLE static QVariantMap defaultImports();
    Q_INVOKABLE QByteArray prettyData(const QByteArray& data) const;
    Q_INVOKABLE void setFontMapping(const QString& key, const QString& value);
    Q_INVOKABLE void resetFontMappings();
    Q_INVOKABLE void setSignals(bool allow);
    void takeSnap(const QString& pngName) const;
    Q_INVOKABLE static QString nearestFontFamily(const QString& requestedFont, bool useQt);
public slots:
    void createDocumentView(const QByteArray& data, bool restoreView);
    void createDocumentSources(const QByteArray& data);
signals:
    void documentCreated();
    void sourceCodeChanged();
    void elementChanged();
    void flagsChanged();
    void error(const QString& errorString) const;
    void warning(const QString& warningString) const;
    void info(const QString& infoString) const;
    void canvasCountChanged();
    void elementCountChanged();
    void currentElementChanged();
    void currentCanvasChanged();
    void canvasNameChanged();
    void elementNameChanged();
    void imageDimensionMaxChanged();
    void documentNameChanged();
    void busyChanged() const;
    void isValidChanged();
    void cancelled();
    void componentsChanged();
    void importsChanged();
    void componentLoaded(int canvas, int element);
    void snapped();
    void takeSnap(const QString& pngName, int canvasToWait, int elementToWait);
    void fontsChanged();
    void fontFolderChanged();
    void refresh();
private:
    bool addImageFile(const QString& imageRef, bool isRendering, const QString& targetDir);
    bool ensureDirExists(const QString& dirname) const;
    bool saveImages(const QString &folder) const;
    template<class T> std::unique_ptr<T> construct(const QJsonObject& data, const QString& targetDir, bool embedImages) const;
    std::optional<QJsonObject> object(const QByteArray& bytes) const;
    void cleanDir(const QString& dirName) const;
private:
    const QString m_qmlDir;
    const ImageProvider mImageProvider;
    const NodeProvider mNodeProvider;
    std::unique_ptr<FigmaFileDocument> m_uiDoc;
    std::unique_ptr<FigmaDataDocument> m_sourceDoc;
    QVariantMap m_imports;
    int m_imageDimensionMax = 1024;
    mutable bool m_busy = false;
    unsigned m_flags = 0;
    QByteArray m_brokenPlaceholder;
    QMap<int, QSet<int>> m_filter;
    QHash<QString, QPair<QString, QString>> m_imageFiles;
    QString m_snap;
    std::unique_ptr<FontCache> m_fontCache;
    QString m_fontFolder;
};


#endif // FIGMACANVAS_H
