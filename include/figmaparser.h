#ifndef FIGMAPARSER_H
#define FIGMAPARSER_H

#include "figmaprovider.h"
#include "orderedmap.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>
#include <QTextStream>
#include <QSet>
#include <QStack>
#include <QFont>
#include <QColor>
#include <optional>

constexpr auto FIGMA_SUFFIX{"_figma"};

/**
 * This class cries TODO!
 *
 * I knew at 1st write that writing parser like this is going to grow problems.
 * That is rather a generator Figmal --> QML source. That makes it very tricky to
 * have a state during the the parsing that would be beneficial later on.
 *
 * This should be rewritten to to tree (or other syntax independent container)
 * that can do quiries while parsing and then separate phase for code generation.
 *
 * Maitaining this is terrible as it requires terrible hacks and tricks whenever
 * changes are needed, state information accessed or maintainded :-/
 *
 */


class FigmaParser {
private:
    // this is set of contexts where a image is used
    using ImageContexts =  QSet<QString>;
    // Components as filename -> object + qml code
    using ComponentStreams = QHash<QByteArray, std::tuple<QJsonObject, QByteArray/*, QString*/>>;
public:
    // Loader (for asLoader replacement) id --> obj + name
    using ExternalLoaders = QHash<QString, std::tuple<QByteArray, QString>>;
public:
    /**
     * @brief The Canvas class is a Figma Page, represents all elements in there
     */
    class Canvas {
    public:
        using ElementVector = std::vector<QJsonObject>;
        QString color() const {return m_color;}
        const QString& id() const {return m_id;}
        const QString& name() const {return m_name;}
        const ElementVector& elements() const {return m_elements;}
        Canvas(const QString& name, const QString& id, const QByteArray color, ElementVector&& frames) :
            m_name(name), m_id(id), m_color(color), m_elements(frames) {}
    private:
        const QString m_name;
        const QString m_id;
        const QByteArray m_color;
        const ElementVector m_elements;
    };
    /**
     * @brief The Element class, an element on page - converted to QML file
     */
    class Element  {
    public:
        Element(const QString& name,
                const QString& id,
                const QString& type,
                QByteArray&& data,
                QStringList&& componentIds,
                QStringList&& contexts,
                QVector<QString>&& aliases,
                const ComponentStreams& componentStreams,
                const ExternalLoaders& externalLoaders) :
            m_name(name), m_id(id), m_type(type), m_data(data), m_componentIds(componentIds),
            m_imageContexts{contexts}, m_aliases(aliases), m_componentStreams(componentStreams), m_externalLoaders(externalLoaders) {}
        Element() {}
        Element(const Element& other) = default;
        Element& operator=(const Element& other) = delete;
        const QString& id() const {return m_id;}
        const QString& name() const {return m_name;}
        const QByteArray& data() const {return m_data;}
        const QStringList& components() const {return m_componentIds;}
        const QStringList& imageContexts() const {return m_imageContexts;}
        const QVector<QString>& aliases() const {return m_aliases;}
        const ComponentStreams& subComponents() const {return m_componentStreams;}
        const ExternalLoaders& externalLoaders() const {return m_externalLoaders;}
    private:
        const QString m_name;
        const QString m_id;
        const QString m_type;
        const QByteArray m_data;
        const QStringList m_componentIds;
        const QStringList m_imageContexts;
        const QVector<QString> m_aliases;
        const ComponentStreams m_componentStreams;
        const ExternalLoaders m_externalLoaders;
    };
    /**
     * @brief The Component class, Figma component, an element can compose and/or override parts with components.
     * Converted to QML file, but not exact match to QML components.
     */
    class Component {
    public:
        Component(const QString& name,
                           const QString& id,
                           const QString& key,
                           const QString& description,
                           QJsonObject&& object) :
            m_name(name), m_id(id), m_key(key),
            m_description(description), m_object(object) {}
        QString name() const {
            Q_ASSERT(m_name.endsWith(FIGMA_SUFFIX) || validFileName(m_name, false) == m_name);
            return m_name;
        }
        const QString& description() const {return m_description;}
        const QString& id() const {return m_id;}
        const QString& key() const {return m_key;}
        const QJsonObject& object() const {return m_object;}
    private:
        const QString m_name;
        const QString m_id;
        const QString m_key;
        const QString m_description;
        const QJsonObject m_object;
    };
    using Components = QHash<QString, std::shared_ptr<Component>>;
    using Canvases = std::vector<Canvas>;

public:
    inline static const QString PlaceHolder = "placeholder";
    enum Flags {        // WARNING these map values are same with figmaqml flags
        PrerenderShapes     = 0x2,
        PrerenderGroups     = 0x4,
        PrerenderComponents = 0x8,
        PrerenderFrames     = 0x10,
        PrerenderInstances  = 0x20,
        NoGradients         = 0x40,
        ParseComponent  = 0x200,
        BreakBooleans   = 0x400,
        AntializeShapes = 0x800,
        QulMode         = 0x1000,
        StaticCode      = 0x2000,
        LoaderPlaceHolders  = 0x100000,

    };
    using EByteArray = std::optional<QByteArray>;
public:
    static std::optional<Components> components(const QJsonObject& project,  FigmaParserData& data);
    static std::optional<Canvases> canvases(const QJsonObject& project, FigmaParserData& data);
    static std::optional<Element> component(const QJsonObject& obj, unsigned flags,  FigmaParserData& data, const Components& components);
    static std::optional<Element> element(const QJsonObject& obj, unsigned flags,  FigmaParserData& data, const Components& components);
    static QString name(const QJsonObject& project);
    static QString lastError();
    static QString makeFileName(const QString& itemName);
private:
    enum class StrokeType {Normal, Double, OnePix};
    enum class ItemType {None, Vector, Text, Frame, Component, Boolean, Instance};
private:
    static QString validFileName(const QString& itemName, bool inited);
    static QHash<QString, QJsonObject> getObjectsByType(const QJsonObject& obj, const QString& type);
    static QJsonObject delta(const QJsonObject& instance, const QJsonObject& base,
                             const QSet<QString>& ignored,
                             const QHash<QString, std::function<QJsonValue (const QJsonValue&, const QJsonValue&)>>& compares);
    static QHash<QString, QString> children(const QJsonObject& obj);
    std::optional<Element> getElement(const QJsonObject& obj);
    QString tabs(int indents) const;
#if 0
    QRectF boundingRect(const QJsonObject& obj);
    QRectF boundingRect(const QString& svgPath, const QSizeF& size) const;
#endif
    static QByteArray toColor(double r, double g, double b, double a = 1.0);
    QByteArray makeId(const QJsonObject& obj);
    QByteArray makeId(const QString& prefix,  const QJsonObject& obj);
    EByteArray makeComponentInstance(const QString& type, const QJsonObject& obj, int indents);
    EByteArray makeItem(const QString& type, const QJsonObject& obj, int indents);

