#pragma once

#include <QVariantMap>
#include <QStringList>

class FigmaQml;

namespace AppWrite {
#ifdef HAS_QUL
bool writeQul(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements);
#ifdef HAS_EXECUTE
bool executeQulApp(const QVariantMap& parameters, const FigmaQml& figmaQml, const std::vector<int>& elements);
#endif
#endif

#ifdef HAS_EXECUTE
bool executeApp(const QVariantMap& parameters, const FigmaQml& figmaQml, const std::vector<int>& elements);
#endif

constexpr auto STM32 = "STM32";
QStringList supportedQulHardware();
bool writeApp(const QString& path, const FigmaQml& figmaQml, bool writeAsApp, const std::vector<int>& elements);
}

