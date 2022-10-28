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
#include <stack>
#include <optional>
#include <cmath>

const QString FIGMA_SUFFIX("_figma");


class FigmaParser {
public:
    class Canvas {
    public:
        using ElementVector = std::vector<QJsonObject>;
        QString color() const {return m_color;}
        QString id() const {return m_id;}
        QString name() const {return m_name;}
        const ElementVector& elements() const {return m_elements;}
        Canvas(const QString& name, const QString& id, const QByteArray color, ElementVector&& frames) :
            m_name(name), m_id(id), m_color(color), m_elements(frames) {}
    private:
        const QString m_name;
        const QString m_id;
        const QByteArray m_color;
        const ElementVector m_elements;
    };
    class Element  {
    public:
        explicit Element(const QString& name, const QString& id, const QString& type, QByteArray&& data,  QStringList&& componentIds) :
            m_name(name), m_id(id), m_type(type), m_data(data), m_componentIds(componentIds) {}
        Element() {}
        Element(const Element& other) = default;
        Element& operator=(const Element& other) = default;
        QString id() const {return m_id;}
        QString name() const {return m_name;}
        QByteArray data() const {return m_data;}
        QStringList components() const {return m_componentIds;}
    private:
        QString m_name;
        QString m_id;
        QString m_type;
        QByteArray m_data;
        QStringList m_componentIds;
    };
    class Component {
    public:
        explicit Component(const QString& name,
                           const QString& id,
                           const QString& key,
                           const QString& description,
                           QJsonObject&& object) :
            m_name(name), m_id(id), m_key(key),
            m_description(description), m_object(object) {}
        QString name() const {
            Q_ASSERT(m_name.endsWith(FIGMA_SUFFIX) || validFileName(m_name) == m_name);
            return m_name;
        }
        QString description() const {return m_description;}
        QString id() const {return m_id;}
        QString key() const {return m_key;}
        QJsonObject object() const {return m_object;}
    private:
        const QString m_name;
        const QString m_id;
        const QString m_key;
        const QString m_description;
        const QJsonObject m_object;
    };
    using Components = QHash<QString, std::shared_ptr<Component>>;
    using Canvases = std::vector<Canvas>;
    class Exception : public std::exception {
    public:
        Exception(const QString& issue) : m_issue((QString("FigmaParser exception: %1").arg(issue)).toLatin1()) {}
        virtual const char* what() const throw() override {
            return m_issue.constData();
          }
    private:
        const QByteArray m_issue;
    };
public:
    inline static const QString PlaceHolder = "placeholder";
    enum Flags {
        PrerenderShapes = 2,
        PrerenderGroups = 4,
        PrerenderComponents = 8,
        PrerenderFrames = 16,
        PrerenderInstances = 32,
        ParseComponent = 512,
        BreakBooleans = 1024,
        AntializeShapes = 2048
    };
public:
    static Components components(const QJsonObject& project,  FigmaParserData& data);
    static Canvases canvases(const QJsonObject& project, FigmaParserData& data);
    static Element component(const QJsonObject& obj, unsigned flags,  FigmaParserData& data, const Components& components);
    static Element element(const QJsonObject& obj, unsigned flags,  FigmaParserData& data, const Components& components);
    static QString name(const QJsonObject& project);
    static QString validFileName(const QString& itemName);

private:
    enum class StrokeType {Normal, Double, OnePix};
    enum class ItemType {None, Vector, Text, Frame, Component, Boolean, Instance};
    
private:

    static QHash<QString, QJsonObject> getObjectsByType(const QJsonObject& obj, const QString& type);
    static QJsonObject delta(const QJsonObject& instance, const QJsonObject& base,
                             const QSet<QString>& ignored,
                             const QHash<QString, std::function<QJsonValue (const QJsonValue&, const QJsonValue&)>>& compares);
    static QHash<QString, QString> children(const QJsonObject& obj);
    Element getElement(const QJsonObject& obj);
    QString tabs(int intendents) const;
#if 0
    QRectF boundingRect(const QJsonObject& obj);
    QRectF boundingRect(const QString& svgPath, const QSizeF& size) const;
#endif
    static QByteArray toColor(double r, double g, double b, double a = 1.0);
    static QByteArray qmlId(const QString& id);
    QByteArray makeComponentInstance(const QString& type, const QJsonObject& obj, int intendents);
    QByteArray makeItem(const QString& type, const QJsonObject& obj, int intendents);

    QPointF position(const QJsonObject& obj) const;