    QPointF position(const QJsonObject& obj) const;

    QByteArray makeExtents(const QJsonObject& obj, int indents, const QRectF& extents = QRectF{0, 0, 0, 0});
    QByteArray makeSize(const QJsonObject& obj, int indents, const QSizeF& extents = QSizeF{0, 0});
    QByteArray makeColor(const QJsonObject& obj, int indents, double opacity = 1.);
    QByteArray makeEffects(const QJsonObject& obj, int indents);
    QByteArray makeTransforms(const QJsonObject& obj, int indents);
    EByteArray makeImageSource(const QString& image, bool isRendering, int indents, const QString& placeHolder = QString());
    EByteArray makeImageRef(const QString& image, int indents);
    EByteArray makeFill(const QJsonObject& obj, int indents);
    EByteArray makeVector(const QJsonObject& obj, int indents);
    QByteArray makeStrokeJoin(const QJsonObject& stroke, int indent);
    QByteArray makeShapeStroke(const QJsonObject& obj, int indents, StrokeType type = StrokeType::Normal);
    QByteArray makeShapeFill(const QJsonObject& obj, int indents);
    EByteArray makePlainItem(const QJsonObject& obj, int indents);
    QByteArray makeSvgPath(int index, bool isFill, const QJsonObject& obj, int indents);

    EByteArray parse(const QJsonObject& obj, int indents);

    bool isGradient(const QJsonObject& obj) const;

    std::optional<QString> imageFill(const QJsonObject& obj) const;

     EByteArray makeImageMaskData(const QString& imageRef, const QJsonObject& obj, int indents);
     QByteArray makeShapeFillData(const QJsonObject& obj, int shapeindents);
     QByteArray makeAntialising(int indents) const;

     /*
      * makeVectorxxxxxFill functions are redundant in purpose - but I ended up
      * to if-else hell and wrote open to keep normal/inside/outside and image/fill
      * cases managed
    */
     EByteArray makeVectorNormalFill(const QJsonObject& obj, int indents);
     EByteArray makeVectorNormalFill(const QString& image, const QJsonObject& obj, int indents);
     EByteArray makeVectorNormal(const QJsonObject& obj, int indents);
     EByteArray makeVectorInsideFill(const QJsonObject& obj, int indents);
     EByteArray makeVectorInsideFill(const QString& image, const QJsonObject& obj, int indents);
     EByteArray makeVectorInside(const QJsonObject& obj, int indentsBase);
     EByteArray makeVectorOutsideFill(const QJsonObject& obj, int indents);
     EByteArray makeVectorOutsideFill(const QString& image, const QJsonObject& obj, int indents);
     EByteArray makeVectorOutside(const QJsonObject& obj, int indentsBase);

