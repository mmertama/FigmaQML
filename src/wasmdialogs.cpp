#include "figmaqml.h"
#include <QTemporaryDir>
#include <QFileDialog>
#include <quazip/JlCompress.h>


bool FigmaQml::saveAllQMLZipped(const QString& docName, const QString& canvasName) {
    QTemporaryDir temp;
    if(!saveAllQML(temp.path())) {
        qDebug() << "Failded to save" << temp.path();
        return false;
    }
     const auto temp_name = QString::fromLatin1(std::tmpnam(nullptr), -1);
     if(!JlCompress::compressDir(temp_name, temp.path())) {
          qDebug() << "Failded to compress" << temp_name << "of" << temp.path();
         return  false;
     }
     QFile dump(temp_name);
     if(!dump.open(QIODevice::ReadOnly)) {
         qDebug() << "Cannot open" << temp_name;
         return false;
     }
     const auto data = dump.readAll();
     const auto zipName = docName + "_" + canvasName + ".zip";
     QFileDialog::saveFileContent(data, zipName);
     return true;
}

bool FigmaQml::importFontFolder() {
    m_fontFolder = "/tmp/fonts";
    const QDir fontFolder(m_fontFolder);
    if(!fontFolder.exists())
        fontFolder.mkpath(".");
    QFileDialog::getOpenFileContent("", [this](const QString & filename, const QByteArray & bytes) {
            QFile file(m_fontFolder + '/' + filename);
            if(!file.open(QIODevice::WriteOnly)) {
                emit error("Cannot write fonts");
            }
            file.write(bytes);
            file.close();
            emit fontFolderChanged();
    });
    return false;
}

bool FigmaQml::store(const QString& docName, const QString& tempName) {
    QFile file(tempName);
    if(!file.open(QIODevice::ReadOnly))
        return false;
    const auto bytes = file.readAll();
    QFileDialog::saveFileContent(bytes, docName);
    return true;
}

void FigmaQml::restore() {
    QFileDialog::getOpenFileContent("FigmaQML (*.figmaqml)",
                                    [this](const QString& name, const QByteArray& bytes) {
        const QString file_name("/tmp/restore.figmaqml");
        QFile file(file_name);
        if(!file.open(QIODevice::WriteOnly)) {
            emit error("Cannot restore");
        }
        file.write(bytes);
        file.close();
        emit wasmRestored(name, file_name);
    });
}
