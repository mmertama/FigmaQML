#ifndef FONTPATH_H
#define FONTPATH_H

#include <QFont>
#include <QObject>

class FontInfo : public QObject{
    Q_OBJECT
public:
    FontInfo(QObject* parent);
    ~FontInfo();
    void getFontFilePath(const QString &family, QFont::Style style, int weight, int size);
    void getFontFilePath(const QFont& font);
signals:
    void fontPath(const QString& path);
    void pathError(const QString& error);
private:
    class Private;
    std::unique_ptr<Private> m_private;
};

#endif // FONTPATH_H
