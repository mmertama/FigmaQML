#include <QFileInfo>
#include "appwrite.h"
#include "execute_utils.h"

#ifndef HAS_QUL
#error HAS_QUL expected
#endif

bool AppWrite::writeQul(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements) {
    const auto res = ExecuteUtils::writeResources(path, figmaQml, writeAsApp, elements);
    if(!res)
        return false;

    const auto& [qml_item_names, qml_files, images] = res.value();

    Q_ASSERT(!qml_files.isEmpty());
    static const QRegularExpression re_project (R"((^\s*files:\s*\[\s*"FigmaQmlUi.qml")(\s*(?:@[A-Z_]+@)?\s*\]))"); // note QML_FILES ! that is for a configure
    constexpr auto JOIN = ",\n            "; //make a genereted qmlproject.in more pretty
    // note a colon
    VERIFY(ExecuteUtils::replaceInFile(path + "/FigmaQmlInterface/FigmaQmlInterface.qmlproject.in", re_project, R"(\1,)" + ExecuteUtils::qq(qml_files).join(JOIN) + R"(\2)", {"Project", "QmlFiles"}), "Cannot update qmlproject");

    QStringList local_images;
    std::transform(images.begin(), images.end(), std::back_inserter(local_images), [](const auto& f){return ExecuteUtils::qq("images/" + QFileInfo(f).fileName());});

    static const QRegularExpression re_images (R"((^\s*files:\s*\[\s*)(\s*(?:@[A-Z_]+@)?\s*\]))");

    if(!local_images.isEmpty())
        VERIFY(ExecuteUtils::replaceInFile(path + "/FigmaQmlInterface/FigmaQmlInterface.qmlproject.in", re_images, R"(\1)" + local_images.join(JOIN) + R"(\2)", {"Project", "ImageFiles"}), "Cannot update qmlproject");

    static const QRegularExpression re_qml(R"(\/\*element_declarations\*\/)");
    VERIFY(ExecuteUtils::replaceInFile(path + "/FigmaQmlInterface/FigmaQmlInterface.hpp", re_qml, ExecuteUtils::qq(qml_item_names).join(","), {}, true), "Cannot update cpp file");

    return true;
}
