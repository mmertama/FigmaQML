#include <QVariant>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "qmetaobject.h"
#include "qulInfo.h"
#include "figmaqml.h"
#include "utils.h"

#define VERIFY(x, t) if(!x) {showError(t); return false;}
constexpr auto FOLDER = "FigmaQmlInterface/";
constexpr auto QML_PREFIX = "qml/";
constexpr auto IMAGE_PREFIX =  "images/";
constexpr auto QML_EXT = ".qml";

void showError(const QString& errorNote, const QString& infoNote = "") {
    QMessageBox msgBox;
    msgBox.setText(errorNote);
    if(!infoNote.isEmpty())
        msgBox.setInformativeText(infoNote);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

constexpr auto RED = "\033[31m";
constexpr auto NONE = "\033[0m";
constexpr auto GREEN = "\033[32m";
constexpr auto YELLOW = "\033[43m";

// clean off ANSI
static
QString clean(const QByteArray& strm) {
    auto str = QString::fromLatin1(strm);
    // . as \u001B is a single character!
    static const QRegularExpression re(R"(\r|(.\[[0-9;]+m))");
    str.remove(re);
    return str;
}

static
void debug_out (QProcess& process) {
    qDebug().noquote() << GREEN << clean(process.readAllStandardOutput()) << NONE;
    if(process.exitCode() == 0)
        qDebug().noquote() << YELLOW << clean(process.readAllStandardError()) << NONE;
    else
        qDebug().noquote() << RED << clean(process.readAllStandardError()) << NONE;
};

class QulInfo::Private {
public:
    Private(QulInfo& info) : m_info(info) {}
    bool connect(const QSerialPortInfo& port_info) {
        m_port = std::make_unique<QSerialPort>(port_info);
        QObject::connect(m_port.get(), &QSerialPort::readyRead, &m_info, [this]() {
            const auto out = QString::fromLatin1(m_port->readAll());
            emit m_info.information(out, 3);
            });

        QObject::connect(m_port.get(), &QSerialPort::errorOccurred, &m_info, [this](auto error) {
            //const auto metaEnum = QMetaEnum::fromType<QSerialPort::SerialPortError>();
            qDebug() << "Serial error:" <<  enumToString(error) << " " << m_port->errorString();
        });

        m_port->setBaudRate(QSerialPort::Baud115200);
        m_port->setDataBits(QSerialPort::Data8);
        m_port->setParity(QSerialPort::NoParity);
        m_port->setStopBits(QSerialPort::OneStop);
        m_port->setFlowControl(QSerialPort::NoFlowControl);

        return m_port->open(QSerialPort::ReadOnly);
    }
private:
    QulInfo& m_info;
    std::unique_ptr<QSerialPort> m_port;
};



QulInfo* QulInfo::instance(const FigmaQml& figmaQml) {
    static QulInfo* info;
    if(!info) {
        info = new QulInfo();
        QObject::connect(info, &QulInfo::information, &figmaQml, &FigmaQml::qulInfo);
    }
    return info;
}



QulInfo::QulInfo(QObject* parent) : QObject(parent),  m_private(std::make_unique<QulInfo::Private>(*this)) {}

QulInfo::~QulInfo() {}

bool QulInfo::connect(const QSerialPortInfo& info) {
    return m_private->connect(info);
}


// very simple search:  just return the 1st match
QString findFile(const QString& path, const QRegularExpression& regexp) {
    QDir dir(path, {}, QDir::Type, QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
    Q_ASSERT(dir.exists());
    for(const auto& entry : dir.entryInfoList()) {
        if(entry.isFile()) {
            const auto match = regexp.match(entry.fileName());
            if(match.hasMatch())
                return entry.absoluteFilePath();
        } else if(entry.isDir()) {
            const auto sub = findFile(entry.absoluteFilePath(), regexp);
            if(!sub.isEmpty())
                return sub;
        }
    }
    return {};
}

static
    bool copy_resources(const QString& path, const QStringList& folders_only = {}) {
    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        if(info.isFile() && info.filePath().mid(1, 5) == "/mcu/" &&
            (folders_only.isEmpty() || std::any_of(folders_only.begin(), folders_only.end(), [&](const auto& f){return info.path().split('/').contains(f);}))) {
            const auto target_path = path + info.filePath().mid(5);
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

static
QSerialPortInfo getSerialPort(const QString& id) {
    for (const QSerialPortInfo &portInfo : QSerialPortInfo::availablePorts()) {
        if(portInfo.description().contains(id)) {
            return portInfo;
        }
    }
    return QSerialPortInfo{};
}

static
    bool replaceInFile(const QString& fname, const QRegularExpression& re, const QString& replacement, const QStringList context) {
    QFile file(fname);
    if(!file.open(QFile::ReadOnly)) {  // I have no idea (not looked very much) why readwrite wont work in my system, hence read and then write
        qDebug() << "cannot open" << fname << "due" << file.errorString();
        return false;
    }

    bool wait_context = false;
    bool in_context = false;
    bool next_context = false;
    bool skip = false;
    QStack<QString> current_context;

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
        lines << (in_context ? line.replace(re, replacement) : line);
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
    return true;
}

static
QString qq(const QString& str) {
    return '"' + str + '"';
}

bool writeQul(const QString& path, const QVariantMap& parameters, const FigmaQml& figmaQml, bool writeAsApp) {

    const auto abs_path = QFileInfo(path).absolutePath();
    VERIFY(QDir().mkpath(path + '/' + FOLDER + QML_PREFIX), "Cannot create folder " + abs_path + FOLDER + QML_PREFIX)

    if(!copy_resources(path, writeAsApp ? QStringList{} : QStringList{"FigmaQmlInterface"}))
        return false;

    QSet<QString> save_image_filter {{figmaQml.elementName()}};

    QStringList qml_files;
    const auto main_file_name =  QML_PREFIX + FigmaQml::validFileName(figmaQml.elementName()) + QML_EXT;
    qml_files << qq(main_file_name);
    QFile qml_out(path + '/' + FOLDER + main_file_name);
    VERIFY(qml_out.open(QFile::WriteOnly), "Cannot write a QtQuick file");
    qml_out.write(figmaQml.sourceCode());
    qml_out.close();

    for(const auto& component_name : figmaQml.components()) {
        save_image_filter.insert(component_name);
        const auto file_name = QML_PREFIX + FigmaQml::validFileName(component_name) + QML_EXT;
        qml_files << qq(file_name);

        const auto target_path = path + '/' + FOLDER + file_name;
        QFile component_out(target_path);
        if(QFile::exists(target_path))
            QFile::remove(target_path);
        VERIFY(component_out.open(QFile::WriteOnly), "Cannot write component " + file_name);
        const auto component_src = figmaQml.componentSourceCode(component_name);
        component_out.write(component_src);
        component_out.close();
    }

    const auto images = figmaQml.saveImages(path + '/' + FOLDER + IMAGE_PREFIX, save_image_filter);

    static const QRegularExpression re_project (R"((^\s*files:\s*\[\s*)(\s*\]))");
    VERIFY(replaceInFile(path + "/FigmaQmlInterface/FigmaQmlInterface.qmlproject", re_project, R"(\1)" + qml_files.join(",") + R"(\2)", {"Project", "QmlFiles"}), "Cannot update qmlproject");

    VERIFY(images, "Cannot save images" )
    QStringList local_images;
    std::transform(images->begin(), images->end(), std::back_inserter(local_images), [](const auto& f){return qq("images/" + QFileInfo(f).fileName());});

    static const QRegularExpression re_images (R"((^\s*files:\s*\[\s*)(\s*\]))");

    VERIFY(replaceInFile(path + "/FigmaQmlInterface/FigmaQmlInterface.qmlproject", re_images, R"(\1)" + local_images.join(",") + R"(\2)", {"Project", "ImageFiles"}), "Cannot update qmlproject");

    if(writeAsApp) {
        static const QRegularExpression re_qml(R"((^\s*source:\s*")("))");
        VERIFY(replaceInFile(path + "/mcu_figma.qml", re_qml, R"(\1)" + main_file_name + R"(\2)", {"Rectangle", "Loader"}), "Cannot update qml file");
    }


    return true;
}

bool executeQulApp(const QVariantMap& parameters, const FigmaQml& figmaQml) {

    // create a monitor
    auto qulInfo = QulInfo::instance(figmaQml);


    QTemporaryDir dir;
    VERIFY(dir.isValid(), "Cannot create temp dir")
    if(!writeQul(dir.path(), parameters, figmaQml, true))
        return false;

    QProcess build_process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_DIR", parameters["qtDir"].toString());
    env.insert("QUL_VER", parameters["qulVer"].toString());
    env.insert("QUL_PLATFORM", parameters["qulPlatform"].toString());
    env.insert("QT_LICENSE_PATH", parameters["qtLicense"].toString());
    build_process.setProcessEnvironment(env);
    build_process.setWorkingDirectory(dir.path());
    build_process.start("bash", {"build.sh"});

    QObject::connect(&build_process, &QProcess::readyReadStandardError, qulInfo, [qulInfo, &build_process]() {
        const auto out = clean(build_process.readAllStandardError());
        qDebug() << (build_process.exitCode() == 0 ? YELLOW : RED) << out << NONE;
        emit qulInfo->information(out, build_process.exitCode() == 0 ? 1 : 0);
    });

    QObject::connect(&build_process, &QProcess::readyReadStandardOutput, qulInfo, [qulInfo, &build_process]() {
        const auto out = clean(build_process.readAllStandardOutput());
        qDebug() << (build_process.exitCode() == 0 ? GREEN : RED) << out << NONE;
        emit qulInfo->information(out, build_process.exitCode() == 0 ? 2 : 0);
    });

    build_process.waitForFinished(-1);
    debug_out(build_process);
    if(build_process.exitCode() == 0) {
        const auto tools_path = parameters["platformTools"].toString();
        if(tools_path.contains("STM32")) {
            const auto programmer = findFile(tools_path, QRegularExpression(R"(STM32_Programmer\.sh)"));
            VERIFY(!programmer.isEmpty(), "Cannot find flasher app");
            const auto platform = parameters["qulPlatform"].toString();
            const auto reg_string = ".*" + platform.left(15) + ".*" + R"(\.stldr$)";
            const auto flash_param = findFile(tools_path, QRegularExpression(reg_string, QRegularExpression::CaseInsensitiveOption));
            VERIFY(!flash_param.isEmpty(), "Cannot find flash configuration");
            const auto binary = findFile(dir.path(), QRegularExpression(R"(mcu_figma\.hex)"));
            VERIFY(!binary.isEmpty(), "Cannot find binary");

            auto serial_port = getSerialPort("STM32 STLink");
            VERIFY(!serial_port.isNull(), "Cannot find serial port connected to board")
            VERIFY(qulInfo->connect(serial_port), "Cannot connect to serial port");

            QProcess flash_process;
            flash_process.setWorkingDirectory(dir.path());
            flash_process.start("bash", {"run_STM32.sh", programmer, flash_param, binary});

            QObject::connect(&flash_process, &QProcess::readyReadStandardError, qulInfo, [qulInfo, &flash_process]() {
                const auto out = clean(flash_process.readAllStandardError());
                qDebug() << (flash_process.exitCode() == 0 ? YELLOW : RED) << out << NONE;
                emit qulInfo->information(out, flash_process.exitCode() == 0 ? 1 : 0);
            });

            QObject::connect(&flash_process, &QProcess::readyReadStandardOutput, qulInfo, [qulInfo, &flash_process]() {
                const auto out = clean(flash_process.readAllStandardOutput());
                qDebug() << GREEN << out << NONE;
                emit qulInfo->information(out, 2);
            });

            flash_process.waitForFinished(-1);
            debug_out(flash_process);
        } else {
            showError("Don't know how to flash the binary!", "Only the STM32 supported, but the image shall be found in " + dir.path() + " as long as this note is open");
        }
    }
    return true;
}
