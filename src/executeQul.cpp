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
#include "qulInfo.h"
#include "figmaqml.h"

#define VERIFY(x, t) if(!x) {showError(t); return false;}

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
            emit m_info.information(QString::fromLatin1(m_port->readAll()));
            });
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
bool copy_resources(const QTemporaryDir& dir) {
    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        if(info.isFile() && info.filePath().mid(1, 5) == "/mcu/") {
            const auto target_path = dir.path() + info.filePath().mid(5);
            const auto abs_path = QFileInfo(target_path).absolutePath();
            VERIFY(QDir().mkpath(abs_path), "Cannot create folder " + abs_path)
            qDebug() << target_path;
            VERIFY(QFile::copy(info.filePath(), target_path), "Cannot create " + target_path)
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


bool executeQulApp(const QVariantMap& parameters, const FigmaQml& figmaQml) {

    // create a monitor
    auto qulInfo = QulInfo::instance(figmaQml);


    QTemporaryDir dir;
    VERIFY(dir.isValid(), "Cannot create temp dir")
    if(!copy_resources(dir))
        return false;

    QFile qml_out(dir.path() + '/' + "mcu_figma.qml");
    VERIFY(qml_out.remove(), "Cannot overwrite QtQuick file");
    VERIFY(qml_out.open(QFile::WriteOnly), "Cannot write a QtQuick file");
    qml_out.write(figmaQml.sourceCode());
    qml_out.close();

    for(const auto& component_name : figmaQml.components()) {
        const auto file_name = FigmaQml::validFileName(component_name);
        QFile component_out(dir.path() + '/' + file_name);
        VERIFY(component_out.open(QFile::WriteOnly), "Cannot write component " + file_name);
        const auto component_src = figmaQml.componentSourceCode(component_name);
        component_out.write(component_src);
        component_out.close();
    }

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
        qulInfo->information(clean(build_process.readAllStandardError()));
    });

    build_process.waitForFinished();
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
            flash_process.waitForFinished();
            debug_out(flash_process);
        } else {
            showError("Don't know how to flash the binary!", "Only the STM32 supported, but the image shall be found in " + dir.path() + " as long as this note is open");
        }
    }
    return true;
}
