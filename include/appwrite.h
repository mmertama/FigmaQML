#pragma once

#include <QVariantMap>
#include <QStringList>

class FigmaQml;

namespace AppWrite {
#ifdef HAS_QUL
bool executeQulApp(const QVariantMap& parameters, const FigmaQml& figmaQml, const std::vector<int>& elements);
bool writeQul(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements);
QStringList supportedQulHardware();
#endif

#ifdef HAS_EXECUTE
bool executeApp(const QVariantMap& parameters, const FigmaQml& figmaQml, const std::vector<int>& elements);
#endif

bool writeApp(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements);
}