    QByteArray makeExtents(const QJsonObject& obj, int intendents, const QRectF& extents = QRectF{0, 0, 0, 0});
    QByteArray makeSize(const QJsonObject& obj, int intendents, const QSizeF& extents = QSizeF{0, 0});
    QByteArray makeColor(const QJsonObject& obj, int intendents, double opacity = 1.);
    QByteArray makeEffects(const QJsonObject& obj, int intendents);
    QByteArray makeTransforms(const QJsonObject& obj, int intendents);
    QByteArray makeImageSource(const QString& image, bool isRendering, int intendents, const QString& placeHolder = QString());
    QByteArray makeImageRef(const QString& image, int intendents);
    QByteArray makeFill(const QJsonObject& obj, int intendents);
    QByteArray makeVector(const QJsonObject& obj, int intendents);
    QByteArray makeStrokeJoin(const QJsonObject& stroke, int intendent);
    QByteArray makeShapeStroke(const QJsonObject& obj, int intendents, StrokeType type = StrokeType::Normal);
    QByteArray makeShapeFill(const QJsonObject& obj, int intendents);
    QByteArray makePlainItem(const QJsonObject& obj, int intendents);
    QByteArray makeSvgPath(int index, bool isFill, const QJsonObject& obj, int intendents);

    QByteArray parse(const QJsonObject& obj, int intendents);

    bool isGradient(const QJsonObject& obj) const;

    std::optional<QString> imageFill(const QJsonObject& obj) const;

     QByteArray makeImageMaskData(const QString& imageRef, const QJsonObject& obj, int intendents, const QString& sourceId, const QString& maskSourceId);
     QByteArray makeShapeFillData(const QJsonObject& obj, int shapeIntendents);
     QByteArray makeAntialising(int intendents) const;

     /*
      * makeVectorxxxxxFill functions are redundant in purpose - but I ended up
      * to if-else hell and wrote open to keep normal/inside/outside and image/fill
      * cases managed
    */
     QByteArray makeVectorNormalFill(const QJsonObject& obj, int intendents);
     QByteArray makeVectorNormalFill(const QString& image, const QJsonObject& obj, int intendents);
     QByteArray makeVectorNormal(const QJsonObject& obj, int intendents);
     QByteArray makeVectorInsideFill(const QJsonObject& obj, int intendents);
     QByteArray makeVectorInsideFill(const QString& image, const QJsonObject& obj, int intendents);
     QByteArray makeVectorInside(const QJsonObject& obj, int intendentsBase);
     QByteArray makeVectorOutsideFill(const QJsonObject& obj, int intendents);
     QByteArray makeVectorOutsideFill(const QString& image, const QJsonObject& obj, int intendents);
     QByteArray makeVectorOutside(const QJsonObject& obj, int intendentsBase);

    QByteArray parseVector(const QJsonObject& obj, int intendents);


    QJsonObject toQMLTextStyles(const QJsonObject& obj) const;

    QByteArray parseStyle(const QJsonObject& obj, int intendents);

     bool isRendering(const QJsonObject& obj) const;

    QByteArray parseText(const QJsonObject& obj, int intendents);

    QByteArray parseSkip(const QJsonObject& obj, int intendents);

     QByteArray parseFrame(const QJsonObject& obj, int intendents);

     QString delegateName(const QString& id);


     QByteArray parseComponent(const QJsonObject& obj, int intendents);

     QByteArray parseBooleanOperation(const QJsonObject& obj, int intendents);


     QSizeF getSize(const QJsonObject& obj) const;


     QByteArray parseRendered(const QJsonObject& obj, int intendents);

     QByteArray makeInstanceChildren(const QJsonObject& obj, const QJsonObject& comp, int intendents);
     QJsonValue getValue(const QJsonObject& obj, const QString& key) const;

     QByteArray parseInstance(const QJsonObject& obj, int intendents);
     QByteArray parseChildren(const QJsonObject& obj, int intendents);

     OrderedMap<QString, QByteArray> parseChildrenItems(const QJsonObject& obj, int intendents);

private:

    FigmaParser(unsigned flags, FigmaParserData& data, const Components* components);

    const unsigned m_flags;
    FigmaParserData& m_data;
    const Components* m_components;
    const QString m_intendent = "    ";
    QSet<QString> m_componentIds;
    const QJsonObject* m_parent;

    static QByteArray fontWeight(double v);
    static FigmaParser::ItemType type(const QJsonObject& obj);
};

#endif // FIGMAPARSER_H
