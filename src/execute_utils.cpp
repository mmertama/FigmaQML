#include "execute_utils.h"
#include "appwrite.h"
#include <QVariantMap>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QEventLoop>
#include <QDirIterator>
#include <QSet>
#include "figmaqml.h"


[[maybe_unused]]
static bool has_duplicates(const QStringList& lst) {
    return lst.size() > QSet(lst.begin(), lst.end()).size();
}


void ExecuteUtils::showError(const QString& errorNote, const QString& infoNote) {
    QMessageBox msgBox;
    msgBox.setText(errorNote);
    if(!infoNote.isEmpty())
        msgBox.setInformativeText(infoNote);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

ExecuteInfo* ExecuteInfo::instance(const FigmaQml& figmaQml) {
    static ExecuteInfo* info;
    if(!info) {
        info = new ExecuteInfo(figmaQml);
    }
    return info;
}


// clean off ANSI
QString ExecuteUtils::clean(const QByteArray& strm) {
    auto str = QString::fromLatin1(strm);
    // . as \u001B is a single character!
    static const QRegularExpression re(R"(\r|(.\[[0-9;]+m))");
    str.remove(re);
    return str;
}


ExecuteInfo::ExecuteInfo(const FigmaQml& figmaQml, QObject* parent) : QObject(parent) {
    QObject::connect(this, &ExecuteInfo::information, &figmaQml, &FigmaQml::qulInfo);
}

ExecuteInfo::~ExecuteInfo() {}

bool ExecuteUtils::copy_resources(const QString& path, const QString& resource_root, const QStringList& folders_only) {
    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        const auto res_path = "/" + resource_root +  "/";
        if(info.isFile() && info.filePath().mid(1, res_path.length()) == res_path &&
            (folders_only.isEmpty() || std::any_of(folders_only.begin(), folders_only.end(), [&](const auto& f){return info.path().split('/').contains(f);}))) {

            [[maybe_unused]] const auto prefix = ":/" + resource_root;
            Q_ASSERT(info.filePath().startsWith(prefix)); // see below

            const auto target_path = path + info.filePath().mid(prefix.length()); // see above
            const auto abs_path = QFileInfo(target_path).absolutePath();
            VERIFY(QDir().mkpath(abs_path), "Cannot create folder " + abs_path)
            qDebug() << target_path;

            if(QFile::exists(target_path))
                QFile::remove(target_path);

            VERIFY(QFile::copy(info.filePath(), target_path), "Cannot create " + target_path)
            const auto permissions = QFile::permissions(target_path);
            QFile::setPermissions(target_path, permissions | QFileDevice::WriteOwner | QFileDevice::WriteGroup | QFileDevice::WriteOther);
        }
    }
    return true;
}


QString ExecuteUtils::qq(const QString& str) {
    return '"' + str + '"';
}

QStringList ExecuteUtils::qq(const QStringList& str) {
    QStringList quoted;
    std::transform(str.begin(), str.end(), std::back_inserter(quoted), [](const auto& s) {return qq(s);});
    return quoted;
}


std::optional<std::tuple<QStringList, QSet<QString>>> ExecuteUtils::writeElement(const QString& path, const QString& main_file_name, const FigmaQml& figmaQml, const std::pair<int, int>& indices) {

    QStringList qml_files;
    QSet<QString> save_image_filter;

    qml_files << main_file_name;
    const auto fullname = path + '/' + FOLDER + main_file_name;

    const auto bytes = figmaQml.sourceCode(indices.first, indices.second);
    VERIFO(!bytes.isEmpty(), "Cannot find data for " + main_file_name);

    figmaQml.testFileExists(fullname, bytes);
    QFile qml_out(fullname);
    VERIFO(qml_out.open(QFile::WriteOnly), "Cannot write a QtQuick file");

    qml_out.write(bytes);

    qml_out.close();

    for(const auto& component_name : figmaQml.components(indices.first, indices.second)) {
        save_image_filter.insert(component_name);
        const auto file_name = QML_PREFIX + FigmaQml::validFileName(component_name) + QML_EXT;
        const auto component_src = figmaQml.componentSourceCode(component_name);
        Q_ASSERT(!component_src.isEmpty());
        figmaQml.testFileExists(file_name, component_src);
        qml_files << file_name;

        const auto target_path = path + '/' + FOLDER + file_name;
        QFile component_out(target_path);
        if(QFile::exists(target_path)) {
            qDebug() << "File replaced" << target_path;
            QFile::remove(target_path);
        }
        VERIFO(component_out.open(QFile::WriteOnly), "Cannot write component " + file_name);

        component_out.write(component_src);
        component_out.close();
    }
    return std::make_tuple(qml_files, save_image_filter);
}

