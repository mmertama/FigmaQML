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
    //QFileDialog::getOpenFileContent(const QString &nameFilter, const std::function<void (const QString &, const QByteArray &)> &fileOpenCompleted)
    return false;
}

bool FigmaQml::store(const QString& docName) {
  //  void QFileDialog::saveFileContent(const QByteArray &fileContent, const QString &fileNameHint = QString())
    return {};
}

QString FigmaQml::restore() {
 //   QFileDialog::getOpenFileContent(const QString &nameFilter, const std::function<void (const QString &, const QByteArray &)> &fileOpenCompleted)
    return {};
}
