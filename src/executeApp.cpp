#include "execute_utils.h"
#include "appwrite.h"
#include <QVariantMap>
#include <QTemporaryDir>
#include <QProcess>
#include <QMessageBox>
#include <QEventLoop>
#include <QDirIterator>
#include <QXmlStreamReader>
#include <QSet>
#include <QProcess>
#include "figmaqml.h"


static void debug_out (QProcess& process) {
    qDebug().noquote() << GREEN << ExecuteUtils::clean(process.readAllStandardOutput()) << NONE;
    if(process.exitCode() == 0)
        qDebug().noquote() << YELLOW << ExecuteUtils::clean(process.readAllStandardError()) << NONE;
    else
        qDebug().noquote() << RED << ExecuteUtils::clean(process.readAllStandardError()) << NONE;
}

static bool runApp(QProcess& run_process, const QTemporaryDir& dir) {
    const auto app = ExecuteUtils::findFile(dir.path(), QRegularExpression(R"(app_figma)"), ExecuteUtils::Exe);
    VERIFY(!app.isEmpty(), "Cannot find application");
    run_process.setWorkingDirectory(dir.path());
    run_process.start(app);
    return true;
}

void ExecuteUtils::waitRun(QProcess& process) {
    QEventLoop innerLoop;
    QObject::connect(&process, &QProcess::finished,  &innerLoop, &QEventLoop::quit);
    innerLoop.exec();
    debug_out(process);
}

bool AppWrite::executeApp(const QVariantMap& parameters, const FigmaQml& figmaQml, const QVector<int>& elements) {

    // create a monitor
    auto eInfo = ExecuteInfo::instance(figmaQml);

    QTemporaryDir dir;
    VERIFY(dir.isValid(), "Cannot create temp dir")
    if(! AppWrite::writeApp(dir.path(), figmaQml, true, elements))
        return false;

    QProcess build_process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const auto qt_dir = parameters["qtDir"].toString();
    VERIFY(QDir(qt_dir).exists(), "Directory " + qt_dir + " not found!");
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

