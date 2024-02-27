#include "execute_utils.h"
#include "appwrite.h"
#include <QXmlStreamReader>
#include <QFile>
#include <QFileInfo>


[[maybe_unused]]
static bool addInXml(const QString& filename, const QStringList& values, const QString& element, const QStringList& context) {
    QFile file(filename);
    VERIFY(file.open(QFile::ReadWrite), "Cannot read " + filename);
    qint64 offset = -1;
    auto cnt = context.begin();
    QXmlStreamReader xml_reader(&file);
    while(!xml_reader.atEnd() && offset < 0) {
        if(cnt == context.end()) {
            offset = xml_reader.characterOffset();
        } else {
            if(!xml_reader.readNextStartElement()) // context are elements
                return false;
            const auto token = xml_reader.name();
            if(*cnt == token) {
                ++cnt;
            }
        }
    }
    if(offset < 0)
        return false;
    VERIFY(file.seek(offset), "Cannot access file position");
    const auto remainder = file.readAll();
    VERIFY(file.resize(offset), "Cannot truncate"); //truncate here
    VERIFY(file.seek(offset), "Cannot access file position");
    for(const auto& value: values) {
        file.write("\n");
        file.write(QString(values.size() * 4, ' ').toLocal8Bit()); // 4 is a tab in spaces
        file.write(('<' + element + '>').toLocal8Bit());
        file.write(value.toLocal8Bit());
        file.write(("</" + element + '>').toLocal8Bit());
    }
    file.write(remainder);
    return true;
}

QStringList AppWrite::supportedQulHardware() {
    return {STM32}; // only supported, TODO more
}



bool AppWrite::writeApp(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const QVector<int>& elements) {
    const auto res = ExecuteUtils::writeResources(path, figmaQml, writeAsApp, elements);
    if(!res)
        return false;

    const auto& [qml_view_names, qml_files, images] = res.value();
    Q_ASSERT(!qml_files.isEmpty());
    static QRegularExpression app_qml_files(R"((set\(APP_QML_FILES)(\)))");
    VERIFY(ExecuteUtils::replaceInFile(path + "/FigmaQmlInterface/CMakeLists.txt", app_qml_files, R"(\1 )" + ExecuteUtils::qq(qml_files).join("\n   ") + R"(\2)", {}, true), "Cannot update resources files");


    QStringList local_images;
    std::transform(images.begin(), images.end(), std::back_inserter(local_images), [](const auto& f){return ExecuteUtils::qq("images/" + QFileInfo(f).fileName());});

    if(!local_images.isEmpty()) {
        static QRegularExpression app_image_files(R"((set\(APP_IMAGE_FILES)(\)))");
        VERIFY(ExecuteUtils::replaceInFile(path + "/FigmaQmlInterface/CMakeLists.txt", app_image_files, R"(\1 )" + local_images.join("\n    ") + R"(\2)", {}, true), "Cannot update resources files");
    }

    static const QRegularExpression re_qml(R"(\/\*element_declarations\*\/)");
    VERIFY(ExecuteUtils::replaceInFile(path + "/FigmaQmlInterface/FigmaQmlInterface.hpp", re_qml, ExecuteUtils::qq(qml_view_names).join(","), {}, true), "Cannot update cpp file");

    return true;
}
