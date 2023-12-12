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

#define VERIFY(x, t) if(!x) {showError(t); return;}

void showError(const QString& errorNote, const QString& infoNote = "") {
    QMessageBox msgBox;
    msgBox.setText(errorNote);
    if(!infoNote.isEmpty())
        msgBox.setInformativeText(infoNote);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void executeQulApp(const QVariantMap& parameters) {
    QTemporaryDir dir;
    VERIFY(dir.isValid(), "Cannot create temp dir")
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

                                                                                                                                                                                                                                                               QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_DIR", parameters["qtDir"].toString());
    env.insert("QUL_VER", parameters["qulVer"].toString());
    env.insert("QUL_PLATFORM", parameters["qulPlatform"].toString());
    env.insert("QT_LICENSE_PATH", parameters["qtLicense"].toString());
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(dir.path());
    process.start("bash", {"build.sh"});
    process.waitForFinished();
    qDebug() << process.readAllStandardOutput();
    qDebug() << process.readAllStandardError();
}
