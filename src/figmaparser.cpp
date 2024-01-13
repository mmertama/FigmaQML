
#include "figmaparser.h"
#include "utils.h"
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
#include <cmath>

#include <QFile>
#include <QTimer>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

const auto QML_TAG = "qml?";

const auto ON_MOUSE_CLICK = ".onMouseClick";

static QString last_parse_error;

#define QUL_LOADER_WORKAROUND

using EByteArray = FigmaParser::EByteArray;

#define ERR(...) {last_parse_error = (toStr(__VA_ARGS__)); return std::nullopt;}

static inline bool eq(double a, double b) {return std::fabs(a - b) < std::numeric_limits<double>::epsilon();}

#define APPENDERR(val, fn) {const auto ob_ = fn; if(!ob_) return std::nullopt; val += ob_.value();}

QByteArray FigmaParser::fontWeight(double v) {
   const auto scaled = ((v - 100) / 900) * 90; // figma scale is 100-900, where Qt is enums
   const std::vector<std::pair<QByteArray, double>> weights { //from Qt docs
       {"Font.Thin", 0},
       {"Font.ExtraLight", 12},
       {"Font.Light", 25},
       {"Font.Normal", 50},
       {"Font.Medium", 57},
       {"Font.DemiBold", 63},
       {"Font.Bold", 75},
       {"Font.ExtraBold", 81},
       {"Font.Black", 87}
   };
   for(const auto& [n, w] : weights) {
       if(scaled <= w) {
           return n;
       }
   }
   return weights.back().first;
}

std::optional<FigmaParser::ItemType> FigmaParser::type(const QJsonObject& obj) {
   const QHash<QString, ItemType> types { //this to make sure we have a case for all types
       {"RECTANGLE", ItemType::Vector},
       {"TEXT", ItemType::Text},
       {"COMPONENT", ItemType::Component},
       {"BOOLEAN_OPERATION", ItemType::Boolean},
       {"INSTANCE", ItemType::Instance},
       {"ELLIPSE", ItemType::Vector},
       {"VECTOR", ItemType::Vector},
       {"LINE", ItemType::Vector},
       {"REGULAR_POLYGON", ItemType::Vector},
       {"STAR", ItemType::Vector},
       {"GROUP", ItemType::Frame},
       {"FRAME", ItemType::Frame},
       {"SLICE", ItemType::None},
       {"NONE", ItemType::None},
       {"COMPONENT_SET", ItemType::Frame}

   };
   const auto type = obj["type"].toString();
   if(!types.contains(type)) {
       ERR(QString("Non supported object type:\"%1\"").arg(type))
   }
   return types[type];
}


