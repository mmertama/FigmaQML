#include <QVariant>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
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
#include "qulInfo.h"
#include "figmaqml.h"
#include "utils.h"
#include "execute_utils.h"


constexpr auto STM32 = "STM32";

extern QStringList supportedQulHardware() {
    return {STM32};
}


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

    void disconnect() {
        m_port->disconnect();
    }

private:
    QulInfo& m_info;
    std::unique_ptr<QSerialPort> m_port;
};


QulInfo* QulInfo::instance(const FigmaQml& figmaQml) {
    static QulInfo* info;
    if(!info) {
        info = new QulInfo(figmaQml);
    }
    return info;
}



QulInfo::QulInfo(const FigmaQml& figmaQml, QObject* parent) : ExecuteInfo(figmaQml, parent),  m_private(std::make_unique<QulInfo::Private>(*this)) {
    QObject::connect(&figmaQml, &FigmaQml::qulInfoStop, this, [this]() {m_private->disconnect();});
}

QulInfo::~QulInfo() {}

bool QulInfo::connect(const QSerialPortInfo& info) {
    return m_private->connect(info);
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

bool writeQul(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements) {
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

static bool flashSTM32(QProcess& flash_process, const QTemporaryDir& dir, const QVariantMap& parameters, QulInfo& qulInfo) {
    const auto tools_path = parameters["platformTools"].toString();
    VERIFY(tools_path.contains("STM32"), "Cannot find STM32 from path");

    const auto programmer =  ExecuteUtils::findFile(tools_path, QRegularExpression(R"(STM32_Programmer\.sh)"), ExecuteUtils::Any);
    VERIFY(!programmer.isEmpty(), "Cannot find flasher app");
    const auto platform = parameters["qulPlatform"].toString();
    const auto reg_string = ".*" + platform.left(15) + ".*" + R"(\.stldr$)";
    const auto flash_param =  ExecuteUtils::findFile(tools_path, QRegularExpression(reg_string, QRegularExpression::CaseInsensitiveOption), ExecuteUtils::Any);
    VERIFY(!flash_param.isEmpty(), "Cannot find flash configuration");
    const auto binary =  ExecuteUtils::findFile(dir.path(), QRegularExpression(R"(mcu_figma\.hex)"), ExecuteUtils::Any);
    VERIFY(!binary.isEmpty(), "Cannot find binary");

    auto serial_port = getSerialPort("STM32 STLink");
    VERIFY(!serial_port.isNull(), "Cannot find serial port connected to board")
    VERIFY(qulInfo.connect(serial_port), "Cannot connect to serial port");

    flash_process.setWorkingDirectory(dir.path());
    flash_process.start("bash", { "run_STM32.sh", programmer, flash_param, binary});

    return true;
}

bool executeQulApp(const QVariantMap& parameters, const FigmaQml& figmaQml, const std::vector<int>& elements) {

    // create a monitor
    auto qulInfo = QulInfo::instance(figmaQml);

    QTemporaryDir dir;
    VERIFY(dir.isValid(), "Cannot create temp dir")
    if(!writeQul(dir.path(), figmaQml, true, elements))
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
        const auto out = ExecuteUtils::clean(build_process.readAllStandardError());
        qDebug() << (build_process.exitCode() == 0 ? YELLOW : RED) << out << NONE;
        emit qulInfo->information(out, build_process.exitCode() == 0 ? 1 : 0);
    });

    QObject::connect(&build_process, &QProcess::readyReadStandardOutput, qulInfo, [qulInfo, &build_process]() {
        const auto out = ExecuteUtils::clean(build_process.readAllStandardOutput());
        qDebug() << (build_process.exitCode() == 0 ? GREEN : RED) << out << NONE;
        emit qulInfo->information(out, build_process.exitCode() == 0 ? 2 : 0);
    });

    ExecuteUtils::waitRun(build_process);

    if(build_process.exitCode() != 0) {
        ExecuteUtils::showError("Build of test application failed!");
        return false;
    }

    const auto flash_failed = [&]() {ExecuteUtils::showError("Flash failed binary!", "However Image shall be found in " + dir.path() + " as long as this note is open");};

    QProcess flash_process;
    QObject::connect(&flash_process, &QProcess::readyReadStandardError, qulInfo, [qulInfo, &flash_process]() {
        const auto out = ExecuteUtils::clean(flash_process.readAllStandardError());
        qDebug() << (flash_process.exitCode() == 0 ? YELLOW : RED) << out << NONE;
        emit qulInfo->information(out, flash_process.exitCode() == 0 ? 1 : 0);
    });

    QObject::connect(&flash_process, &QProcess::readyReadStandardOutput, qulInfo, [qulInfo, &flash_process]() {
        const auto out = ExecuteUtils::clean(flash_process.readAllStandardOutput());
        qDebug() << GREEN << out << NONE;
        emit qulInfo->information(out, 2);
    });

    const auto hardware = parameters["platformHardwareValue"].toString();

    if(hardware == STM32) {
        if(!flashSTM32(flash_process, dir, parameters, *qulInfo)) {
            flash_failed();
            return false;
        }
        ExecuteUtils::waitRun(flash_process);
    } else {
        flash_failed();
        return false;
    }

    return true;
}
