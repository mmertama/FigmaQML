#ifndef EXECUTE_UTILS_H
#define EXECUTE_UTILS_H

#include <QString>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>

#define VERIFY(x, t) if(!x) {ExecuteUtils::showError(t); return false;}

constexpr auto RED = "\033[31m";
constexpr auto NONE = "\033[0m";
constexpr auto GREEN = "\033[32m";
constexpr auto YELLOW = "\033[43m";


constexpr auto FOLDER = "FigmaQmlInterface/";
constexpr auto QML_PREFIX = "qml/";
constexpr auto IMAGE_PREFIX =  "images/";
constexpr auto QML_EXT = ".qml";

class FigmaQml;

namespace ExecuteUtils {

    /**
     * @brief showError
     * @param errorNote
     * @param infoNote
     */
    void showError(const QString& errorNote, const QString& infoNote = "");

    /**
     * @brief waitRun
     * @param process
     */
    void waitRun(QProcess& process);

    /**
     * @brief clean - clean off ANSI
     * @param strm
     * @return
     */
    QString clean(const QByteArray& strm);

    /**
     * @brief copy_resources
     * @param path
     * @param resouce_root
     * @param folders_only
     * @return
     */
    bool copy_resources(const QString& path, const QString& resouce_root, const QStringList& folders_only = {});

    /**
     * @brief writeElement
     * @param path
     * @param main_file_name
     * @param figmaQml
     * @param qml_files
     * @param save_image_filter
     * @param indices
     * @return
     */
    bool writeElement(const QString& path, const QString& main_file_name, const FigmaQml& figmaQml, QStringList& qml_files, QSet<QString>& save_image_filter, std::pair<int, int> indices);

    /**
     * @brief qq
     * @param str
     * @return
     */
    QString qq(const QString& str); // todo replace std::quoted

    /**
    * @brief replaceInFile
    * @param fname
    * @param re
    * @param replacement
    * @param context
    * @param force_context is a hack so I can use this for C++ files, sorry
    * @return
    */
    bool replaceInFile(const QString& fname, const QRegularExpression& re, const QString& replacement, const QStringList context, bool force_context = false);

    /**
     * @brief findFile
     * @param path
     * @param regexp
     * @return
     */
    QString findFile(const QString& path, const QRegularExpression& regexp);
}

class ExecuteInfo: public QObject {
    Q_OBJECT
public:
    static ExecuteInfo* instance(const FigmaQml& figmaQml);
    virtual ~ExecuteInfo();
protected:
    ExecuteInfo(const FigmaQml& figmaQml, QObject* parent = nullptr);
signals:
    void information(const QString& info, int level);
};

#endif // EXECUTE_UTILS_H