std::optional<FigmaParser::Components> FigmaParser::components(const QJsonObject& project, FigmaParserData& data) {
        Components map; 
        auto componentObjects = getObjectsByType(project["document"].toObject(), "COMPONENT");
        const auto components = project["components"].toObject();
        for (const auto& key : components.keys()) {
            if(!componentObjects.contains(key)) {
                const auto response = data.nodeData(key);
                if(response.isEmpty()) {
                    ERR(toStr("Component not found", key, "for"))
                }
                QJsonParseError err;
                const auto obj = QJsonDocument::fromJson(response, &err).object();
                if(err.error == QJsonParseError::NoError) {
                    const auto receivedObjects = getObjectsByType(obj["nodes"]
                            .toObject()[key]
                            .toObject()["document"]
                            .toObject(), "COMPONENT");
                    if(!receivedObjects.contains(key)) {
                         ERR(toStr("Unrecognized component", key));
                    }
                    componentObjects.insert(key, receivedObjects[key]);
                } else {
                    ERR(toStr("Invalid component", key));
                }
            }
            const auto c = components[key].toObject();
            const auto componentName = c["name"].toString();
            auto uniqueComponentName = validFileName(componentName, false); //names are expected to be unique, so we ensure so
            /*
            int count = 1;
            while(std::find_if(map.begin(), map.end(), [&uniqueComponentName](const auto& c) {
                return c->name() == uniqueComponentName;
            }) != map.end()) {
                uniqueComponentName = validFileName(QString("%1_%2").arg(componentName, count), false);
                ++count;
            }

            qDebug() << "uniqueComponentName" << uniqueComponentName << " " << foo;*/


            map.insert(key, std::shared_ptr<Component>(new Component(
                                uniqueComponentName,
                                key,
                                c["key"].toString(),
                                c["description"].toString(),
                                std::move(componentObjects[key]))));

        }
        return map;
    }

    std::optional<FigmaParser::Canvases> FigmaParser::canvases(const QJsonObject& project, FigmaParserData& data) {
        Canvases array;

        const auto doc = project["document"].toObject();
        const auto canvases = doc["children"].toArray();
        for(const auto& c : canvases) {
            const auto canvas = c.toObject();
            const auto children = canvas["children"].toArray();
            Canvas::ElementVector vec;
            std::transform(children.begin(), children.end(), std::back_inserter(vec), [](const auto& f){return f.toObject();});
            const auto col = canvas["backgroundColor"].toObject();
            array.push_back({
                                canvas["name"].toString(),
                                canvas["id"].toString(),
                                toColor(col["r"].toDouble(), col["g"].toDouble(), col["b"].toDouble(), col["a"].toDouble()),
                                std::move(vec)});
        }
        return array;
    }

     std::optional<FigmaParser::Element> FigmaParser::component(const QJsonObject& obj, unsigned flags, FigmaParserData& data, const Components& components) {
        FigmaParser p(flags | Flags::ParseComponent, data, &components);
        return p.getElement(obj);
    }

     std::optional<FigmaParser::Element> FigmaParser::element(const QJsonObject& obj, unsigned flags, FigmaParserData& data, const Components& components) {
        FigmaParser p(flags, data, &components);
        return p.getElement(obj);
    }

    QString FigmaParser::name(const QJsonObject& project) {
         return project["name"].toString();
    }

    QString FigmaParser::validFileName(const QString& itemName, bool inited) {
        if(itemName.isEmpty())
            return QString();

        Q_ASSERT(inited == itemName.endsWith(FIGMA_SUFFIX));
        auto name = itemName;

        // TODO this makes store-restore not to work as shall be cleaned inbetween access
        // static is a bit cheap solution to ensure unique names, but proper fix (be a class member) requires some refactoring
        // TODO a 'elegant' fix.
        static QMap<QString, int> unique_names;

        if(!inited) {
            auto it = unique_names.find(name);
            if(it == unique_names.end()) {
                unique_names.insert(name, 0);
            } else {
                *it += 1; // if there add a name - note that this can be non-unique as well... :-/ ooof but let's go with this untill
                name += std::to_string(*it);    // properly done....(if ever)
            }
            name += FIGMA_SUFFIX;
           }

        return makeFileName(name);
    }

    QString FigmaParser::makeFileName(const QString& fileName) {
        auto name = fileName;
        static const QRegularExpression re(R"([\\\/:*?"<>|\s])");
        name.replace(re, QLatin1String("_"));
        static const QRegularExpression ren(R"([^a-zA-Z0-9_])");
        name = name.replace(ren, "_");
        if(!((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z'))) {
            name.insert(0, "C");
        }
        name.replace(0, 1, name[0].toUpper());
        return name;
    }

    FigmaParser::FigmaParser(unsigned flags, FigmaParserData& data, const Components* components) : m_flags(flags), m_data(data), m_components(components), m_parent{nullptr, nullptr} {}

    FigmaParser::~FigmaParser() {
        while(m_parent.parent)      // this is NOT very rigid, but at least not leak :-/ (better would be assert here that all memory has freed (what push, its pop))
            m_parent.pop();
    }

    QHash<QString, QJsonObject> FigmaParser::getObjectsByType(const QJsonObject& obj, const QString& type) {
        QHash<QString, QJsonObject>objects;
        if(obj["type"] == type) {
            objects.insert(obj["id"].toString(), obj);
        } else if(obj.contains("children")) {
            const auto children = obj["children"].toArray();
            for(const auto& child : children) {
                const auto childrenobjects = getObjectsByType(child.toObject(), type);
                const auto keys = childrenobjects.keys();
                for(const auto& k : keys) {
                    objects.insert(k, childrenobjects[k]);
                }
            }
        }
        return objects;
    }

    QJsonObject FigmaParser::delta(const QJsonObject& instance, const QJsonObject& base, const QSet<QString>& ignored, const QHash<QString, std::function<QJsonValue (const QJsonValue&, const QJsonValue&)>>& compares) {
        QJsonObject newObject;
        for(const auto& k : instance.keys()) {
            if(ignored.contains(k))
                continue;
            if(!base.contains(k)) {
                 newObject.insert(k, instance[k]);
            } else if(compares.contains(k)) {
                const QJsonValue ret = compares[k](base[k], instance[k]);
                if(ret.type() != QJsonValue::Null) {
                     newObject.insert(k, ret);
                }
            } else if(base[k] != instance[k]) {
                newObject.insert(k, instance[k]);
            }
        }
        //These items get wiped off, but are needed later - so we put them back
        if(!newObject.isEmpty()) {
            if(!ignored.contains("name") && instance.contains("name"))
                newObject.insert("name", instance["name"]);
        }
        return newObject;
    }

    QHash<QString, QString> FigmaParser::children(const QJsonObject& obj) {
        QHash<QString, QString> cList;
        if(obj.contains("children")) {
            const auto children = obj["children"].toArray();
            for(const auto& c : children) {
                const auto childObj = c.toObject();
                cList.insert(childObj["id"].toString(), childObj["name"].toString());
            }
        }
        return cList;
    }

    std::optional<FigmaParser::Element> FigmaParser::getElement(const QJsonObject& obj) {
        m_parent.push(&obj);
        RAII_ raii {[this](){m_parent.pop();}};
        auto bytes = parse(obj, 1);
        if(!bytes)
            return std::nullopt;
        QStringList ids(m_componentIds.begin(), m_componentIds.end());

        // Yet another face slap due poor parser desing
        // let's dig out the images used in this element
        QStringList image_contexts;
        for(const auto& k : m_imageContext) {
           // if(!m_parent.parent)                // top level has all underneath
                image_contexts.append(k);
            /*else {
                bool found = false;
                auto p = &m_parent;
                while(p && !found) {                        // go thru all levels
                    Q_ASSERT(p->obj);
                    const auto& o = *p->obj;
                    found = o["id"] == obj["id"];      // until 'this' level
                    for(const auto& context : qAsConst(v)) {          // see if context has
                        if(context ==  o["id"])        // this or underneath images
                            image_contexts.append(k);
                    }
                }
            }*/
        }

        QVector<QString> aliases;
        for(const auto& alias : m_aliases)
            aliases.append(alias.id);

        return Element{
                validFileName(obj["name"].toString(), false),
                obj["id"].toString(),
                obj["type"].toString(),
                std::move(bytes.value()),
                std::move(ids),
                std::move(image_contexts),
                std::move(aliases),
                m_componetStreams // can this be moved?
        };
    }

    QString FigmaParser::tabs(int intendents) const {
        return QString(m_intendent).repeated(intendents);
    }

#if 0
    QRectF boundingRect(const QJsonObject& obj) const {
        QRectF bounds = {0, 0, 0, 0};
        const auto so = obj["size"].toObject();
        const QSizeF size = {so["x"].toDouble(), so["y"].toDouble()};
        const auto fills = obj["fillGeometry"].toArray();
        for(const auto f : fills) {
            const auto fill = f.toObject();
            const auto fillPath = fill["path"].toString();
            if(!fillPath.isEmpty()) {
                const auto r = boundingRect(fillPath, size);
                bounds = bounds.united(r);
            }
        }
        const auto strokes = obj["strokeGeometry"].toArray();
        for(const auto s : strokes) {
            const auto stroke = s.toObject();
            const auto strokePath = stroke["path"].toString();
            if(!strokePath.isEmpty()) {
                const auto r = boundingRect(strokePath, size);
                bounds = bounds.united(r);
            }
        }
        return bounds;
    }

    QRectF boundingRect(const QString& svgPath, const QSizeF& size) const {
        QByteArray svgData;
        svgData += "<svg version=\"1.1\"\n";
        svgData += QString("width=\"%1\" height=\"%2\"\n").arg(size.width()).arg(size.height());
        svgData += "xmlns=\"http://www.w3.org/2000/svg\">\n";
        svgData += "<path id=\"path\" d=\"" + svgPath + "\"/>\n";
        svgData += "</svg>\n";
        QSvgRenderer renderer(svgData);
        if(!renderer.isValid()) {
            ERR("Invalid SVG path", svgData);
        }
        return renderer.boundsOnElement("path");
    }
#endif
     QByteArray FigmaParser::toColor(double r, double g, double b, double a) {
        return QString("\"#%1%2%3%4\"")
                .arg(static_cast<unsigned>(std::round(a * 255.)), 2, 16, QLatin1Char('0'))
                .arg(static_cast<unsigned>(std::round(r * 255.)), 2, 16, QLatin1Char('0'))
                .arg(static_cast<unsigned>(std::round(g * 255.)), 2, 16, QLatin1Char('0'))
                .arg(static_cast<unsigned>(std::round(b * 255.)), 2, 16, QLatin1Char('0')).toLatin1();
    }

     QByteArray FigmaParser::makeId(const QJsonObject& obj)  {
        QString cid = obj["id"].toString();
        static const QRegularExpression re(R"([^a-zA-Z0-9])");
        const auto qml_id = "figma_" + cid.replace(re, "_").toLower().toLatin1();
        if(qml_id == "figma_711_1213") {
            qDebug() << "what is here?";
        }
        /*
        TODO: Really not working - why there are unable to find_id errors
        if(m_parent.parent->parent)  // presumably this prevents toplevel alias?*/
            m_parent.ids.insert(QString(qml_id));
        return qml_id;
    }


    QByteArray FigmaParser::makeId(const QString& prefix, const QJsonObject& obj)  {
        QString cid = obj["id"].toString();
        static const QRegularExpression re(R"([^a-zA-Z0-9])");
        const auto qml_id = prefix.toLatin1() + "figma_" + cid.replace(re, "_").toLower().toLatin1();
        m_parent.ids.insert(QString(qml_id));
        return qml_id;
    }

     QByteArray FigmaParser::makeComponentInstance(const QString& type, const QJsonObject& obj, int intendents) {
         QByteArray out;
         const auto intendent = tabs(intendents - 1);
         const auto intendent1 = tabs(intendents);
         out += intendent + type + " {\n";
         Q_ASSERT(obj.contains("type") && obj.contains("id"));

         out += intendent + "// component (Instance) level: " + QString::number(m_componentLevel)  + " \n";
         if(m_componentLevel == 0) {
            const auto id_string = makeId(obj);
            out += intendent1 + "id: " + id_string + "\n";
            if(obj["name"].toString().startsWith(QML_TAG))
                m_aliases.append({id_string, obj});
         }

         out += intendent1 + "// # \"" + obj["name"].toString().replace("\"", "\\\"") + "\"\n";
         if(!isQul()) // what was the role of objectName? To document, however not applicable for Qt for MCU
            out += intendent1 + "objectName:\"" + obj["name"].toString().replace("\"", "\\\"") + "\"\n";
         return out;
     }

    QByteArray FigmaParser::makeItem(const QString& type, const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto intendent1 = tabs(intendents);
        out += makeComponentInstance(type, obj, intendents);
        out += makeEffects(obj, intendents);
        out += makeTransforms(obj, intendents);
        if(obj.contains("visible") && !obj["visible"].toBool()) {
            out += intendent1 + "visible: false\n";
        }
        if(obj.contains("opacity")) {
             out += intendent1 + "opacity: " +  QString::number(obj["opacity"].toDouble()) + "\n";
        }
        return out;
    }

    QPointF FigmaParser::position(const QJsonObject& obj) const {
        const auto rows = obj["relativeTransform"].toArray();
        const auto row1 = rows[0].toArray();
        const auto row2 = rows[1].toArray();
        return {row1[2].toDouble(), row2[2].toDouble()};
    }

    QByteArray FigmaParser::makeExtents(const QJsonObject& obj, int intendents, const QRectF& extents) {
        QByteArray out;
        QString horizontal("LEFT");
        QString vertical("TOP");
        const auto intendent = tabs(intendents);
        if(obj.contains("constraints")) {
            const auto constraints = obj["constraints"].toObject();
            vertical = constraints["vertical"].toString();
            horizontal = constraints["horizontal"].toString();
        }
        if(obj.contains("relativeTransform")) { //even figma may contain always this, the deltainstance may not
            const auto p = position(obj);
            Q_ASSERT(m_parent.parent);
            const bool top_level = m_parent.parent->parent == nullptr;
            const auto tx = static_cast<int>(!top_level ? p.x() + extents.x() : extents.x());
            const auto ty = static_cast<int>(!top_level ? p.y() + extents.y() : extents.y());

            Q_ASSERT(tx >= -1080);
            Q_ASSERT(tx < 2060);
            Q_ASSERT(ty >= -1080);
            Q_ASSERT(ty < 2060);


            if(horizontal == "LEFT" || horizontal == "SCALE" || horizontal == "LEFT_RIGHT" || horizontal == "RIGHT") {
                out += intendent + QString("x:%1\n").arg(tx);
            } else if(horizontal == "CENTER") {
                const auto parentWidth = m_parent["size"].toObject()["x"].toDouble();
                const auto extent_id = QString(makeId(*m_parent.obj));
                const auto width = getValue(obj, "size").toObject()["x"].toDouble();
                const auto staticWidth = (parentWidth - width) / 2. - tx;
                if(eq(staticWidth, 0))
                    out += intendent + QString("x: (%1.width - width) / 2\n").arg(extent_id);
                else
                    out += intendent + QString("x: (%1.width - width) / 2 %2 %3\n").arg(extent_id).arg(staticWidth < 0 ? "+" : "-").arg(std::abs(staticWidth));
            }

            if(vertical == "TOP" || vertical == "SCALE" || vertical == "TOP_BOTTOM" || vertical == "BOTTOM") {
               out += intendent + QString("y:%1\n").arg(ty);
            } else  if(vertical == "CENTER") {
                const auto parentHeight = m_parent["size"].toObject()["y"].toDouble();
                const auto extent_id = QString(makeId(*m_parent.obj));
                const auto height = getValue(obj, "size").toObject()["y"].toDouble();
                const auto staticHeight = (parentHeight - height) / 2. - ty;
                if(eq(staticHeight, 0))
                    out += intendent + QString("y: (%1.height - height) / 2\n").arg(extent_id);
                else
                    out += intendent + QString("y: (%1.height - height) / 2 %2 %3\n").arg(extent_id).arg(staticHeight < 0 ? "+" : "-").arg(std::abs(staticHeight));
            }
        }
        if(obj.contains("size")) {
            const auto vec = obj["size"];
            const auto width = vec["x"].toDouble();
            const auto height = vec["y"].toDouble();
                out += intendent + QString("width:%1\n").arg(width + extents.width());
                out += intendent + QString("height:%1\n").arg(height + extents.height());
        }
        return out;
    }

    QByteArray FigmaParser::makeSize(const QJsonObject& obj, int intendents, const QSizeF& extents) {
        QByteArray out;
        const auto intendent = tabs(intendents);
        const auto s = obj["size"].toObject();
        const auto width = s["x"].toDouble()  + extents.width();
        const auto height = s["y"].toDouble()  + extents.height();
        out += intendent + QString("width:%1\n").arg(width);
        out += intendent + QString("height:%1\n").arg(height);
        return out;
    }

    QByteArray FigmaParser::makeColor(const QJsonObject& obj, int intendents, double opacity) {
        QByteArray out;
        const auto intendent = tabs(intendents);
        Q_ASSERT(obj["r"].isDouble() && obj["g"].isDouble() && obj["b"].isDouble() && obj["a"].isDouble());
        out += intendent + "color:" + toColor(obj["r"].toDouble(), obj["g"].toDouble(), obj["b"].toDouble(), obj["a"].toDouble() * opacity) + "\n";
        return out;
    }

    QByteArray FigmaParser::makeEffects(const QJsonObject& obj, int intendents) {
        QByteArray out;

        if(isQul())
            return out; // Qul has no effects

        if(obj.contains("effects")) {
                const auto effects = obj["effects"].toArray();
                if(!effects.isEmpty()) {
                    const auto intendent = tabs(intendents);
                    const auto intendent1 = tabs(intendents + 1);
                    const auto& e = effects[0];   //Qt supports only ONE effect!
                    const auto effect = e.toObject();
                    if(effect["type"] == "INNER_SHADOW" || effect["type"] == "DROP_SHADOW") {
                        const auto color = e["color"].toObject();
                        const auto radius = e["radius"].toDouble();
                        const auto offset = e["offset"].toObject();
                        out += tabs(intendents) + "layer.enabled:true\n";
                        out += tabs(intendents) + "layer.effect: DropShadow {\n";
                        if(effect["type"] == "INNER_SHADOW") {
                            out += intendent1 + "horizontalOffset: " + QString::number(-offset["x"].toDouble()) + "\n";
                            out += intendent1 + "verticalOffset: " + QString::number(-offset["y"].toDouble()) + "\n";
                        } else {
                            out += intendent1 + "horizontalOffset: " + QString::number(offset["x"].toDouble()) + "\n";
                            out += intendent1 + "verticalOffset: " + QString::number(offset["y"].toDouble()) + "\n";
                        }
                        out += intendent1 + "radius: " + QString::number(radius) + "\n";
                        out += intendent1 + "samples: 17\n";
                        out += intendent1 + "color: " + toColor(
                                color["r"].toDouble(),
                                color["g"].toDouble(),
                                color["b"].toDouble(),
                                color["a"].toDouble()) + "\n";
                        out += intendent + "}\n";
                    }
                }
        }
        return out;
    }


    QByteArray FigmaParser::makeTransforms(const QJsonObject& obj, int intendents) {
        QByteArray out;
        if(obj.contains("relativeTransform")) {
            const auto rows = obj["relativeTransform"].toArray();
            const auto row1 = rows[0].toArray();
            const auto row2 = rows[1].toArray();
            const auto intendent = tabs(intendents + 1);

            const double r1[3] = {row1[0].toDouble(), row1[1].toDouble(), row1[2].toDouble()};
            const double r2[3] = {row2[0].toDouble(), row2[1].toDouble(), row2[2].toDouble()};

            if(!eq(r1[0], 1.0) || !eq(r1[1], 0.0) || !eq(r2[0], 0.0) || !eq(r2[1], 1.0)) {
                out += tabs(intendents) + "transform: Matrix4x4 {\n";
                out += intendent + "matrix: Qt.matrix4x4(\n";
                out += intendent + QString("%1, %2, %3, 0,\n").arg(r1[0]).arg(r1[1]).arg(r1[2]);
                out += intendent + QString("%1, %2, %3, 0,\n").arg(r2[0]).arg(r2[1]).arg(r2[2]);

                out += intendent + "0, 0, 1, 0,\n";
                out += intendent + "0, 0, 0, 1)\n";
                out += tabs(intendents) + "}\n";
            }
        }
        return out;
    }

    EByteArray FigmaParser::makeImageSource(const QString& image, bool isRendering, int intendents, const QString& placeHolder) {
        QByteArray out;
        m_imageContext.insert(image);
        auto imageData = m_data.imageData(image, isRendering);
        if(imageData.isEmpty()) {
            if(placeHolder.isEmpty()) {
                ERR("Cannot read imageRef", image)
            } else {
                imageData = m_data.imageData(placeHolder, isRendering);
                 if(imageData.isEmpty()) {
                     ERR("Cannot load placeholder");
                 }
                out += tabs(intendents) + "//Image load failed, placeholder\n";
                out += tabs(intendents) + "sourceSize: Qt.size(parent.width, parent.height)\n";
            }
        }

        for(auto  pos = 1024 ; pos < imageData.length(); pos+= 1024) { //helps source viewer....
            imageData.insert(pos, "\" +\n \"");
        }

        out += tabs(intendents) + "source: \"" + imageData + "\"\n";
        return out;
    }

    EByteArray FigmaParser::makeImageRef(const QString& image, int intendents) {
        QByteArray out;
        const auto intendent = tabs(intendents + 1);
        out += tabs(intendents) + "Image {\n";
        out += intendent + "anchors.fill: parent\n";
        if(!isQul())
            out += intendent + "mipmap: true\n";
        out += intendent + "fillMode: Image.PreserveAspectCrop\n";
        APPENDERR(out, makeImageSource(image, false, intendents + 1));
        out += tabs(intendents) + "}\n";
        return out;
    }

    // if graddients are not supported flat them to a single color - kind of weighted avg
    QByteArray FigmaParser::makeGradientToFlat(const QJsonObject& obj, int intendents) {
        double a0 = 0;
        double r0 = 0;
        double g0 = 0;
        double b0 = 0;

        double a = 0;
        double r = 0;
        double g = 0;
        double b = 0;

        auto current_pos = 0;
        const auto gradients_stops = obj["gradientStops"].toArray();
        for(const auto& s : gradients_stops) {
            const auto stop = s.toObject();
            const auto pos =  stop["position"].toDouble();

            if(current_pos < pos) {
                const auto f = pos - current_pos;
                a0 += a * f;
                r0 += r * f;
                g0 += g * f;
                b0 += b * f;
            }

            current_pos = pos;

            const auto col = stop["color"].toObject();
            r = col["r"].toDouble();
            g = col["g"].toDouble();
            b = col["b"].toDouble();
            a = col["a"].toDouble();
        }

        if(current_pos < 1) {
            const auto f = 1 - current_pos;
            a0 += a * f;
            r0 += r * f;
            g0 += g * f;
            b0 += b * f;
        }

        auto intendent = tabs(intendents);
        QByteArray o;
        o += intendent + "color: " + FigmaParser::toColor(r0, g0, b0, a0) + "\n";
        return o;
    }

    EByteArray FigmaParser::makeGradientFill(const QJsonObject& obj, int intendents) {
        QByteArray out;
        out  += tabs(intendents) + "gradient: \n";
        const auto idt1 = tabs(intendents + 1);
        const auto idt2 = tabs(intendents + 2);
        const auto idt3 = tabs(intendents + 3);

        unsigned handle_pos = 0;
        const auto handle_positions = obj["gradientHandlePositions"].toArray();
        const auto gradients_stops = obj["gradientStops"].toArray();

        /*const auto iterateStops = [&](){
            if(handle_pos < handle_positions.size())
                return std::make_optional<QPair<QJsonValue>>(QPair{});
        };
        */
        if(obj["type"].toString() == "GRADIENT_LINEAR" ) {
            out += idt1 + "LinearGradient {\n";
            out += idt2 + "x1:" + QString::number(handle_positions[0].toObject()["x"].toDouble()) + "\n";
            out += idt2 + "y1:" + QString::number(handle_positions[0].toObject()["y"].toDouble()) + "\n";

            out += idt2 + "x2:" + QString::number(handle_positions[1].toObject()["x"].toDouble()) + "\n";
            out += idt2 + "y2:" + QString::number(handle_positions[1].toObject()["y"].toDouble()) + "\n";

            for(const auto& stop : gradients_stops) {
                out += idt2 + "GradientStop {\n";
                out += idt3 + "position: " + QString::number(stop.toObject()["position"].toDouble()) + "\n";
                out += makeColor(stop.toObject()["color"].toObject(), intendents + 3);
                out += idt2 + "}\n";
            }

             out += idt1 + "}\n";

        } else if(obj.contains("type") && obj["type"].toString() == "GRADIENT_RADIAL" ) {
            qDebug() << "todo"; return std::nullopt;
        } else if(obj.contains("type") && obj["type"].toString() == "GRADIENT_ANGULAR" ) {
            qDebug() << "todo"; return std::nullopt;
        } else if(obj.contains("type") && obj["type"].toString() == "GRADIENT_DIAMOND" ) {
            qDebug() << "todo"; return std::nullopt;
        } else {
            return std::nullopt;
        }
        return out;
    }

    EByteArray FigmaParser::makeFill(const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto invisible = obj.contains("visible") && !obj["visible"].toBool();
        if(obj.contains("color")) {
            const auto color = obj["color"].toObject();
            if(!invisible && obj.contains("opacity")) {
                out += makeColor(color, intendents, obj["opacity"].toDouble());
            } else {
                out += makeColor(color, intendents, invisible ? 0.0 : 1.0);
            }
        } else if(obj.contains("type")) {
            if(m_flags & FigmaParser::NoGradients) {
                out += makeGradientToFlat(obj, intendents);
            } else {
                const auto f = makeGradientFill(obj, intendents);
                if(f)
                    out += f.value();
                else
                    out += makeGradientToFlat(obj, intendents);
            }
        } else {
            out  += tabs(intendents) + "color: \"transparent\"\n";
        }
        if(obj.contains("imageRef")) {
            APPENDERR(out, makeImageRef(obj["imageRef"].toString(), intendents + 1));
        }
        return out;
    }

    EByteArray FigmaParser::makeVector(const QJsonObject& obj, int intendents) {
        QByteArray out;
        out += makeExtents(obj, intendents);
        const auto fills = obj["fills"].toArray();
        if(fills.size() > 0) {
           APPENDERR(out, makeFill(fills[0].toObject(), intendents));
        } else if(!obj["fills"].isString()) {
            out += tabs(intendents) + "color: \"transparent\"\n"; // by default vector shape background is transparent
        }
        return out;
    }



    QByteArray FigmaParser::makeStrokeJoin(const QJsonObject& stroke, int intendent) {
        QByteArray out;
        if(stroke.contains("strokeJoin")) {
            const QHash<QString, QString> joins = {
               {"MITER", "MiterJoin"},
               {"BEVEL", "MiterBevel"},
               {"ROUND", "MiterRound"}
            };
             out += tabs(intendent) + "joinStyle: ShapePath."  + joins[stroke["strokeJoin"].toString()] + "\n";
        } else {
            out += tabs(intendent) + "joinStyle: ShapePath.MiterJoin\n";
        }
        return out;
    }

    QByteArray FigmaParser::makeShapeStroke(const QJsonObject& obj, int intendents, StrokeType type) {
        QByteArray out;
        const auto intendent =  tabs(intendents);
        const QByteArray colorType = obj["type"] == "LINE" ? "fillColor" : "strokeColor"; //LINE works better this way
        if(obj.contains("strokes") && !obj["strokes"].toArray().isEmpty()) {
            const auto stroke = obj["strokes"].toArray()[0].toObject();
            out += makeStrokeJoin(stroke, intendents);
            const auto opacity = stroke.contains("opacity") ? stroke["opacity"].toDouble() : 1.0;
            const auto color = stroke["color"].toObject();
            out += intendent + colorType + ": " + toColor(
                    color["r"].toDouble(),
                    color["g"].toDouble(),
                    color["b"].toDouble(),
                    color["a"].toDouble() * opacity) + "\n";
        } else if(!obj["strokes"].isString()) {
             out += intendent + colorType + ": \"transparent\"\n";
        }
        if(obj.contains("strokeWeight")) {
            auto val = 1.0;
            if(type != StrokeType::OnePix) {
                val = obj["strokeWeight"].toDouble();
                if(type == StrokeType::Double)
                    val *= 2.0;
                }
            out += intendent + "strokeWidth:" + QString::number(val) + "\n";
        }
        return out;
    }

    QByteArray FigmaParser::makeShapeFill(const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto intendent =  tabs(intendents);
        if(obj["type"] != "LINE") {
            if(obj.contains("fills") && !obj["fills"].toArray().isEmpty()) {
                const auto fills = obj["fills"].toArray();
                const auto fill = fills[0].toObject();
                const double opacity = fill.contains("opacity") ? fill["opacity"].toDouble() : 1.0;
                const auto color = fill["color"].toObject();
                out += intendent + "fillColor:" + toColor(
                        color["r"].toDouble(),
                        color["g"].toDouble(),
                        color["b"].toDouble(),
                        color["a"].toDouble() * opacity) + "\n";
            } else if(!obj["fills"].isString()){
                out += intendent + "fillColor:\"transparent\"\n";
            }
        } else
            out += intendent + "strokeColor: \"transparent\"\n";

        out += intendent + "// component (shapeFill) level: " + QString::number(m_componentLevel)  + " \n";
        if(m_componentLevel == 0)
            out += intendent + "id: " + makeId("svgpath_" , obj) + "\n";

        return out;
    }

    EByteArray FigmaParser::makePlainItem(const QJsonObject& obj, int intendents) {
         QByteArray out;
         out += makeItem("Rectangle", obj, intendents); //TODO: set to item
         APPENDERR(out, makeFill(obj, intendents));
         out += makeExtents(obj, intendents);
         APPENDERR(out, parseChildren(obj, intendents));
         out += tabs(intendents - 1) + "}\n";
         return out;
    }

    QByteArray FigmaParser::makeSvgPath(int index, bool isFill, const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto intendent = tabs(intendents);
        const auto intendent1 = tabs(intendents + 1);

        const auto array = isFill ? obj["fillGeometry"].toArray() : obj["strokeGeometry"].toArray();
        const auto path = array[index].toObject();
        if(index == 0 && path["windingRule"] == "NONZERO") { //figma let set winding for each path, QML does not
            out += intendent +  "fillRule: ShapePath.WindingFill\n";
        }

        out += intendent + "PathSvg {\n";
        out += intendent1 + "path: \"" + path["path"].toString() + "\"\n";
        out += intendent + "} \n";
        return out;
    }

    EByteArray FigmaParser::parse(const QJsonObject& obj, int intendents) {
        const auto type = obj["type"].toString();
        const QHash<QString, std::function<EByteArray (const QJsonObject&, int)> > parsers {
            {"RECTANGLE", std::bind(&FigmaParser::parseVector, this, std::placeholders::_1, std::placeholders::_2)},
            {"TEXT", std::bind(&FigmaParser::parseText, this, std::placeholders::_1, std::placeholders::_2)},
            {"COMPONENT", std::bind(&FigmaParser::parseComponent, this, std::placeholders::_1, std::placeholders::_2)},
            {"BOOLEAN_OPERATION", std::bind(&FigmaParser::parseBooleanOperation, this, std::placeholders::_1, std::placeholders::_2)},
            {"INSTANCE", std::bind(&FigmaParser::parseInstance, this, std::placeholders::_1, std::placeholders::_2)},
            {"ELLIPSE", std::bind(&FigmaParser::parseVector, this, std::placeholders::_1, std::placeholders::_2)},
            {"VECTOR", std::bind(&FigmaParser::parseVector, this, std::placeholders::_1, std::placeholders::_2)},
            {"LINE", std::bind(&FigmaParser::parseVector, this, std::placeholders::_1, std::placeholders::_2)},
            {"REGULAR_POLYGON", std::bind(&FigmaParser::parseVector, this, std::placeholders::_1, std::placeholders::_2)},
            {"STAR", std::bind(&FigmaParser::parseVector, this, std::placeholders::_1, std::placeholders::_2)},
            {"GROUP", std::bind(&FigmaParser::parseFrame, this, std::placeholders::_1, std::placeholders::_2)},
            {"FRAME", std::bind(&FigmaParser::parseFrame, this, std::placeholders::_1, std::placeholders::_2)},
            {"COMPONENT_SET", std::bind(&FigmaParser::parseFrame, this, std::placeholders::_1, std::placeholders::_2)},
            {"SLICE", std::bind(&FigmaParser::parseSkip, this, std::placeholders::_1, std::placeholders::_2)},
            {"STAMP", std::bind(&FigmaParser::parseSkip, this, std::placeholders::_1, std::placeholders::_2)},
            {"STICKY", std::bind(&FigmaParser::parseSkip, this, std::placeholders::_1, std::placeholders::_2)},
            {"SHAPE_WITH_TEXT", std::bind(&FigmaParser::parseSkip, this, std::placeholders::_1, std::placeholders::_2)},
            {"NONE", [this](const QJsonObject& o, int i){return makePlainItem(o, i);}}
        };
        if(!parsers.contains(type)) {
            ERR(QString("Non supported object type:\"%1\"").arg(type))
        }
        if(isRendering(obj))
            return parseRendered(obj, intendents);
        return parsers[type](obj, intendents);
    }

    bool FigmaParser::isGradient(const QJsonObject& obj) const {
         if(obj.contains("fills")) {
             const auto array = obj["fills"].toArray();
             for(const auto& f : array)
                if(f.toObject().contains("gradientHandlePositions"))
                    return true;
         }
         return false;
    }

    std::optional<QString> FigmaParser::imageFill(const QJsonObject& obj) const {
        if(obj.contains("fills")) {
            const auto fills = obj["fills"].toArray();
            if(fills.size() > 0) {
                const auto fill = fills[0].toObject();
                if(fill.contains("imageRef")) {
                    return fill["imageRef"].toString();
                }
            }
        }
        return std::nullopt;
    }

    EByteArray FigmaParser::makeImageMaskDataQt(const QString& imageRef, const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto intendent = tabs(intendents);
        const auto intendent1 = tabs(intendents + 1);

        const auto sourceId =  makeId("source_", obj);
        const auto maskSourceId = makeId( "maskSource_", obj);


        out += intendent + "OpacityMask {\n";
        out += intendent1 + "anchors.fill:parent\n";
        out += intendent1 + "source: " + sourceId +  "\n";
        out += intendent1 + "maskSource: " + maskSourceId + "\n";
        out += intendent + "}\n";

        out += intendent + "Image {\n";
        out += intendent1 + "id: " + sourceId + "\n";
        out += intendent1 + "layer.enabled: true\n";
        out += intendent1 + "fillMode: Image.PreserveAspectCrop\n";
        out += intendent1 + "visible: false\n";
        out += intendent1 + "mipmap: true\n";
        out += intendent1 + "anchors.fill:parent\n";
        APPENDERR(out, makeImageSource(imageRef, false, intendents + 1));
        out += intendent + "}\n";


        out += intendent + "Shape {\n";
        out += intendent1 + "id: " + maskSourceId + "\n";
        out += intendent1 + "anchors.fill: parent\n";
        out += intendent1 + "layer.enabled: true\n";
        out += intendent1 + "visible: false\n";

        out += intendent1 + "ShapePath {\n";
        out += makeShapeStroke(obj, intendents + 2, StrokeType::Normal);
        out += tabs(intendents + 2) + "fillColor:\"black\"\n";
        out += makeShapeFillData(obj, intendents + 2);

        out += intendent1 + "}\n";
        out += intendent + "}\n";

        return out;
    }



    EByteArray FigmaParser::makeImageMaskDataQul(const QString& imageRef, const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto intendent = tabs(intendents);
        const auto intendent1 = tabs(intendents + 1);
        const auto sourceId = makeId("source_", obj);
        out += intendent + "Image {\n";
        out += intendent1 + "id: " + sourceId + "\n";
        out += intendent1 + "fillMode: Image.PreserveAspectCrop\n";
        out += intendent1 + "visible: true\n";
        out += intendent1 + "anchors.fill:parent\n";
        APPENDERR(out, makeImageSource(imageRef, false, intendents + 1));
        out += intendent + "}\n";
        return out;
    }

    EByteArray FigmaParser::makeImageMaskData(const QString& imageRef, const QJsonObject& obj, int intendents) {
        return isQul() ? makeImageMaskDataQul(imageRef, obj, intendents) : makeImageMaskDataQt(imageRef, obj, intendents);
    }

     QByteArray FigmaParser::makeShapeFillData(const QJsonObject& obj, int shapeIntendents) {
         QByteArray out;
         if(!obj["fillGeometry"].toArray().isEmpty()) {
             for(int i = 0; i < obj["fillGeometry"].toArray().count(); i++)
                 out += makeSvgPath(i, true, obj, shapeIntendents );
         } else if(!obj["strokeGeometry"].toArray().isEmpty()) {
             for(int i = 0; i < obj["strokeGeometry"].toArray().count(); i++)
                 out += makeSvgPath(i, false, obj, shapeIntendents);
         }
         return out;
     }


     QByteArray FigmaParser::makeAntialising(int intendents) const {
         return !isQul() && (m_flags & AntializeShapes) ? // antialiazing is not supported
            (tabs(intendents) + "antialiasing: true\n").toLatin1() : QByteArray();
     }

     /*
      * makeVectorxxxxxFill functions are redundant in purpose - but I ended up
      * to if-else hell and wrote open to keep normal/inside/outside and image/fill
      * cases managed
    */
     QByteArray FigmaParser::makeVectorNormalFill(const QJsonObject& obj, int intendents) {
         QByteArray out;
         out += makeItem("Shape", obj, intendents);
         out += makeExtents(obj, intendents);
     //    out += makeSvgPathProperty(obj, intendents);
         const auto intendent = tabs(intendents);
         out += makeAntialising(intendents);
         out += intendent + "ShapePath {\n";
         out += makeShapeStroke(obj, intendents + 1, StrokeType::Normal);
         out += makeShapeFill(obj, intendents + 1);
         out += makeShapeFillData(obj, intendents + 1);
         out += intendent + "}\n";
         out += tabs(intendents - 1) + "}\n";
         return out;
     }

     EByteArray FigmaParser::makeVectorNormalFill(const QString& image, const QJsonObject& obj, int intendents) {
         QByteArray out;
         const auto intendent = tabs(intendents);
         const auto intendent1 = tabs(intendents + 1);

         out += makeItem("Item", obj, intendents);
         out += makeExtents(obj, intendents);

        APPENDERR(out, makeImageMaskData(image, obj, intendents));

         out += intendent + "Shape {\n";
         out += intendent1 + "anchors.fill: parent\n";
         out += makeAntialising(intendents + 1);
         out += intendent1 + "ShapePath {\n";
         out += makeShapeStroke(obj, intendents + 2, StrokeType::Normal);
         out += makeShapeFill(obj, intendents + 2);
         out += makeShapeFillData(obj, intendents + 2);
         out += intendent1 + "}\n";
         out += intendent + "}\n";

         out += tabs(intendents - 1) + "} \n";
         return out;
     }

    EByteArray FigmaParser::makeVectorNormal(const QJsonObject& obj, int intendents) {
        const auto image = imageFill(obj);
        return image ? makeVectorNormalFill(*image, obj, intendents) : makeVectorNormalFill(obj, intendents);
    }

    QByteArray FigmaParser::makeVectorInsideFill(const QJsonObject& obj, int intendents) {
        QByteArray out;
        out += tabs(intendents - 1) + "// QML (SVG) supports only center borders, thus an extra mask is created for " + obj["strokeAlign"].toString()  + "\n";
        out += makeItem("Item", obj, intendents);
        out += makeExtents(obj, intendents);
        const auto borderSourceId = makeId("borderSource_", obj);

        const auto intendent = tabs(intendents);
        const auto intendent1 = tabs(intendents + 1);

        out += intendent + "Shape { \n";
        out += intendent1 + "id:" + borderSourceId + "\n";

        out += intendent1 + "anchors.fill: parent\n";
        out += makeAntialising(intendents + 1);
        out += intendent1 + "visible: false\n";
        out += intendent1 + "ShapePath {\n";
        out += makeShapeStroke(obj, intendents + 2, StrokeType::Double);
        out += makeShapeFill(obj, intendents + 2);
        out += makeShapeFillData(obj, intendents + 2);

        out += tabs(intendents + 2) + "}\n";
        out += intendent1 + "}\n";


        const auto borderMaskId = makeId("borderMask_", obj);
        out += intendent1 + "Shape {\n";
        out += intendent1 + "id: " + borderMaskId + "\n";
        out += intendent1 + "anchors.fill:parent\n";
        out += makeAntialising(intendents + 1);
        out += intendent1 + "layer.enabled: true\n"; //we drawn out of bounds
        out += intendent1 + "visible: false\n";

        out += intendent1 + "ShapePath {\n";
        const auto intendent2 = tabs(intendents + 2);
        out +=  intendent2 + "fillColor: \"black\"\n";
        out +=  intendent2 + "strokeColor: \"transparent\"\n";
        out +=  intendent2 + "strokeWidth: 0\n";
        out +=  intendent2 + "joinStyle: ShapePath.MiterJoin\n";

        out += makeShapeFillData(obj, intendents + 2);

        out += intendent1 + "}\n"; //shapemask
        out += intendent + "}\n"; //shape

        if(!isQul()) { // OpacityMask is not supported
            out += intendent + "OpacityMask {\n";
            out += intendent1 + "anchors.fill:parent\n";
            out += intendent1 + "source: " + borderSourceId +  "\n";
            out += intendent1 + "maskSource: " + borderMaskId + "\n";
            out += intendent + "}\n"; //Opacity
        }

        out += tabs(intendents - 1) + "}\n"; //Item

        return out;
    }

    EByteArray FigmaParser::makeVectorInsideFill(const QString& image, const QJsonObject& obj, int intendents) {
        QByteArray out;
        out += tabs(intendents - 1) + "// QML (SVG) supports only center borders, thus an extra mask is created for " + obj["strokeAlign"].toString()  + "\n";
        out += makeItem("Item", obj, intendents);
        out += makeExtents(obj, intendents);

        const auto borderSourceId = makeId("borderSource_", obj);

        const auto intendent = tabs(intendents);
        const auto intendent1 = tabs(intendents + 1);
        const auto intendent2 = tabs(intendents + 2);

        out += intendent + "Item {\n";
        out += intendent1 + "id:" + borderSourceId + "\n";
        out += intendent1 + "anchors.fill: parent\n";
        out += makeAntialising(intendents + 1);
        out += intendent1 + "visible: false\n";

        APPENDERR(out, makeImageMaskData(image, obj, intendents + 1));

        out += intendent1 + "Shape {\n";
        out += intendent2 + "anchors.fill: parent\n";
        out += makeAntialising(intendents + 2);

        out += intendent2 + "ShapePath {\n";
        out += makeShapeStroke(obj, intendents + 3, StrokeType::Double);
        out += makeShapeFill(obj, intendents + 3);
        out += makeShapeFillData(obj, intendents + 3);
        out += intendent2 + "}\n";
        out += intendent1 + "}\n";
        out += intendent + "}\n";

        const auto borderMaskId =  makeId("borderMask_", obj);
        out += intendent + "Shape {\n";
        out += intendent1 + "id: " + borderMaskId + "\n";
        out += intendent1 + "anchors.fill:parent\n";
        out += makeAntialising(intendents + 1);
        out += intendent1 + "layer.enabled: true\n"; //we drawn out of bounds
        out += intendent1 + "visible: false\n";

        out += intendent1 + "ShapePath {\n";
        out +=  intendent2 + "fillColor: \"black\"\n";
        out +=  intendent2 + "strokeColor: \"transparent\"\n";
        out +=  intendent2 + "strokeWidth: 0\n";
        out +=  intendent2 + "joinStyle: ShapePath.MiterJoin\n";

        out += makeShapeFillData(obj, intendents + 2);

        out += intendent1 + "}\n"; //shapemask
        out += intendent + "}\n"; //shape

        if(!isQul()) { // OpacityMask is not supported
            out += intendent + "OpacityMask {\n";
            out += intendent1 + "anchors.fill:parent\n";
            out += intendent1 + "source: " + borderSourceId +  "\n";
            out += intendent1 + "maskSource: " + borderMaskId + "\n";
            out += intendent + "}\n"; //Opacity
        }

        out += tabs(intendents - 1) + "}\n"; //Item

        return out;

    }

    EByteArray FigmaParser::makeVectorInside(const QJsonObject& obj, int intendentsBase) {
        const auto image = imageFill(obj);
        return image ? makeVectorInsideFill(*image, obj, intendentsBase) : makeVectorInsideFill(obj, intendentsBase);
    }

    QByteArray FigmaParser::makeVectorOutsideFill(const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto borderWidth = obj["strokeWeight"].toDouble();
        out += tabs(intendents - 1) + "// QML (SVG) supports only center borders, thus an extra mask is created for " + obj["strokeAlign"].toString()  + "\n";
        out += makeItem("Item", obj, intendents);
        out += makeExtents(obj, intendents, {-borderWidth, -borderWidth, borderWidth * 2., borderWidth * 2.}); //since borders shall fit in we must expand (otherwise the mask is not big enough, it always clips)

        const auto borderSourceId = makeId( "borderSource_", obj);

        const auto intendent = tabs(intendents);
        const auto intendent1 = tabs(intendents + 1);
        const auto intendent2 = tabs(intendents + 2);
        const auto intendent3 = tabs(intendents + 3);

        out += intendent + "Shape {\n";

        out += intendent1 + "x: " + QString::number(borderWidth) + "\n";
        out += intendent1 + "y: " + QString::number(borderWidth) + "\n";
        out += makeSize(obj, intendents + 1);
        out += makeAntialising(intendents + 1);
        out += intendent1 + "ShapePath {\n";
        out += makeShapeFill(obj, intendents + 2);
        out += makeShapeFillData(obj, intendents + 2);

        out += intendent2 + "strokeWidth: 0\n";
        out += intendent2 + "strokeColor: fillColor\n";
        out += intendent2 + "joinStyle: ShapePath.MiterJoin\n";


        out += intendent1 + "}\n";
        out += intendent + "}\n";

        out += intendent + "Item {\n";
        out += intendent1 + "id: " + borderSourceId + "\n";
        out += intendent1 + "anchors.fill:parent\n";
        out += intendent1 + "visible: false\n";
        out += intendent1 + "Shape {\n";
        out += makeAntialising(intendents + 2);
        out += intendent2 + "x: " + QString::number(borderWidth) + "\n";
        out += intendent2 + "y: " + QString::number(borderWidth) + "\n";
        out += makeSize(obj, intendents + 2);
        out += intendent2 + "ShapePath {\n";
        out += intendent3 + "fillColor: \"black\"\n";
        out += makeShapeStroke(obj,  intendents + 3, StrokeType::Double);

        out += makeShapeFillData(obj, intendents + 3);

        out += intendent2 + "}\n"; //shapepath
        out += intendent1 + "}\n"; //shape
        out += intendent + "}\n"; //Item


        const auto borderMaskId = makeId("borderMask_", obj);
        out += intendent + "Item {\n";
        out += intendent1 + "id: " + borderMaskId + "\n";
        out += intendent1 + "anchors.fill:parent\n";
        out += makeAntialising(intendents + 1);
        out += intendent1 + "visible: false\n";
        out += intendent1 + "Shape {\n";
        out += intendent2 + "x: " + QString::number(borderWidth) + "\n";
        out += intendent2 + "y: " + QString::number(borderWidth) + "\n";
        out += makeSize(obj, intendents + 2);

        out += intendent2 + "ShapePath {\n";
        out += intendent3 + "fillColor: \"black\"\n";
        out += intendent3 + "strokeColor: \"transparent\"\n";
        out += intendent3 + "strokeWidth: "  + QString::number(borderWidth) + "\n";
        out += intendent3 + "joinStyle: ShapePath.MiterJoin\n";

        out += makeShapeFillData(obj, intendents + 3);

        out += intendent2 + "}\n"; //shapemask
        out += intendent1 + "}\n"; //shape
        out += intendent + "}\n"; //Item

        if(!isQul()) { // OpacityMask is not supported
            out += intendent + "OpacityMask {\n";
            out += intendent1 + "anchors.fill:parent\n";
            out += intendent1 + "maskSource: " + borderMaskId +  "\n";
            out += intendent1 + "source: " + borderSourceId + "\n";
            out += intendent1 + "invert: true\n";
            out += intendent + "}\n"; //Opacity
        }

        out += tabs(intendents - 1) + "}\n"; //Item

        return out;
    }

    EByteArray FigmaParser::makeVectorOutsideFill(const QString& image, const QJsonObject& obj, int intendents) {
        QByteArray out;
        const auto borderWidth = obj["strokeWeight"].toDouble();
        out += tabs(intendents - 1) + "// QML (SVG) supports only center borders, thus an extra mask is created for " + obj["strokeAlign"].toString()  + "\n";
        out += makeItem("Item", obj, intendents);
        out += makeExtents(obj, intendents, {-borderWidth, -borderWidth, borderWidth * 2., borderWidth * 2.}); //since borders shall fit in we must expand (otherwise the mask is not big enough, it always clips)

        const auto borderSourceId = makeId("borderSource_", obj);

        const auto intendent = tabs(intendents);
        const auto intendent1 = tabs(intendents + 1);
        const auto intendent2 = tabs(intendents + 2);
        const auto intendent3 = tabs(intendents + 3);

        out += intendent + "Item {\n";
        out += intendent1 + "x: " + QString::number(borderWidth) + "\n";
        out += intendent1 + "y: " + QString::number(borderWidth) + "\n";
        out += makeSize(obj, intendents + 1);
        out += makeAntialising(intendents + 1);

        APPENDERR(out, makeImageMaskData(image, obj, intendents + 1));

        out += intendent1 + "Shape {\n";
        out += intendent2 + "anchors.fill: parent\n";
        out += makeAntialising(intendents + 2);
        out += intendent2 + "ShapePath {\n";
        out += intendent3 + "strokeColor: \"transparent\"\n";
        out += intendent3 + "strokeWidth: 0\n";
        out += intendent3 + "joinStyle: ShapePath.MiterJoin\n";
        out += makeShapeFill(obj, intendents + 3);
        out += makeShapeFillData(obj, intendents  + 3);

        out += intendent2 + "} \n";
        out += intendent1 + "} \n";
        out += intendent + "} \n";

        out += intendent + "Item {\n";
        out += intendent1 + "id: " + borderSourceId + "\n";
        out += intendent1 + "anchors.fill:parent\n";
        out += intendent1 + "visible: false\n";
        out += intendent1 + "Shape {\n";
        out += makeAntialising(intendents + 2);
        out += intendent2 + "x: " + QString::number(borderWidth) + "\n";
        out += intendent2 + "y: " + QString::number(borderWidth) + "\n";
        out += makeSize(obj, intendents + 2);
        out += intendent2 + "ShapePath {\n";
        out += intendent3 + "fillColor: \"black\"\n";
        out += makeShapeStroke(obj,  intendents + 3, StrokeType::Double);

        out += makeShapeFillData(obj, intendents + 3);

        out += intendent2 + "}\n"; //shapepath
        out += intendent1 + "}\n"; //shape
        out += intendent + "}\n"; //Item

        const auto borderMaskId = makeId("borderMask_", obj);
        out += intendent + "Item {\n";
        out += intendent1 + "id: " + borderMaskId + "\n";
        out += intendent1 + "anchors.fill:parent\n";
        out += makeAntialising(intendents + 1);
        out += intendent1 + "visible: false\n";
        out += intendent1 + "Shape {\n";
        out += intendent2 + "x: " + QString::number(borderWidth) + "\n";
        out += intendent2 + "y: " + QString::number(borderWidth) + "\n";
        out += makeSize(obj, intendents + 2);

        out += intendent2 + "ShapePath {\n";
        out += intendent3 + "fillColor: \"black\"\n";
        out += intendent3 + "strokeColor: \"transparent\"\n";
        out += intendent3 + "strokeWidth: "  + QString::number(borderWidth) + "\n";
        out += intendent3 + "joinStyle: ShapePath.MiterJoin\n";

        out += makeShapeFillData(obj, intendents + 3);

        out += intendent2 + "}\n"; //shapemask
        out += intendent1 + "}\n"; //shape
        out += intendent + "}\n"; //Item

        if(!isQul()) { // OpacityMask is not supported
            out += intendent + "OpacityMask {\n";
            out += intendent1 + "anchors.fill:parent\n";
            out += intendent1 + "maskSource: " + borderMaskId +  "\n";
            out += intendent1 + "source: " + borderSourceId + "\n";
            out += intendent1 + "invert: true\n";
            out += intendent + "}\n"; //Opacity
        }

        out += tabs(intendents - 1) + "}\n"; //Item

        return out;
    }

    EByteArray FigmaParser::makeVectorOutside(const QJsonObject& obj, int intendentsBase) {
        const auto image = imageFill(obj);
        return image ? makeVectorOutsideFill(*image, obj, intendentsBase) : makeVectorOutsideFill(obj, intendentsBase);
    }




    EByteArray FigmaParser::parseVector(const QJsonObject& obj, int intendents) {

        const auto hasBorders = obj.contains("strokes") && !obj["strokes"].toArray().isEmpty() && obj.contains("strokeWeight") && obj["strokeWeight"].toDouble() > 1.0;
        if(hasBorders && obj["strokeAlign"] == "INSIDE")
            return makeVectorInside(obj, intendents);
        if(hasBorders && obj["strokeAlign"] == "OUTSIDE")
            return makeVectorOutside(obj, intendents);
        else
            return makeVectorNormal(obj, intendents);

    }


    QJsonObject FigmaParser::toQMLTextStyles(const QJsonObject& obj) const {
        QJsonObject styles;
        const auto resolvedFunction = m_data.fontInfo(obj["fontFamily"].toString());
        styles.insert("font.family", QString("\"%1\"").arg(resolvedFunction));
        styles.insert("font.italic", QString(obj["italic"].toBool() ? "true" : "false"));
        styles.insert("font.pixelSize", QString::number(static_cast<int>(std::floor(obj["fontSize"].toDouble()))));
        styles.insert("font.weight", QString(fontWeight(obj["fontWeight"].toDouble())));
        if(obj.contains("textCase")) {
            const QHash<QString, QString> capitalization {
                {"UPPER", "Font.AllUppercase"},
                {"LOWER", "Font.AllLowercase"},
                {"TITLE", "Font.MixedCase"},
                {"SMALL_CAPS", "Font.SmallCaps"},
                {"SMALL_CAPS_FORCED", "Font.Capitalize"
                },
            };
            styles.insert("font.capitalization",  capitalization[obj["textCase"].toString()]);
        }
        if(obj.contains("textDecoration")) {
            const QHash<QString, QString> decoration {
                {"STRIKETHROUGH", "strikeout"},
                {"UNDERLINE", "underline"}
            };
            styles.insert(decoration[obj["textDecoration"].toString()], true);
        }
        if(obj.contains("paragraphSpacing")) {
            styles.insert("topPadding", QString::number(obj["paragraphSpacing"].toInt()));
        }
        if(obj.contains("paragraphIndent")) {
            styles.insert("leftPadding", QString::number(obj["paragraphIndent"].toInt()));
        }
        if(!isQul()) {
            const QHash<QString,  QString> hAlign {
                {"LEFT", "Text.AlignLeft"},
                {"RIGHT", "Text.AlignRight"},
                {"CENTER", "Text.AlignHCenter"},
                {"JUSTIFIED", "Text.AlignJustify"}
            };
            styles.insert("horizontalAlignment", hAlign[obj["textAlignHorizontal"].toString()]);
            const QHash<QString, QString> vAlign {
                {"TOP", "Text.AlignTop"},
                {"BOTTOM", "Text.AlignBottom"},
                {"CENTER", "Text.AlignVCenter"}
            };
            styles.insert("verticalAlignment", vAlign[obj["textAlignVertical"].toString()]);

            styles.insert("font.letterSpacing", QString::number(obj["letterSpacing"].toDouble()));
        }
        return styles;
    }

    EByteArray FigmaParser::parseStyle(const QJsonObject& obj, int intendents) {
         QByteArray out;
         const auto intendent = tabs(intendents);
         const auto styles = toQMLTextStyles(obj);
         for(const auto& k : styles.keys()) {
             const auto v = styles[k];
             const auto value = v.toVariant().toString();
             out += intendent + k + ": " + value + "\n";
         }
         const auto fills = obj["fills"].toArray();
         if(fills.size() > 0) {
             APPENDERR(out, makeFill(fills[0].toObject(), intendents));
         }
         return out;
    }

     bool FigmaParser::isRendering(const QJsonObject& obj) const {
        if(obj["isRendering"].toBool())
            return true;
        if(type(obj) == ItemType::Vector && (m_flags & PrerenderShapes || isGradient(obj))) // || /*(m_flags & PrerenderGradients &&*/ isGradient(obj)))
            return true;
        if(type(obj) == ItemType::Text && /*(m_flags & PrerenderGradients &&*/ isGradient((obj)))
            return true;
        if(type(obj) == ItemType::Frame && (obj["type"].toString() != "GROUP") && (m_flags & Flags::PrerenderFrames))
            return true;
        if(obj["type"].toString() == "GROUP" && (m_flags & Flags::PrerenderGroups))
            return true;
        if(type(obj) == ItemType::Component && (m_flags & Flags::PrerenderComponents /*|| (m_flags & PrerenderGradients && isGradient(obj)) */)) // prerender here makes figma respond with errors
            return true;
        if(type(obj) == ItemType::Instance && (m_flags & Flags::PrerenderInstances /*|| (m_flags & PrerenderGradients && isGradient(obj)) */))
            return true;
        return false;
    }

    EByteArray FigmaParser::parseText(const QJsonObject& obj, int intendents) {
        QByteArray out;
        out += makeItem("Text", obj, intendents);
        APPENDERR(out, makeVector(obj, intendents));
        const auto intendent = tabs(intendents);
        if(!isQul()) // word wrap is not supported
            out += intendent + "wrapMode: TextEdit.WordWrap\n";
        out += intendent + "text:\"" + obj["characters"].toString() + "\"\n";
        APPENDERR(out, parseStyle(obj["style"].toObject(), intendents));
        out += tabs(intendents - 1) + "}\n";
        return out;
     }

    QByteArray FigmaParser::parseSkip(const QJsonObject& obj, int intendents) {
        Q_UNUSED(obj);
        Q_UNUSED(intendents);
        return QByteArray();
    }

     EByteArray FigmaParser::parseFrame(const QJsonObject& obj, int intendents) {
         QByteArray out = makeItem("Rectangle", obj, intendents);
         APPENDERR(out, makeVector(obj, intendents));
         const auto intendent = tabs(intendents);
         if(obj.contains("cornerRadius")) {
             out += intendent + "radius:" + QString::number(obj["cornerRadius"].toDouble()) + "\n";
         }
         out += intendent + "clip: " + (obj["clipsContent"].toBool() ? "true" : "false") + " \n";
         APPENDERR(out, parseChildren(obj, intendents));
         out += tabs(intendents - 1) + "}\n";
         return out;
     }

     QString FigmaParser::delegateName(const QString& id) {
         auto did = id;
         did.replace(':', QLatin1String("_"));
         return ("delegate_" + did).toLatin1();
     }


     QByteArray FigmaParser::parseQtComponent(const OrderedMap<QString, QByteArray>& children, int intendents) {
       QByteArray out;
       const auto intendent = tabs(intendents);
       const auto keys = children.keys();
        for(const auto& key : keys) {
            const auto id = delegateName(key);
            const auto component_name = QString(id[0]).toUpper() + id.mid(1);
            out += intendent + QString("property Component %1: ").arg(id).toLatin1() + children[key];
            out += intendent + QString("property Item i_%1\n").arg(id);
            out += intendent + QString("property matrix4x4 %1_transform: Qt.matrix4x4(%2)\n").arg(id).arg(QString("Nan ").repeated(16).split(' ').join(",")).toLatin1();
            out += intendent + QString("on%1_transformChanged: {if(i_%2 && i_%2.transform != %2_transform) i_%2.transform = %2_transform;}\n").arg(component_name, id).toLatin1();
            const QStringList properties = {"x", "y", "width", "height"};
            for(const auto& p : properties) {
                out += intendent + QString("property real %1_%2: NaN\n").arg(id, p).toLatin1();
                out += intendent + QString("on%1_%3Changed: {if(i_%2 && i_%2.%3 != %2_%3) i_%2.%3 = %2_%3;}\n").arg(component_name, id, p).toLatin1();
            }
        }

        const auto intendent1 = tabs(intendents + 1);
        out += intendent + "Component.onCompleted: {\n";
        for(const auto& key : keys) {
            const auto dname = delegateName(key);
            out += intendent1 + "const o_" + dname + " = {}\n";
            out += intendent1 + QString("if(!isNaN(%1_transform.m11)) o_%1['transform'] = %1_transform;\n").arg(dname);
            const QStringList properties = {"x", "y", "width", "height"};
            for(const auto& p : properties) {
                out += intendent1 + QString("if(!isNaN(%1_%2)) o_%1['%2'] = %1_%2;\n").arg(dname, p);
            }
            out += intendent1 + QString("i_%1 = %1.createObject(this, o_%1)\n").arg(dname).toLatin1();
            for(const auto& p : properties) {
                out += intendent1 + QString("%1_%2 = Qt.binding(()=>i_%1.%2)\n").arg(dname, p);
            }
        }
        out += intendent + "}\n";
        return out;
    }

     QString componentName(const QString& id) {
         auto did = id;
         did.replace(':', QLatin1String("_"));
         return ("Component_" + did).toLatin1();
     }


    // Qul does not support Qt component creation, but along Qt5.15 there come new syntax
    // if this get working well have this as a common method?
    // But I really get my concept of creating Components like this ... they anyway jas unique intances,
    // and therefore can be just children, what is going on here?
    // ... It creates a component of that name, as it says and then a Item to plug a delegate and that is wrapped
    // in component... is that then some one else to access the component? Kind of chain?
     QByteArray FigmaParser::parseQulComponent(const OrderedMap<QString, QByteArray>& children, int intendents) {
         QByteArray out;
         const auto intendent = tabs(intendents);
         const auto intendent2 = tabs(intendents + 1);
         const auto keys = children.keys();
         for(const auto& key : keys) {
             const auto id = componentName(key);
           //  const auto component_name = QString(id[0]).toUpper() + id.mid(1);
             out += intendent + QString("Component {\n");
             out += intendent2 + QString("id: comp_%1\n").arg(id);
             out += intendent2 + children[key];
             out += intendent + QString("}\n");
#ifdef QUL_LOADER_WORKAROUND
            out += intendent + QString("property alias %1: loader_%2.source\n").arg(delegateName(key), id);
#else
            out += intendent + QString("property alias %1: loader_%2.sourceComponent\n").arg(delegateName(key), id);
#endif
         }

         /**
         *  For each components there is created a component and an property for it
         *
         */

        for(const auto& key : keys) {
            out += intendent + "Loader {\n";
            const auto id = componentName(key);
            out += intendent2 + QString("id: loader_%1\n").arg(id);
          //  out += intendent2 + QString("sourceComponent: comp_%1\n").arg(id);
            out += intendent + "}\n";
         }

        //for(const auto& key : keys) {
        //    const auto dname = componentName(key);
        //    out += intendent + dname + " {}\n";
        // }
         return out;
     }

     EByteArray FigmaParser::parseComponent(const QJsonObject& obj, int intendents) {
         if(!(m_flags & Flags::ParseComponent)) {
             return  parseInstance(obj, intendents);
        } else {
             const auto intendent = tabs(intendents);
             QByteArray out = makeItem("Rectangle", obj, intendents);
             APPENDERR(out, makeVector(obj, intendents));
             if(obj.contains("cornerRadius")) {
                 out += intendent + "radius:" + QString::number(obj["cornerRadius"].toDouble()) + "\n";
             }
             out += intendent + "clip: " + (obj["clipsContent"].toBool() ? "true" : "false") + " \n";

             const auto children = parseChildrenItems(obj, intendents);
             if(!children)
                 return std::nullopt;
             if(isQul())
                out += parseQulComponent(*children, intendents);
             else
               out += parseQtComponent(*children, intendents);
             out += tabs(intendents - 1) + "}\n";
             return out;
         }
     }

     EByteArray FigmaParser::parseBooleanOperationUnion(const QJsonObject& obj, int intendents, const QString& sourceId, const QString& maskSourceId) {
        Q_ASSERT(!isQul());
        QByteArray out;
         const auto intendent = tabs(intendents);
         const auto intendent1 = tabs(intendents + 1);
         out += intendent + "Rectangle {\n";
         out += intendent1 + "id: " + sourceId + "\n";
         out += intendent1 + "anchors.fill: parent\n";
         const auto fills = obj["fills"].toArray();
         if(fills.size() > 0) {
             APPENDERR(out, makeFill(fills[0].toObject(), intendents + 1));
         } else if(!obj["fills"].isString()) {
             out += intendent1 + "color: \"transparent\"\n";
         }
         out += intendent1 + "visible: false\n";
         out += intendent + "}\n";

         out += intendent + "Item {\n";
         out += intendent1 + "anchors.fill: parent\n";
         out += intendent1 + "visible: false\n";
         out += intendent1 + "id: " + maskSourceId + "\n";
         APPENDERR(out, parseChildren(obj, intendents + 1));
         out += intendent + "}\n";


        out += intendent + "OpacityMask {\n";
        out += intendent1 + "anchors.fill:" + sourceId + "\n";
        out += intendent1 + "source:" + sourceId + "\n";
        out += intendent1 + "maskSource:" + maskSourceId + "\n";
        out += intendent + "}\n";

         return out;
     }

     EByteArray FigmaParser::parseBooleanOperationSubtract(const QJsonObject& obj, const QJsonArray& children, int intendents, const QString& sourceId, const QString& maskSourceId) {
        Q_ASSERT(!isQul());
        QByteArray out;

         const auto intendent = tabs(intendents);
         const auto intendent1 = tabs(intendents + 1);
         out += intendent + "Item {\n";
         out += intendent1 + "anchors.fill: parent\n";
         out += intendent1 + "visible: false\n";
         out += intendent1 + "id: " + sourceId + "_subtract\n";
         const auto intendent2 = tabs(intendents + 2);
         out += intendent1 + "Rectangle {\n";
         out += intendent2 + "id: " + sourceId + "\n";
         out += intendent2 + "anchors.fill: parent\n";
         out += intendent2 + "visible: false\n";
         const auto fills = obj["fills"].toArray();
         if(fills.size() > 0) {
             APPENDERR(out, makeFill(fills[0].toObject(), intendents + 2));
         } else if(!obj["fills"].isString()) {
             out += intendent1 + "color: \"transparent\"\n";
         }
         out += intendent1 + "}\n";
         out += intendent1 + "Item {\n";
         out += intendent2 + "anchors.fill: parent\n";
         out += intendent2 + "visible: false\n";
         out += intendent2 + "id:" + maskSourceId + "\n";
         APPENDERR(out, parse(children[0].toObject(), intendents + 3));
         out += intendent1 + "}\n";

         out += intendent1 + "OpacityMask {\n";
         out += intendent2 + "anchors.fill:" + sourceId + "\n";
         out += intendent2 + "source:" + sourceId + "\n";
         out += intendent2 + "maskSource:" + maskSourceId + "\n";
         out += intendent1 + "}\n";
         out += intendent + "}\n";
         //This was one we subtracts from

         out += intendent + "Item {\n";
         out += intendent1 + "anchors.fill: parent\n";
         out += intendent1 + "visible: false\n";
         out += intendent1 + "id: " + maskSourceId + "_subtract\n";
         for(int i = 1; i < children.size(); i++ )
            APPENDERR(out, parse(children[i].toObject(), intendents + 2));
         out += intendent + "}\n";

         out += intendent + "OpacityMask {\n";
         out += intendent1 + "anchors.fill:" + sourceId + "_subtract\n";
         out += intendent1 + "source:" + sourceId + "_subtract\n";
         out += intendent1 + "maskSource:" + maskSourceId + "_subtract\n";
         out += intendent1 + "invert: true\n";
         out += intendent + "}\n";
         return out;
     }

     EByteArray FigmaParser::parseBooleanOperationIntersect(const QJsonObject& obj, const QJsonArray& children, int intendents, const QString& sourceId, const QString& maskSourceId) {
        Q_ASSERT(!isQul());
         QByteArray out;
         const auto intendent = tabs(intendents);
         const auto intendent1 = tabs(intendents + 1);

         out += intendent + "Rectangle {\n";
         out += intendent1 + "id: " + sourceId + "\n";
         out += intendent1 + "anchors.fill: parent\n";
         const auto fills = obj["fills"].toArray();
         if(fills.size() > 0) {
             APPENDERR(out, makeFill(fills[0].toObject(), intendents + 1));
         } else if(!obj["fills"].isString()) {
             out += intendent1 + "color: \"transparent\"\n";
         }
         out += intendent1 + "visible: false\n";
         out += intendent + "}\n";

         auto nextSourceId = sourceId;

         for(auto i = 0; i < children.size(); i++) {
             const auto maskId =  maskSourceId + "_" + QString::number(i);
             out += intendent + "Item {\n";
             out += intendent1 + "anchors.fill: parent\n";
             out += intendent1 + "visible: false\n";
             APPENDERR(out, parse(children[i].toObject(), intendents + 2));
             out += intendent1 + "id: " + maskId + "\n";
             out += intendent + "}\n";

             out += intendent + "OpacityMask {\n";
             out += intendent1 + "anchors.fill:" + sourceId + "\n";
             out += intendent1 + "source:" + nextSourceId + "\n";
             out += intendent1 + "maskSource:" + maskId + "\n";
             nextSourceId = sourceId + "_" + QString::number(i).toLatin1();
             out += intendent1 + "id: " + nextSourceId + "\n";
             if(i < children.size() - 1)
                out += intendent1 + "visible: false\n";
             out += intendent + "}\n";
         }
         return out;
     }

     EByteArray FigmaParser::parseBooleanOperationExclude(const QJsonObject& obj, const QJsonArray& children, int intendents, const QString& sourceId, const QString& maskSourceId) {
        Q_ASSERT(!isQul());
         QByteArray out;

         const auto intendent = tabs(intendents);
         const auto intendent1 = tabs(intendents + 1);

         out += intendent + "Rectangle {\n";
         out += intendent1 + "id: " + sourceId + "\n";
         out += intendent1 + "anchors.fill: parent\n";
         const auto fills = obj["fills"].toArray();
         if(fills.size() > 0) {
             APPENDERR(out, makeFill(fills[0].toObject(), intendents + 1));
         } else if(!obj["fills"].isString()) {
             out += intendent1 + "color: \"transparent\"\n";
         }
         out += intendent1 + "visible: false\n";
         out += intendent1 + "layer.enabled: true\n";


         const auto intendent2 = tabs(intendents + 2);
         const auto intendent3 = tabs(intendents + 3);
         out += intendent1 + "readonly property string shaderSource: \"\n";
         out += intendent2 + "uniform lowp sampler2D colorSource;\n";
         out += intendent2 + "uniform lowp sampler2D prevMask;\n";
         out += intendent2 + "uniform lowp sampler2D currentMask;\n";
         out += intendent2 + "uniform lowp float qt_Opacity;\n";
         out += intendent2 + "varying highp vec2 qt_TexCoord0;\n";
         out += intendent2 + "void main() {\n";
         out += intendent3 + "vec4 color = texture2D(colorSource, qt_TexCoord0);\n";
         out += intendent3 + "vec4 cm = texture2D(currentMask, qt_TexCoord0);\n";
         out += intendent3 + "vec4 pm = texture2D(prevMask, qt_TexCoord0);\n";

         out += intendent3 + "gl_FragColor = qt_Opacity * color * ((cm.a * (1.0 - pm.a)) + ((1.0 - cm.a) * pm.a));\n";
         out += intendent2 + "}\"\n";

         out += intendent1 + "readonly property string shaderSource0: \"\n";
         out += intendent2 + "uniform lowp sampler2D colorSource;\n";
         out += intendent2 + "uniform lowp sampler2D currentMask;\n";
         out += intendent2 + "uniform lowp float qt_Opacity;\n";
         out += intendent2 + "varying highp vec2 qt_TexCoord0;\n";
         out += intendent2 + "void main() {\n";
         out += intendent3 + "vec4 color = texture2D(colorSource, qt_TexCoord0);\n";
         out += intendent3 + "vec4 cm = texture2D(currentMask, qt_TexCoord0);\n";
         out += intendent3 + "gl_FragColor = cm.a * color;\n";
         out += intendent2 + "}\"\n";

         out += intendent + "}\n";

         QString nextSourceId;

         for(auto i = 0; i < children.size() ; i++) {
             const auto maskId =  maskSourceId + "_" + QString::number(i);
             out += intendent + "Item {\n";
             out += intendent1 + "visible: false\n";
             out += intendent1 + "anchors.fill: parent\n";
             APPENDERR(out, parse(children[i].toObject(), intendents + 2));
             out += intendent1 + "layer.enabled: true\n";
             out += intendent1 + "id: " + maskId + "\n";
             out += intendent + "}\n";

             out += intendent + "ShaderEffect {\n";
             out += intendent1 + "anchors.fill: parent\n";
             out += intendent1 + "layer.enabled: true\n";
             out += intendent1 + "property var colorSource:" +  sourceId + "\n";
             if(!nextSourceId.isEmpty()) {
                  out += intendent2 + "property var prevMask: ShaderEffectSource {\n";
                  out += intendent2 + "sourceItem: " + nextSourceId + "\n";
                  out += intendent1 + "}\n";

             }
             out += intendent1 + "property var currentMask:" +  maskId + "\n";
             out += intendent1 + "fragmentShader: " + sourceId + (nextSourceId.isEmpty() ? ".shaderSource0" : ".shaderSource") + "\n";
             nextSourceId = sourceId + "_" + QString::number(i).toLatin1();
             if(i < children.size() - 1) {
                  out += intendent1 + "visible: false\n";
                  out += intendent1 + "id: " + nextSourceId + "\n";
             }
             out += intendent1 + "}\n";
         }
         return out;
     }


     EByteArray FigmaParser::parseBooleanOperation(const QJsonObject& obj, int intendents) {
         if((m_flags & Flags::BreakBooleans) == 0 || isQul()) // Qul does not support boolean operations (due no OpacityMasks)
            return parseVector(obj, intendents);

         const auto children = obj["children"].toArray();
         if(children.size() < 2) {
             ERR("Boolean needs at least two elemetns");
         }
         const auto operation = obj["booleanOperation"].toString();

         QByteArray out;
         out += makeItem("Item", obj, intendents);
         out += makeExtents(obj, intendents);
         //const auto intendent = tabs(intendents);
         //const auto intendent1 = tabs(intendents + 1);
         const auto sourceId = makeId("source_", obj);
         const auto maskSourceId = makeId("maskSource_", obj);
         if(operation == "UNION") {
            APPENDERR(out, parseBooleanOperationUnion(obj, intendents, sourceId, maskSourceId));
         } else if(operation == "SUBTRACT") {
             APPENDERR(out, parseBooleanOperationSubtract(obj, children, intendents, sourceId, maskSourceId));
         } else if(operation == "INTERSECT") {
            APPENDERR(out, parseBooleanOperationIntersect(obj, children, intendents, sourceId, maskSourceId));
         } else if(operation == "EXCLUDE") {
            APPENDERR(out, parseBooleanOperationExclude(obj, children, intendents, sourceId, maskSourceId));
         } else {
             // not supported
             return QByteArray();
         }
         out += tabs(intendents - 1) + "}\n";
         return out;
     }

     QSizeF FigmaParser::getSize(const QJsonObject& obj) const {
             const auto rect = obj["absoluteBoundingBox"].toObject();
             QSizeF sz(
                     rect["width"].toDouble(),
                     rect["height"].toDouble());
             if(obj.contains("children")) {
                 const auto children = obj["children"].toArray();
                 for(const auto& child : children) {
                     sz = sz.expandedTo(getSize(child.toObject()));
                 }
             }
             return sz;
     }

     EByteArray FigmaParser::makeRendered(const QJsonObject& obj, int intendents) {
         const auto imageId = makeId("i_", obj);
         QByteArray out;
         const auto intendent = tabs(intendents );
         out += intendent + "Image {\n";
         const auto intendent1 = tabs(intendents + 1);
         out += intendent1 + "id: " + imageId + "\n";
         out += intendent1 + "anchors.centerIn: parent\n";
         if(!isQul())
             out += intendent1 + "mipmap: true\n";
         out += intendent1 + "fillMode: Image.PreserveAspectFit\n";

         APPENDERR(out, makeImageSource(obj["id"].toString(), true, intendents + 1, PlaceHolder));
         out += intendent + "}\n";
         return out;
     }


     EByteArray FigmaParser::parseRendered(const QJsonObject& obj, int intendents) {
         QByteArray out;
         out += makeComponentInstance("Item", obj, intendents);
         const auto intendent = tabs(intendents );
         Q_ASSERT(m_parent.obj->contains("absoluteBoundingBox"));
         const auto prect = m_parent["absoluteBoundingBox"].toObject();
         const auto px = prect["x"].toDouble();
         const auto py = prect["y"].toDouble();

         const auto rect = obj["absoluteBoundingBox"].toObject();  //we still need node positions
         const auto x = rect["x"].toDouble();
         const auto y = rect["y"].toDouble();

         const auto rsect = getSize(obj);
         const auto width = rsect.width();
         const auto height =  rsect.height();



         out += intendent + QString("x: %1\n").arg(x - px);
         out += intendent + QString("y: %1\n").arg(y - py);

         out += intendent + QString("width:%1\n").arg(width);
         out += intendent + QString("height:%1\n").arg(height);

         const auto invisible = obj.contains("visible") && !obj["visible"].toBool();
         if(!invisible) {  //Prerendering is not available for invisible elements
             APPENDERR(out, makeRendered(obj, intendents + 1));
         }
         out += tabs(intendents - 1) + "}\n";
         return out;
     }



     EByteArray FigmaParser::makeInstanceChildren(const QJsonObject& obj, const QJsonObject& comp, int intendents) {
        QByteArray out;
        // m_componentLevel should propably apply for Qul only
        ++m_componentLevel;
        RAII_ raii {[this](){--m_componentLevel;}};
        const auto compChildren = comp["children"].toArray();
        const auto objChildren = obj["children"].toArray();
        auto children = parseChildrenItems(obj, intendents);  //const not accepted! bug in VC??
        if(!children)
            return std::nullopt;
        if(compChildren.size() != children->size()) { //TODO: better heuristics what to do if kids count wont match, problem is z-order, but we can do better
            for(const auto& [k, bytes] : *children)
                out += bytes;
            return out;
        }
     //   QSet<QString> unmatched = QSet<QString>::fromList(children.keys());
     //   Q_ASSERT(compChildren.size() == children.size());
        const auto keys = children->keys();
        const auto intendent = tabs(intendents);
        for(const auto& cc : compChildren) {
            //first we find the corresponsing object child
            const auto cchild = cc.toObject();
            const auto id = cchild["id"].toString();
            //when it's keys last section  match
            const auto keyit = std::find_if(keys.begin(), keys.end(), [&id](const auto& key){return key.split(';').last() == id;});
            Q_ASSERT(keyit != keys.end());
            //and get its index
        //    Q_ASSERT(unmatched.contains(*keyit));
        //    unmatched.remove(*keyit);
            const auto index = std::distance(keys.begin(), keyit);
            //here we have it
            const auto objChild = objChildren[index].toObject();
            //Then we compare to to find delta, we ignore absoluteBoundingBox as size and transformations are aliases
            const auto deltaObject = delta(objChild, cchild, {"absoluteBoundingBox", "name", "id"}, {{"children", [objChild, this](const auto& o, const auto& c) {
                                                                                                          return (type(objChild) == ItemType::Boolean && !(m_flags & BreakBooleans)) || o == c ? QJsonValue() : c;
                                                                                                      }}});

            // difference, nothing to override
            if(deltaObject.isEmpty())
                continue;

            if(deltaObject.size() <= 2
                    && ((deltaObject.size() == 2
                         && deltaObject.contains("relativeTransform")
                         && deltaObject.contains("size"))
                        || (deltaObject.size() == 1
                            && (deltaObject.contains("relativeTransform")
                                || deltaObject.contains("size"))))) {
                const auto delegateId = delegateName(id);
                if(deltaObject.contains("relativeTransform")) {
                    const auto transform = makeTransforms(objChild, intendents + 1);
                    if(!transform.isEmpty())
                        out += intendent + QString("%1_transform: %2\n").arg(delegateId, QString(transform));
                    const auto pos = position(objChild);
                    out += intendent + QString("%1_x: %2\n").arg(delegateId).arg(static_cast<int>(pos.x()));
                    out += intendent + QString("%1_y: %2\n").arg(delegateId).arg(static_cast<int>(pos.y()));
                }
                if(deltaObject.contains("size")) {
                    const auto size = deltaObject["size"].toObject();
                    out += intendent + QString("%1_width: %2\n").arg(delegateId).arg(static_cast<int>(size["x"].toDouble()));
                    out += intendent + QString("%1_height: %2\n").arg(delegateId).arg(static_cast<int>(size["y"].toDouble()));
                }
                continue;
            }
            const auto child_item = (*children)[*keyit];
#ifdef QUL_LOADER_WORKAROUND
            const auto sub_component =  addComponentStream(cchild, child_item);
            Q_ASSERT(!sub_component.isEmpty());
            out += intendent + delegateName(id) + ": \"" + sub_component + "\"\n";
#else
            out += intendent + delegateName(id) + ": " + child_item;
#endif
        }
        return out;
    }

    QJsonValue FigmaParser::getValue(const QJsonObject& obj, const QString& key) const {
        if(obj.contains(key))
            return obj[key];
        else if(type(obj) == ItemType::Instance) {
            return getValue((*m_components)[obj["componentId"].toString()]->object(), key);
        }
        return QJsonValue();
    }

     EByteArray FigmaParser::parseInstance(const QJsonObject& obj, int intendents) {
         QByteArray out;
         const auto isInstance = type(obj) == ItemType::Instance;
         const auto componentId = (isInstance ? obj["componentId"] : obj["id"]).toString();
         m_componentIds.insert(componentId);

         if(!m_components->contains(componentId)) {
             ERR("Unexpected component dependency from", obj["id"].toString(), "to", componentId);
         }

         const auto comp = (*m_components)[componentId];

         if(!isInstance) {
             out += makeComponentInstance(comp->name(), obj, intendents);
         } else {

             auto instanceObject = delta(obj, comp->object(), {"children"}, {});

             //Just dummy to prevent transparent
             if(obj.contains("fills") && !instanceObject.contains("fills")) {
                 instanceObject.insert("fills", "");
             }

             //Just dummy to prevent transparent
             if(obj.contains("strokes") && !instanceObject.contains("strokes")) {
                 instanceObject.insert("strokes", "");
             }

             out += makeItem(comp->name(), instanceObject, intendents);
             APPENDERR(out, makeVector(instanceObject, intendents));

             APPENDERR(out, makeInstanceChildren(obj, comp->object(), intendents));
         }
         out += tabs(intendents - 1) + "}\n";
         return out;
     }

      EByteArray FigmaParser::parseChildren(const QJsonObject& obj, int intendents) {
          QByteArray out;
          const auto items = parseChildrenItems(obj, intendents);
          if(!items)
              return std::nullopt;
          for(const auto& [k, bytes] : *items)
              out += bytes;

          // add alias set signal

          if(!m_parent.parent->parent && (m_flags & GenerateAccess) && !m_aliases.isEmpty()) {
              const auto intend = tabs(intendents);
              const auto intend2 = tabs(intendents + 1);
              const auto intend3 = tabs(intendents + 2);
              /*if(m_flags & QulMode) {

                  out += intend + "FigmaQmlSingleton.onSetValue: {\n";

                  out += intend3 + "console.log(element, value);\n";

                  out += intend2 + "switch(element) {\n";

                  for(const auto& alias : m_aliases) {
                      const auto obj_name = alias.obj["name"].toString();
                      const auto begin = obj_name.indexOf('?');
                      const auto end = obj_name.lastIndexOf('.');
                      const auto name = obj_name.mid(begin + 1, end - begin - 1);
                      const auto var = obj_name.mid(end);
                      out += intend3 + "case '" + name + "': " + alias.id + var + " = value; break;\n";
                  }
                  out +=  intend2 + "}\n";
                  out += intend + "}\n";

              } else*/ {
                  out += intend + "Connections {\n";
                  out += intend2 + "target: FigmaQmlSingleton\n";
                  /*
                   * The standard Qt Quick offers a new QML syntax to define signal handlers as a function within a Connections type. Qt Quick Ultralite does not support this feature yet, and it does not warn if you use it.
                   * https://doc.qt.io/QtForMCUs-2.5/qtul-known-issues.html#connection-known-issues
                   */
                  out += intend2 + "onSetValue: {\n";
                  //out += intend2 + "function onSetValue(element, value) {\n";
                  //out += intend3 + "console.log('FigmaQmlSingleton-setValue:', element, value);\n";
                  out += intend3 + "switch(element) {\n";

                  for(const auto& alias : m_aliases) {
                      const auto obj_name = alias.obj["name"].toString();
                      const auto begin = obj_name.indexOf('?');
                      const auto end = obj_name.lastIndexOf('.');
                      const auto name = obj_name.mid(begin + 1, end - begin - 1);
                      const auto var = obj_name.mid(end);
                      if(var != ON_MOUSE_CLICK) {
                        const auto intend4 = tabs(intendents + 3);
                         out += intend4 + "case '" + name + "': " + alias.id + var + " = value; break;\n";
                      } else {
                          qDebug() << "TODO " << ON_MOUSE_CLICK;
                      }
                  }

                  out +=  intend3 + "}\n";
                  out +=  intend2 + "}\n";
                  out += intend + "}\n";
              }
          }
          return out;
      }

    EByteArray FigmaParser::makeChildMask(const QJsonObject& child, int intendents) {
          QByteArray out;
          const auto intendent = tabs(intendents);
          const auto intendent1 = tabs(intendents + 1);
          const auto maskSourceId = makeId("mask_", child);
          const auto sourceId = makeId("source_", child);
          out += tabs(intendents) + "Item {\n";
          out += intendent + "anchors.fill:parent\n";
          if(!isQul()) {
              out += intendent + "OpacityMask {\n";
              out += intendent1 + "anchors.fill:parent\n";
              out += intendent1 + "source: " + sourceId +  "\n";
              out += intendent1 + "maskSource: " + maskSourceId + "\n";
              out += intendent + "}\n\n";
          }
          out += intendent + "Item {\n";
          out += intendent1 + "id: " + maskSourceId + "\n";
          out += intendent1 + "anchors.fill:parent\n";
          APPENDERR(out, parse(child, intendents + 2));
          out += intendent1 + "visible:false\n";
          out += intendent + "}\n\n";
          out += intendent + "Item {\n";
          out += intendent1 + "id: " + sourceId + "\n";
          out += intendent1 + "anchors.fill:parent\n";
          out += intendent1 + "visible:false\n";
          return out;
      }

    std::optional<OrderedMap<QString, QByteArray>> FigmaParser::parseChildrenItems(const QJsonObject& obj, int intendents) {
        OrderedMap<QString, QByteArray> childrenItems;
        m_parent.push(&obj);
        if(obj.contains("children")) {
            bool hasMask = false;
            QByteArray out;
            auto children = obj["children"].toArray();
            for(const auto& c : children) {
                //m_parent = {&obj, m_parent};
                auto child = c.toObject();
                const bool isMask = child.contains("isMask") && child["isMask"].toBool(); //mask may not be the first, but it masks the rest
                if(isMask) {
                    APPENDERR(out, makeChildMask(child, intendents));
                    hasMask = true;
                } else {
                    const auto parsed = parse(child, hasMask ? intendents + 2 : intendents + 1);
                    if(!parsed)
                        return std::nullopt;
                    childrenItems.insert(child["id"].toString(), *parsed);
                }
            }
            if(hasMask) {
                for(const auto& [k, bytes]: childrenItems)
                    out += bytes;
                out += tabs(intendents + 1) + "}\n";
                out += tabs(intendents) + "}\n";
                childrenItems.clear();
                childrenItems.insert("maskedItem", out);
            }
        }
        //m_parent = parent;
        m_parent.pop();
        return childrenItems;
    }

    QString FigmaParser::lastError() {
        return last_parse_error;
    }


    QByteArray FigmaParser::addComponentStream(const QJsonObject& obj,  const QByteArray& child_item) {
        QByteArray filename;
        for(;;) {
            const auto name = obj["name"].toString() + '_' + obj["id"].toString() + "_" + QString::number(m_counter) + FIGMA_SUFFIX;
            Q_ASSERT(!name.isEmpty());
            filename = makeFileName("component_" + name).toLatin1();
            const auto it = m_componetStreams.find(filename);
            if(it == m_componetStreams.end())
                break;  // not found
            const auto& current_data = std::get<QByteArray>(*it);
            const auto& current_obj = std::get<QJsonObject>(*it);
            if(current_data.length() == child_item.length() && obj == current_obj && qChecksum(child_item) == qChecksum(current_data)) {
                break; // equal
            }
            ++m_counter;
        };
        m_componetStreams.insert(filename, std::make_tuple(obj, child_item)); // what to do for std::move ?
        return filename + ".qml";
    }
