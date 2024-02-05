#ifndef FIGMACANVAS_H
#define FIGMACANVAS_H

#include "figmadocument.h"
#include "figmaprovider.h"
#include "figmaparser.h"
#include <QObject>
#include <QVariantMap>
#include <QUrl>
#include <QVector>
#include <memory>
#include <optional>

class FigmaFileDocument;
class FigmaDataDocument;
class FontCache;
class FontInfo;


class FigmaQml : public QObject, public FigmaParserData {
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
    Q_PROPERTY(QVariantMap fonts READ fonts WRITE setFonts NOTIFY fontsChanged STORED false)
    Q_PROPERTY(QString fontFolder MEMBER m_fontFolder NOTIFY fontFolderChanged)
    Q_PROPERTY(QString documentsLocation READ documentsLocation CONSTANT)
    Q_PROPERTY(QVariantList elements READ elements NOTIFY elementsChanged)
public:
    enum Flags { // WARNING these map values are (partly) same with figmaparser flags
        PrerenderShapes     = 0x2,
        PrerenderGroups     = 0x4,
        PrerenderComponets  = 0x8,
        PrerenderFrames     = 0x10,
        PrerenderInstances  = 0x20,
        NoGradients         = 0x40,
        BreakBooleans       = 0x400,
        AntializeShapes     = 0x800,
        QulMode             = 0x1000,
        StaticCode          = 0x2000,
        EmbedImages         = 0x10000,
        Timed               = 0x20000,
        AltFontMatch        = 0x40000,
        KeepFigmaFontName   = 0x80000,
        LoaderPlaceHolders  = 0x100000,
    };
    Q_ENUM(Flags)
public:
     void parseError(const QString&, bool isFatal) override;
     QByteArray imageData(const QString&, bool isRendering) override;
     QByteArray nodeData(const QString&) override;
     QString fontInfo(const QString&) override;
     unsigned unique_number() override;
public:
    FigmaQml(const QString& qmlDir, const QString& fontFolder, FigmaProvider& provider, QObject* parent = nullptr);
    ~FigmaQml();
    QByteArray sourceCode() const;
    QByteArray sourceCode(unsigned canvasIndex, unsigned elementIndex) const;
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
    QStringList components(int canvas_index, int element_index) const;
    QVariantMap fonts() const;
    void setFonts(const QVariantMap& map);
    bool setBrokenPlaceholder(const QString& placeholder);
    bool isValid() const;
    void setFilter(const QMap<int, QSet<int>>& filter);
    void restore(int flags, const QVariantMap& imports);
    QString documentsLocation() const;
    QVariantList elements() const;
    const auto& externalLoaders() const {return m_externalLoaders;}
    Q_INVOKABLE bool saveAllQML(const QString& folderName);
    Q_INVOKABLE bool saveCurrentQML(const QVariantMap& parameters, const QString& folderName, bool writeAsApp, const std::vector<int>& elements);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE static QString validFileName(const QString& name);
    Q_INVOKABLE QByteArray componentSourceCode(const QString& name) const;
    Q_INVOKABLE QString componentObject(const QString& name) const;
    Q_INVOKABLE QVariantMap defaultImports() const;
    Q_INVOKABLE QByteArray prettyData(const QByteArray& data) const;
    Q_INVOKABLE void setFontMapping(const QString& key, const QString& value);
    Q_INVOKABLE void resetFontMappings();
    Q_INVOKABLE void setSignals(bool allow);
    //void takeSnap(const QString& pngName) const;
    Q_INVOKABLE static QString nearestFontFamily(const QString& requestedFont, bool useQt);
    Q_INVOKABLE void executeQul(const QVariantMap& parameters, const std::vector<int>& elements);
    Q_INVOKABLE bool hasFontPathInfo() const;
    Q_INVOKABLE void findFontPath(const QString& fontFamilyName) const;
#ifdef USE_NATIVE_FONT_DIALOG
    // sigh font native dialog wont work on WASM and QML dialog is buggy
    Q_INVOKABLE void showFontDialog(const QString& currentFont);
#endif
#ifdef WASM_FILEDIALOGS
    Q_INVOKABLE bool saveAllQMLZipped(const QString& docName, const QString& canvasName);
    Q_INVOKABLE bool importFontFolder();
    Q_INVOKABLE bool store(const QString& docName, const QString& tempName);
    Q_INVOKABLE void restore();
#endif
    std::optional<QStringList> saveImages(const QString &folder, const QSet<QString>& filter = {}) const;
    bool writeQmlFile(const QString& component_name, const QByteArray& element_data, const QByteArray& header, const QString& subFolder = {});
    QByteArray makeHeader() const;
    bool testFileExists(const QString& filename, const QByteArray& data) const;
    Q_INVOKABLE void reset(bool keepFonts, bool keepSources, bool keepImages, bool keepFetch);
public slots:
    void createDocumentView(const QByteArray& data, bool restoreView);
    void createDocumentSources(const QByteArray& data);
signals:
    void figmaDocumentCreated(FigmaFileDocument* doc);
    void figmaDocumentCreated(FigmaDataDocument* doc);
    void documentCreated();
    void sourceCodeChanged();
    void elementChanged();
    void flagsChanged();
    void error(const QString& errorString) const;
    void warning(const QString& warningString) const;
    void info(const QString& infoString) const;
    void qulInfo(const QString& qulInfo, int level);
    void qulInfoStop();
    void canvasCountChanged();
    void elementCountChanged();
    void currentElementChanged();
    void currentCanvasChanged();
    void canvasNameChanged();
    void elementNameChanged();
    void imageDimensionMaxChanged();
    void documentNameChanged();
    void busyChanged();
    void isValidChanged();
    void cancelled();
    void componentsChanged();
    void importsChanged();
    void componentLoaded(int canvas, int element);
    void snapped();
    void takeSnap(const QString& pngName, int canvasToWait, int elementToWait);
    void fontsChanged();
    void fontFolderChanged();
    void fontLoaded(const QFont& font);
    void fontPathFound(const QString& fontPath);
    void fontPathError(const QString& error);
    void elementsChanged();
#ifdef USE_NATIVE_FONT_DIALOG
    void fontAdded(const QString& fontFamilyName);
#endif
    void refresh();
#ifdef WASM_FILEDIALOGS
    void wasmRestored(const QString& name, const QString& file_name);
#endif
private slots:
    void doCancel();
    void updateDefaultImports();
private:
    void addImageFile(const QString& imageRef, bool isRendering);
    bool addImageFileData(const QString& imageRef, const QByteArray& bytes, int mime);
    bool ensureDirExists(const QString& dirname) const;
    bool doCreateDocument(FigmaDocument& doc, const QJsonObject& json);
    template<class FigmaDocType>
    void createDocument(const QJsonObject& json);
    std::optional<QJsonObject> object(const QByteArray& bytes);
    void cleanDir(const QString& dirName);
    std::optional<std::tuple<QByteArray, int>> getImage(const QString& imageRef, bool isRendering);
    void suspend();
    bool writeComponents(FigmaDocument& doc, const FigmaParser::Components& components, const QByteArray& header);
    bool setDocument(FigmaDocument& doc, const FigmaParser::Canvases& canvases, const FigmaParser::Components& components, const QByteArray& header);
    QString qmlTargetDir() const override;
    std::optional<QString> uniqueFilename(const QString& filename, const QByteArray& data);
private:
    const QString m_qmlDir;
    FigmaProvider& mProvider;
    std::unique_ptr<FigmaFileDocument> m_uiDoc;
    std::unique_ptr<FigmaDataDocument> m_sourceDoc;
    QVariantMap m_imports;
    int m_imageDimensionMax = 1024;
    bool m_busy = false;
    unsigned m_flags = 0;
    QByteArray m_brokenPlaceholder;
    QMap<int, QSet<int>> m_filter;
    QHash<QString, QPair<QString, QString>> m_imageFiles;
    QString m_snap;
    std::unique_ptr<FontCache> m_fontCache;
    QString m_fontFolder;
    std::atomic_bool m_doCancel = false;    
    std::atomic_bool m_ok = true;
    bool m_embedImages = false;
    enum class State {Constructing, Failed, Suspend};
    State m_state = State::Constructing;
    std::function<void (bool)> mRestore = nullptr;
    QHash<QString, QSet<QString>> m_imageContexts;
    FontInfo* m_fontInfo;
    FigmaParser::ExternalLoaders m_externalLoaders;
    unsigned m_unique_number = 1;
    QHash<QString, quint16> m_crcs;
};


#endif // FIGMACANVAS_H