     EByteArray parseVector(const QJsonObject& obj, int indents);


    QJsonObject toQMLTextStyles(const QJsonObject& obj) const;

    EByteArray parseStyle(const QJsonObject& obj, int indents);

     bool isRendering(const QJsonObject& obj) const;

    EByteArray parseText(const QJsonObject& obj, int indents);

     QByteArray parseSkip(const QJsonObject& obj, int indents);

     EByteArray parseFrame(const QJsonObject& obj, int indents);

     QString delegateName(const QString& id);


     EByteArray parseComponent(const QJsonObject& obj, int indents);

     EByteArray parseBooleanOperation(const QJsonObject& obj, int indents);


     QSizeF getSize(const QJsonObject& obj) const;

     enum class Content {Rendered, Loader};
     EByteArray parseContainer(const QJsonObject& obj, Content content, int indents);

     EByteArray makeInstanceChildren(const QJsonObject& obj, const QJsonObject& comp, int indents);
     QJsonValue getValue(const QJsonObject& obj, const QString& key) const;

     EByteArray parseInstance(const QJsonObject& obj, int indents);
     EByteArray parseChildren(const QJsonObject& obj, int indents);

     std::optional<OrderedMap<QString, QByteArray>> parseChildrenItems(const QJsonObject& obj, int indents);

     EByteArray parseBooleanOperationUnion(const QJsonObject& obj, int indents, const QString& sourceId, const QString& maskSourceId);
     EByteArray parseBooleanOperationSubtract(const QJsonObject& obj, const QJsonArray& children, int indents, const QString& sourceId, const QString& maskSourceId);
     EByteArray parseBooleanOperationIntersect(const QJsonObject& obj, const QJsonArray& children, int indents, const QString& sourceId, const QString& maskSourceId);
     EByteArray parseBooleanOperationExclude(const QJsonObject& obj, const QJsonArray& children, int indents, const QString& sourceId, const QString& maskSourceId);

     QByteArray parseQtComponent(const OrderedMap<QString, QByteArray>& children, int indents);
     QByteArray parseQulComponent(const OrderedMap<QString, QByteArray>& children, int indents);
     EByteArray makeChildMask(const QJsonObject& child, int indents);
     EByteArray makeImageMaskDataQul(const QString& imageRef, const QJsonObject& obj, int indents);
     EByteArray makeImageMaskDataQt(const QString& imageRef, const QJsonObject& obj, int indents);
     EByteArray makeGradientFill(const QJsonObject& obj, int indents);
     EByteArray makeRendered(const QJsonObject& obj, int indents);
     EByteArray makeLoader(const QJsonObject& obj, int indents);
     QByteArray makeGradientToFlat(const QJsonObject& obj, int indents);
     QByteArray addComponentStream(const QJsonObject& obj,  const QByteArray& child_item);
     QByteArray makePropertyChangeHandler(const QJsonObject& obj, int indents);
     EByteArray makeComponentPropertyChangeHandler(const QJsonObject& obj, int indents);
     QString makeFileName(const QJsonObject& obj, const QString& prefix) const;
     ~FigmaParser();
private:
     struct Parent{
         const QJsonObject* obj;
         const Parent* parent;
         QSet<QString> ids;
         template<typename T>
         auto operator[](const T& k) const {return (*obj)[k];}
         void push(const QJsonObject* obj_) {
             parent = new Parent{obj, parent, {}};
             obj = obj_;
         }
         void pop() {
             Q_ASSERT(parent);
             obj = parent->obj;
             auto p = parent;
             parent = p->parent;
             ids += p->ids;
             delete p;
         }
     };
     struct Alias {
         QString id;
         QJsonObject obj;
     };
 private:
    FigmaParser(unsigned flags, FigmaParserData& data, const Components* components);
    bool isQul() const {return m_flags & QulMode;}
    bool generateAccess() const {return (m_flags & StaticCode) == 0;}
private:
    const unsigned m_flags;
    FigmaParserData& m_data;
    const Components* m_components;
    const QString m_indent = "    ";
    QSet<QString> m_componentIds;
    Parent m_parent;
    ImageContexts m_imageContext;
    QVector<Alias> m_aliases;
    int m_componentLevel = 0;
    ComponentStreams m_componentStreams;
    static QByteArray fontWeight(double v);
    static std::optional<FigmaParser::ItemType> type(const QJsonObject& obj);
    int m_counter = 0;
    ExternalLoaders m_externalLoaders;
};

#endif // FIGMAPARSER_H
