#include "figmaget.h"
#include "figmaqml.h"
#include "clipboard.h"
#include "downloads.h"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSslSocket>
#include <QMessageBox>
#include <QSurfaceFormat>
#include <QSettings>
#include <QTemporaryDir>
#include <QCommandLineParser>
#include <QTextStream>
#include <QRegularExpression>

//-DCMAKE_CXX_FLAGS="-DNON_CONCURRENT=1"
QT_REQUIRE_CONFIG(ssl);

#define STRINGIFY0(x) #x
#define STRINGIFY(x) STRINGIFY0(x)

constexpr char PROJECT_TOKEN[]{"project_token"};
constexpr char USER_TOKEN[]{"user_token"};
constexpr char FLAGS[]{"flags"};
constexpr char EMBED_IMAGES[]{"embed_images"};
constexpr char IMAGEMAXSIZE[]{"image_max_size"};
constexpr char IMPORTS[]{"imports"};
constexpr char COMPANY_NAME[]{"Moonshine shade of haste productions"};
constexpr char PRODUCT_NAME[]{"FigmaQML"};


QTextStream& print() {
    static QTextStream r{stdout};
    return r;
}

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    const auto versionNumber = QString(STRINGIFY(VERSION_NUMBER));

    QCommandLineParser parser;
    parser.setApplicationDescription(QString("Figma to QML generator, version: %1, Markus Mertama 2020").arg(versionNumber));
    parser.addHelpOption();
    const QCommandLineOption renderFrameParameter("render-frame", "Render frames as images");
    const QCommandLineOption imageDimensionMaxParameter("image-dimension-max", "Capping image size to a square, default 1024", "imageDimensionMax");
    const QCommandLineOption embedImagesParameter("embed Images", "Embed images into QML files");
    const QCommandLineOption breakBooleansParameter("break-boolean", "Break Figma booleans to components");
    const QCommandLineOption antializeShapesParameter("antialize-shapes", "Add antialiaze property to shapes");
    const QCommandLineOption importsParameter("imports", "QML imports, ';' separated list of imported modules as <module-name> <version-number>", "imports");
    const QCommandLineOption snapParameter("snap", "Take snapshot and exit, expects restore or project parameter be given", "snapFile");
    const QCommandLineOption storeParameter("store", "Create .figmaqml file and exit, expects project parameter be given");
    const QCommandLineOption timedParameter("timed", "Time parsing process");
    const QCommandLineOption showParameter("show", "Set current page and view to <page index>-<view index>, indexing starts from 1", "show");

    parser.addPositionalArgument("argument 1", "Optional", "<FIGMA_QML_FILE>|<USER_TOKEN>");
    parser.addPositionalArgument("argument 2", "Optional", "<OUTPUT if FIGMA_QML_FILE>| PROJECT_TOKEN if USER_TOKEN");
    parser.addPositionalArgument("argument 3", "Optional", "<OUTPUT if USER_TOKEN>");


    parser.addOptions({
                          renderFrameParameter,
                          imageDimensionMaxParameter,
                          breakBooleansParameter,
                          antializeShapesParameter,
                          embedImagesParameter,
                          importsParameter,
                          snapParameter,
                          storeParameter,
                          timedParameter,
                          showParameter
                      });
    parser.process(app);

    enum {
        CmdLine = 1,
        Store = 2
    };

    int state = 0;

    QString restore;
    QString userToken;

    if(parser.positionalArguments().size() > 0) {
        const auto name = parser.positionalArguments().at(0);
        if(name.endsWith(".figmaqml") || QFile::exists(name)) {
          if(!QFile::exists(name)) {
              ::print() << "Error:" << name << " not found" << Qt::endl;
              return -9;
          };
          restore = name;
        } else
            userToken = name;
    }

    QString projectToken;
    QString output;
    if(parser.positionalArguments().size() > 1) {
        if(!restore.isEmpty())
            output = parser.positionalArguments().at(1);
        else
            projectToken = parser.positionalArguments().at(1);
    }

    if(!userToken.isEmpty() && projectToken.isEmpty()) {
        parser.showHelp(-10);
    }

    if(!userToken.isEmpty() && parser.positionalArguments().size() > 2) {
        output = parser.positionalArguments().at(2);
    }

    const QString snapFile = parser.value(snapParameter);

    if(!output.isEmpty())
        state |= CmdLine;

    if(parser.isSet(storeParameter))
        state |= Store;

    if(!snapFile.isEmpty() && !(userToken.isEmpty() || restore.isEmpty())) {
        parser.showHelp(-2);
    }

    int canvas = 1;
    int element = 1;

    if(parser.isSet(showParameter)) {
        const auto ce = parser.value(showParameter).split('-');
        if(ce.length() != 2) {
            parser.showHelp(-11);
        }
        bool ok;
        canvas = ce[0].toInt(&ok);
        if(!ok) parser.showHelp(-12);
        element = ce[1].toInt(&ok);
        if(!ok) parser.showHelp(-13);
    }

    /*
    if(!gui) {
        if(parser.positionalArguments().size() != 3 || parser.positionalArguments().size() != 2) {
            parser.showHelp();
            return -1;
        }
    }*/

    if (!QSslSocket::supportsSsl()) {
        QMessageBox::warning(nullptr,
                             "Secure Socket Client",
                             "Cannot load SSL/TLS. Please make sure that DLLS are available.");
        return -1;
    }


    QSurfaceFormat format;
    format.setSamples(8);
    QSurfaceFormat::setDefaultFormat(format);

    QTemporaryDir dir;

    std::unique_ptr<FigmaGet> figmaGet(new FigmaGet(dir.path() + "/images/"));
    std::unique_ptr<FigmaQml> figmaQml(new FigmaQml(dir.path(),
                                           [&figmaGet](const QString& id, bool isRendering, const QSize& maxSize) {
        return isRendering  ? figmaGet->getRendering(id) : figmaGet->getImage(id, maxSize);
    },
    [&figmaGet](const QString& key) {
        return figmaGet->getNode(key);
    }));


    QQmlApplicationEngine engine;
    Clipboard clipboard;
   // figmaQml->setFilter({{1,{2}}});

    figmaQml->setBrokenPlaceholder(":/broken_image.jpg");

    if(state & CmdLine || !snapFile.isEmpty()) {
        unsigned qmlFlags = 0;

        if(restore.isEmpty()) {
         if(parser.isSet(renderFrameParameter))
             qmlFlags |= FigmaQml::PrerenderFrames;
         if(parser.isSet(breakBooleansParameter))
             qmlFlags |= FigmaQml::BreakBooleans;
         if(parser.isSet(antializeShapesParameter))
             qmlFlags |= FigmaQml::AntializeShapes;
         if(parser.isSet(embedImagesParameter))
             qmlFlags |= FigmaQml::EmbedImages;

         if(parser.isSet(importsParameter)) {
             QMap<QString, QVariant> imports;
             const auto p = parser.value(importsParameter).split(';');
             for(const auto& i : p) {
                 const auto importLine = i.split(' ');
                 imports.insert(importLine[0], importLine[1]);
             }
             figmaQml->setProperty("imports", imports);
         }
        }

        if(parser.isSet(timedParameter))
            qmlFlags |= FigmaQml::Timed;

         if(!userToken.isEmpty())
             figmaGet->setProperty("userToken", userToken);
         if(!projectToken.isEmpty())
             figmaGet->setProperty("projectToken", projectToken);

         figmaQml->setProperty("flags", qmlFlags);

         if(parser.isSet(imageDimensionMaxParameter))
            figmaQml->setProperty("imageDimensionMax", parser.value(imageDimensionMaxParameter));
     }

     if(state & CmdLine) {
         auto start = QTime::currentTime();
         QObject::connect(figmaGet->downloadProgress(), &Downloads::bytesReceivedChanged, [&start]() {
             const auto now = QTime::currentTime();
             if(start.secsTo(now) > 1) {
                ::print() << "." << Qt::flush;
                start = now;
             }
         });
         QObject::connect(figmaQml.get(), &FigmaQml::warning, [](const QString& infoString){
            ::print() << "\nInfo: " << infoString << Qt::endl;
         });
         QObject::connect(figmaQml.get(), &FigmaQml::warning, [](const QString& warningString){
            ::print() << "\nWarning: " << warningString << Qt::endl;
         });
         QObject::connect(figmaQml.get(), &FigmaQml::error, [&app](const QString& errorString){
            ::print() << "\nError: " << errorString << Qt::endl;
            QTimer::singleShot(0, [&app](){app.exit(-2);});
         });
         QObject::connect(figmaGet.get(), &FigmaGet::error, [&app](const QString& errorString){
            ::print() << "\nError: " << errorString << Qt::endl;
            QTimer::singleShot(0, [&app](){app.exit(-3);});
         });

         QObject::connect(figmaQml.get(), &FigmaQml::sourceCodeChanged, [&figmaQml, &figmaGet, output, &app, state]() {
             int excode = 0;
             if(state & Store) {
                 const auto saveName = output.endsWith(".figmaqml") ? output : output + ".figmaqml";
                 if(figmaGet->store(saveName, figmaQml->property(FLAGS).toUInt(), figmaQml->property(IMPORTS).value<QVariantMap>())) {
                     ::print() << "\nStored to " << saveName << Qt::endl;
                 } else {
                     ::print() << "\nStore to " << saveName << " failed" << Qt::endl;
                      excode = -1;
                     }
             } else if(!output.isEmpty()) {
                if(figmaQml->saveAllQML(output)) {
                    ::print() << "\nSaved to " << output << Qt::endl;
                } else {
                    ::print() << "\nSave to " << output << " failed" << Qt::endl;
                    excode = -1;
                }
             }
             QTimer::singleShot(0, [&app, excode](){app.exit(excode);});
         });

         if(!restore.isEmpty()) {
             QObject::connect(figmaGet.get(), &FigmaGet::restored,
                              figmaQml.get(), [&figmaGet, &figmaQml](unsigned flags, const QVariantMap& imports) {

                 figmaQml->restore(flags, imports);
                 figmaQml->createDocumentSources(figmaGet->data());
             });
         } else {
             QObject::connect(figmaGet.get(), &FigmaGet::dataChanged,
                              figmaQml.get(), [&figmaGet, &figmaQml]() {
                 figmaQml->createDocumentSources(figmaGet->data());
             });
             figmaGet->update();
        }
     }

     if(!(state & CmdLine) && snapFile.isEmpty()) {
         QObject::connect(figmaQml.get(), &FigmaQml::flagsChanged,
                          figmaQml.get(), [&figmaGet, &figmaQml]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(FLAGS, figmaQml->property("flags").toUInt());
             figmaQml->createDocumentView(figmaGet->data());
         });

         QObject::connect(figmaQml.get(), &FigmaQml::imageDimensionMaxChanged,
                          figmaQml.get(), [&figmaGet, &figmaQml]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(IMAGEMAXSIZE, figmaQml->property("imageDimensionMax").toInt());
             figmaQml->createDocumentView(figmaGet->data());
         });

         QObject::connect(figmaGet.get(), &FigmaGet::projectTokenChanged, [&figmaGet](){
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(PROJECT_TOKEN, figmaGet->property("projectToken").toString());
         });

         QObject::connect(figmaGet.get(), &FigmaGet::userTokenChanged, [&figmaGet](){
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(USER_TOKEN, figmaGet->property("userToken").toString());
         });

         QObject::connect(figmaQml.get(), &FigmaQml::importsChanged, [&figmaQml, &figmaGet]() {
             QSettings settings(COMPANY_NAME, PRODUCT_NAME);
             settings.setValue(IMPORTS, figmaQml->property("imports").toMap());
             figmaQml->createDocumentView(figmaGet->data());
         });

         QObject::connect(figmaGet.get(), &FigmaGet::restored,
                          figmaQml.get(), [&figmaGet, &figmaQml](unsigned flags, const QVariantMap& imports) {
             figmaQml->restore(flags, imports);
             figmaQml->createDocumentView(figmaGet->data());
         });

         QSettings settings(COMPANY_NAME, PRODUCT_NAME);

         figmaGet->setProperty("projectToken", settings.value(PROJECT_TOKEN).toString());
         figmaGet->setProperty("userToken", settings.value(USER_TOKEN).toString());
         figmaQml->setProperty("flags", settings.value(FLAGS, 0).toUInt());
         figmaQml->setProperty("imports", settings.value(IMPORTS, FigmaQml::defaultImports()).toMap());

         figmaGet->setProperty("embedImages", settings.value(EMBED_IMAGES).toString());
         figmaQml->setProperty("imageDimensionMax", settings.value(IMAGEMAXSIZE, 1024).toInt());

     }

     if(!snapFile.isEmpty()) {
         QObject::connect(figmaQml.get(), &FigmaQml::snapped, [&app]() {
              QTimer::singleShot(0, [&app](){app.quit();});
         });

         QObject::connect(figmaQml.get(), &FigmaQml::componentLoaded, [&figmaQml, &canvas, &element, &snapFile](int currentCanvas, int currentElement) {
             if(currentCanvas == canvas - 1 && currentElement == element - 1) {
                 emit figmaQml->takeSnap(snapFile, canvas - 1, element - 1);
             }
         });

         if(!restore.isEmpty()) {
             QObject::connect(figmaGet.get(), &FigmaGet::restored,
                              figmaQml.get(), [&figmaGet, &figmaQml](unsigned flags, const QVariantMap& imports) {
                  figmaQml->restore(flags, imports);
                  figmaQml->createDocumentView(figmaGet->data());
             });
         } else {
             QObject::connect(figmaGet.get(), &FigmaGet::dataChanged,
                              figmaQml.get(), [&figmaGet, &figmaQml]() {
                 figmaQml->createDocumentView(figmaGet->data());
             });
             figmaGet->update();
         }
     }

     if(!(state & CmdLine)) {

         QObject::connect(figmaGet.get(), &FigmaGet::dataChanged,
                          figmaQml.get(), [&figmaGet, &figmaQml]() {
             figmaQml->createDocumentView(figmaGet->data());
         });

        if(parser.isSet(showParameter)) {
                auto connection = std::make_shared<QMetaObject::Connection>();
                *connection = QObject::connect(figmaQml.get(), &FigmaQml::documentCreated, [&figmaQml, &canvas, &element, &app, connection] () {
                    if(figmaQml->isValid()) {
                         if(!figmaQml->setCurrentCanvas(canvas - 1)) {
                             ::print() << "Error: Invalid page " << canvas << " of " << figmaQml->canvasCount() << Qt::endl;
                             QTimer::singleShot(0, [&app](){app.exit(-2);}); //ensure a correct thread
                             return;
                         }

                         if(!figmaQml->setCurrentElement(element - 1))  {
                             ::print() << "Error: Invalid view " << element << " of " << figmaQml->elementCount() << Qt::endl;
                             QTimer::singleShot(0, [&app](){app.exit(-2);});
                             return;
                         }
                    }
                    QObject::disconnect(*connection);
                });
        }

         engine.rootContext()->setContextProperty("clipboard", &clipboard);
         engine.rootContext()->setContextProperty("figmaGet", figmaGet.get());
         engine.rootContext()->setContextProperty("figmaQml", figmaQml.get());
         engine.rootContext()->setContextProperty("figmaDownload", figmaGet->downloadProgress());
         engine.rootContext()->setContextProperty("figmaQmlVersionNumber", versionNumber);

         engine.load(QUrl("qrc:/main.qml"));
     }




     if(!restore.isEmpty()) {
         figmaGet->restore(restore);
     }

    return app.exec();
}


