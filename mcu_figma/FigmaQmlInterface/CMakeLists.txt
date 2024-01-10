cmake_minimum_required (VERSION 3.21.1)

project(FigmaQmlInterface VERSION 0.1.0 LANGUAGES C CXX)
set(BUG_199 TRUE) # untill 2.7
find_package(Qul)

qul_add_target(${PROJECT_NAME}
   STATIC_LIBRARY
   QML_PROJECT "${PROJECT_NAME}.qmlproject"
   FigmaQmlInterface.hpp
   FigmaQmlUi.qml
)

if(BUG_199)
# Using workaround for a QT MCU bug: see https://bugreports.qt.io/browse/QTMCU-199"
target_compile_definitions(${PROJECT_NAME} PUBLIC
      BUG_199=1
  )
endif()


target_include_directories(${PROJECT_NAME} PUBLIC .)


