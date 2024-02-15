cmake_minimum_required(VERSION 3.21)

project(app_figma LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 CONFIG COMPONENTS Core Quick Network REQUIRED)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Quick)
qt_standard_project_setup()

add_subdirectory(FigmaQmlInterface)

set(SOURCES
    qml/main.qml
    src/main.cpp)

qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    qml.qrc)
    
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)

if (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE ON
            )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Quick FigmaQmlInterface)