bool ExecuteUtils::replaceInFile(const QString& fname, const QRegularExpression& re, const QString& replacement, const QStringList context, bool force_context) {
    Q_ASSERT_X(re.isValid(), "regexp", re.errorString().toLocal8Bit().constData());
    QFile file(fname);
    if(!file.open(QFile::ReadOnly)) {  // I have no idea (not looked very much) why readwrite wont work in my system, hence read and then write
        qDebug() << "cannot open" << fname << "due" << file.errorString();
        return false;
    }

    bool wait_context = false;
    bool in_context = force_context;
    bool next_context = false;
    bool skip = false;
    QStack<QString> current_context;
    unsigned replacements = 0;
    QStringList lines;
    QTextStream read(&file);
    while(!read.atEnd()) {
        auto line = read.readLine();

        if(current_context.isEmpty()) {
            static const QRegularExpression import_re(R"(^\s*import\s+)");
            const auto m = import_re.match(line);
            if(m.hasMatch()) skip = true;
        }

        if(!skip) {
            static const QRegularExpression token_re(R"(^\s*([A-Za-z._]+)\s*(\{|:)?)");
            const auto token_match = token_re.match(line);
            if(token_match.hasMatch()) {
                if(token_match.captured(2).isEmpty()) {
                    current_context.push(token_match.captured(1));
                    wait_context = true;
                } else if (token_match.captured(2) == "{") { // 'property :' is not a context
                    current_context.push(token_match.captured(1));
                    next_context = true;
                }
            }


            if(wait_context) {
                static const QRegularExpression open_re(R"(^\s*\{)");
                const auto m = open_re.match(line);
                if(m.hasMatch()) {
                    next_context = true;
                    wait_context = false;
                }
            }

            if(next_context) {
                next_context = false;
                in_context = current_context.size() == context.size();
                for(auto i = 0; in_context && i < context.size(); ++i)
                    in_context &= current_context[i] == context[i];

            }


            static const QRegularExpression open_re(R"(^\s*\})");
            const auto m = open_re.match(line);
            if(m.hasMatch()) {
                current_context.pop();
            }
        }
        skip = false;
        const auto old_line = line;
        lines << (in_context ? line.replace(re, replacement) : line);
        if(old_line != line)
            ++replacements;
    }

    VERIFY(current_context.isEmpty() && !wait_context, "Invalid file: " + fname);

    file.close();
    if(!file.remove())
        return false;
    if(!file.open(QFile::WriteOnly)) {
        qDebug() << "cannot open" << fname << "due" << file.errorString();
        return false;
    }
    QTextStream write(&file);
    for(const auto& l : lines)
        write << l << "\n";
    Q_ASSERT_X(replacements > 0, "replaceInFile", ("Nothing changed in " + fname).toLatin1().data());
    return true;
}


QString ExecuteUtils::findFile(const QString& path, const QRegularExpression& regexp, Kind kind) {
    QDir dir(path, {}, QDir::Type, QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    if(dir.exists()) {
        for(const auto& entry : dir.entryInfoList()) {
            if(entry.isFile()) {
                const auto match = regexp.match(entry.fileName());
                if(match.hasMatch() && (kind == Kind::Any || (kind == Kind::Exe && entry.isExecutable())))
                    return entry.absoluteFilePath();
            } else if(entry.isDir()) {
                const auto sub = findFile(entry.absoluteFilePath(), regexp, kind);
                if(!sub.isEmpty())
                    return sub;
            }
        }
    }
    return {};
}

std::optional<std::tuple<QStringList, QStringList, QStringList>> ExecuteUtils::writeResources(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements) {
    // I decided to remove all as if not done it causes problems
    if(QDir(path).exists()) {
        VERIFO(QDir(path).removeRecursively(), "Cannot clean folder");
    }

    const auto abs_path = QFileInfo(path).absolutePath();
    VERIFO(QDir().mkpath(path + '/' + FOLDER + QML_PREFIX), "Cannot create folder " + abs_path + FOLDER + QML_PREFIX)

    if(!ExecuteUtils::copy_resources(path, "app", writeAsApp ? QStringList{} : QStringList{"FigmaQmlInterface"}))
        return std::nullopt;

    // these are "views"
    QStringList qml_view_names;
    // these are images needed
    QSet<QString> save_image_filter {{figmaQml.elementName()}};
    // these are all qml files needed
    QSet<QString> qml_files;


    const auto file_name =  QML_PREFIX + FigmaQml::validFileName(figmaQml.elementName()) + QML_EXT;
    //qml_files.append(file_name);
    qml_view_names.append(file_name);

    const auto mwo = ExecuteUtils::writeElement(path, file_name, figmaQml, {figmaQml.currentCanvas(), figmaQml.currentElement()});
    if(!mwo)
        return std::nullopt; // it already tells what went wrong
    const auto& [mcomponents, mfilter] = mwo.value();
    save_image_filter.unite(mfilter);
    std::copy(mcomponents.begin(), mcomponents.end(), std::inserter(qml_files, qml_files.begin()));

    const auto els = figmaQml.elements();
    for(const auto& var : elements) {
        const auto& map = els[var].toMap();
        const auto element_name = map["element_name"].toString();
        const auto canvas_index = map["canvas"].toInt();
        const auto element_index = map["element"].toInt();
        const auto file_name =  QML_PREFIX + FigmaQml::validFileName(element_name) + QML_EXT;
        const auto wo = ExecuteUtils::writeElement(path, file_name, figmaQml, {canvas_index, element_index});
        if(!wo)
            return std::nullopt; // it already tells what went wrong
        const auto& [components, filter] = wo.value();
        std::copy(components.begin(), components.end(), std::inserter(qml_files, qml_files.begin()));
        qml_view_names.append(file_name);
        save_image_filter.unite(filter);
    }

    const auto images = figmaQml.saveImages(path + '/' + FOLDER + IMAGE_PREFIX, save_image_filter);
    VERIFO(images, "Cannot save images");

    Q_ASSERT(!has_duplicates(qml_view_names));
    Q_ASSERT(!has_duplicates(QStringList(qml_files.begin(), qml_files.end())));
    Q_ASSERT(!has_duplicates(*images));
    return std::make_tuple(qml_view_names, QStringList(qml_files.begin(), qml_files.end()), *images);
}
