
//#include <QtSystemDetection>
#include "fontinfo.h"

#ifdef Q_OS_LINUX
#include <QFont>
#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include "utils.h"


class FontInfo::Private : public QProcess {};

FontInfo::~FontInfo() {}

FontInfo::FontInfo(QObject* parent) : QObject(parent), m_private{new Private} {
    connect(m_private.get(), &QProcess::finished, this, [this](int exitCode, auto){
        if(exitCode == 0) {
            const auto fontFilePath = m_private->readAllStandardOutput().trimmed();
            qDebug() << "fontFilePath" << fontFilePath;
            emit fontPath(fontFilePath);
        } else {
            qDebug() << "Error running fc-match:" << m_private->errorString();
            emit pathError(m_private->errorString());
        }
    });
}

void FontInfo::getFontFilePath(const QFont& font) {
        getFontFilePath(font.family(), font.style(), font.weight(), font.pixelSize());
    }


void FontInfo::getFontFilePath(const QString &family, QFont::Style style, int weight, int size) {
    // Construct the font pattern string
    const auto style_str = enumToString(style);
    const auto fontPattern = QString("%1:style=%2:weight=%3:pixelSize=%4").arg(family, style_str).arg(weight).arg(size);

    // Run fc-match to get the font file path
    m_private->start("fc-match", QStringList() << "--format=%{file}" << fontPattern);

}
#else
class FontInfo::Private {};
FontInfo::~FontInfo() {}
FontInfo::FontInfo(QObject* parent) : QObject(parent) {}
void  FontInfo::getFontFilePath(const QFont&) {}
void  FontInfo::getFontFilePath(const QString &, QFont::Style , int , int ) {}
#endif


