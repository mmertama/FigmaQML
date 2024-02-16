#include <QVariantMap>
#include <QTemporaryDir>
#include <QProcess>
#include <QMessageBox>
#include <QEventLoop>
#include <QDirIterator>
#include <QXmlStreamReader>
#include <QSet>
#include "figmaqml.h"
#include "execute_utils.h"



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

static void debug_out (QProcess& process) {
    qDebug().noquote() << GREEN << ExecuteUtils::clean(process.readAllStandardOutput()) << NONE;
    if(process.exitCode() == 0)
        qDebug().noquote() << YELLOW << ExecuteUtils::clean(process.readAllStandardError()) << NONE;
    else
        qDebug().noquote() << RED << ExecuteUtils::clean(process.readAllStandardError()) << NONE;
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

void ExecuteUtils::waitRun(QProcess& process) {
    QEventLoop innerLoop;
    QObject::connect(&process, &QProcess::finished,  &innerLoop, &QEventLoop::quit);
    innerLoop.exec();
    debug_out(process);
}


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
    Q_ASSERT(dir.exists());
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
    return {};
}

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

bool writeApp(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements) {
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

static bool runApp(QProcess& run_process, const QTemporaryDir& dir) {
    const auto app = ExecuteUtils::findFile(dir.path(), QRegularExpression(R"(app_figma)"), ExecuteUtils::Exe);
    VERIFY(!app.isEmpty(), "Cannot find application");
    run_process.setWorkingDirectory(dir.path());
    run_process.start(app);
    return true;
}

bool executeApp(const QVariantMap& parameters, const FigmaQml& figmaQml, const std::vector<int>& elements) {

    // create a monitor
    auto eInfo = ExecuteInfo::instance(figmaQml);

    QTemporaryDir dir;
    VERIFY(dir.isValid(), "Cannot create temp dir")
    if(!writeApp(dir.path(), figmaQml, true, elements))
        return false;

    QProcess build_process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const auto qt_dir = parameters["qtDir"].toString();
    env.insert("QT_DIR", qt_dir);
    const auto builder = ExecuteUtils::findFile(qt_dir, QRegularExpression(R"(qt-cmake.*)"), ExecuteUtils::Exe);
    VERIFY(!builder.isEmpty(), "Cannot find qt-cmake");
    build_process.setProcessEnvironment(env);
    build_process.setWorkingDirectory(dir.path());
    build_process.start("bash", {"build.sh", builder});

    QObject::connect(&build_process, &QProcess::readyReadStandardError, eInfo, [eInfo, &build_process]() {
        const auto out = ExecuteUtils::clean(build_process.readAllStandardError());
        qDebug() << (build_process.exitCode() == 0 ? YELLOW : RED) << out << NONE;
        emit eInfo->information(out, build_process.exitCode() == 0 ? 1 : 0);
    });

    QObject::connect(&build_process, &QProcess::readyReadStandardOutput, eInfo, [eInfo, &build_process]() {
        const auto out = ExecuteUtils::clean(build_process.readAllStandardOutput());
        qDebug() << (build_process.exitCode() == 0 ? GREEN : RED) << out << NONE;
        emit eInfo->information(out, build_process.exitCode() == 0 ? 2 : 0);
    });

    ExecuteUtils::waitRun(build_process);

    if(build_process.exitCode() != 0) {
         ExecuteUtils::showError("Build of test application failed!");
        return false;
    }

    QProcess execute_process;
    QObject::connect(&execute_process, &QProcess::readyReadStandardError, eInfo, [eInfo, &execute_process]() {
        const auto out = ExecuteUtils::clean(execute_process.readAllStandardError());
        qDebug() << (execute_process.exitCode() == 0 ? YELLOW : RED) << out << NONE;
        emit eInfo->information(out, execute_process.exitCode() == 0 ? 1 : 0);
    });

    QObject::connect(&execute_process, &QProcess::readyReadStandardOutput, eInfo, [eInfo, &execute_process]() {
        const auto out = ExecuteUtils::clean(execute_process.readAllStandardOutput());
        qDebug() << GREEN << out << NONE;
        emit eInfo->information(out, 2);
    });

    if(!runApp(execute_process, dir)) {
        qDebug() << "Failed in" << dir.path();
        ExecuteUtils::showError("Execution of test application failed!");
        return false;
    }
    ExecuteUtils::waitRun(execute_process);

    return true;
}
