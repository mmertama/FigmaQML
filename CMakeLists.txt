cmake_minimum_required(VERSION 3.16)

project(FigmaQML LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(LET_QT5)
    find_package(Qt5 QUIET COMPONENTS Core Quick Network Widgets Concurrent)
    if(Qt5_FOUND)
        message("Project is Qt5")
    else()
        message(FATAL_ERROR "Qt5 is not found!")
    endif()
elseif(EMSCRIPTEN)
    find_package(Qt6 CONFIG COMPONENTS Core Quick Core5Compat)
    if(Qt6_FOUND)
        message("Project is Qt6 Emscripten")
    else()
         message(FATAL_ERROR "Qt6 Emscripten is not found!")
     endif()
else()
    find_package(Qt6 CONFIG COMPONENTS Core Quick Network Widgets Concurrent Core5Compat)
    if(Qt6_FOUND)
        message("Project is Qt6")
    else()
         message(FATAL_ERROR "Qt6 is not found!")
    endif()
endif()


SET(SOURCES
    src/main.cpp
    qml/qml.qrc
    src/figmaget.cpp
    include/figmaget.h
    src/figmaqml.cpp
    include/figmaqml.h
    include/clipboard.h
    include/figmaparser.h
    include/downloads.h
    src/downloads.cpp
    include/figmadata.h
    include/figmadocument.h
    include/fontcache.h
    include/providers.h
    src/figmaparser.cpp
    include/orderedmap.h
    include/utils.h
    include/functorslot.h
    include/figmaprovider.h
)

if(EMSCRIPTEN)
    SET(SOURCES ${SOURCES} src/wasmdialogs.cpp)
endif()


set(VERSION_NUMBER "2.0.0")

add_compile_definitions(VERSION_NUMBER=${VERSION_NUMBER})

#add_executable(FigmaQML
qt_add_executable(FigmaQML ${SOURCES})

if(WIN32)
  #  Windows does not support dual mode, we pick cmd mode
  #  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
  #  set (EXTRA PUBLIC Qt5::WinMain)
endif()

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()


if (APPLE)
    set_target_properties(FigmaQML PROPERTIES
            MACOSX_BUNDLE ON
            )
endif()

include_directories(include)

target_compile_definitions(FigmaQML PRIVATE ASSERT_NESTED=1)

if(NOT EMSCRIPTEN)
target_compile_definitions(FigmaQML
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
endif()

if(Qt5_FOUND)
    target_compile_definitions(FigmaQML PRIVATE -DQT5)
    target_link_libraries(FigmaQML
        PRIVATE Qt5::Core Qt5::Quick Qt5::Network Qt5::Widgets Qt5::Concurrent ${EXTRA})
elseif(EMSCRIPTEN)
    set(QT_WASM_INITIAL_MEMORY, "300MB")
    subdirs(modules/zlib)
    add_custom_target(zlib_target DEPENDS zlibstatic)
    set(ZLIB_INCLUDE  ${CMAKE_SOURCE_DIR}/modules/zlib)
    set(ZCONF_INCLUDE  ${CMAKE_BINARY_DIR}/modules/zlib)
    set(ZLIB_LIBRARY  zlibstatic)
    subdirs(modules/quazip)
    execute_process(COMMAND em++ --version OUTPUT_VARIABLE out_p OUTPUT_STRIP_TRAILING_WHITESPACE)
    add_custom_target(quazip DEPENDS QuaZip)
    add_dependencies(quazip zlib_target)
    include_directories(${CMAKE_SOURCE_DIR}/modules/quazip ${ZLIB_INCLUDE} ${ZCONF_INCLUDE})
    string(REGEX MATCH "[0-9]+\.[0-9]+\.[0-9]+"
        ver_p ${out_p})
    if(NOT ${ver_p} STREQUAL "3.1.14")
        message(FATAL_ERROR "emsdk 3.1.14 expected now!, you have ${ver_p}")
    endif()
    message("emsdk ${ver_p}")
    configure_file(favicon.ico "${CMAKE_CURRENT_BINARY_DIR}/favicon.ico" COPYONLY)
    target_link_options(FigmaQML PUBLIC -sASYNCIFY -Os -sNO_DISABLE_EXCEPTION_CATCHING -sASYNCIFY_STACK_SIZE=65535 -sASSERTIONS=2 -sRUNTIME_LOGGING=1 -sSAFE_HEAP=1)
    target_compile_definitions(FigmaQML PRIVATE
        -DNO_CONCURRENT
        -DNO_SSL
        -DWASM_FILEDIALOGS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_NAME}.html $<CONFIG>/${PROJECT_NAME}.html
        COMMAND ${CMAKE_COMMAND} -E copy qtloader.js $<CONFIG>/qtloader.js
        COMMAND ${CMAKE_COMMAND} -E copy qtlogo.svg $<CONFIG>/qtlogo.svg
        COMMAND ${CMAKE_COMMAND} -E copy favicon.ico $<CONFIG>/favicon.ico
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Obvious bug in QT 6.4 and these files are in the wrong place")
    target_link_libraries(FigmaQML
      PRIVATE QuaZip
      PRIVATE Qt6::Core Qt6::Quick Qt6::Network Qt6::Widgets Qt6::Core5Compat ${EXTRA})
else()
    target_compile_definitions(FigmaQML PRIVATE -DNO_CONCURRENT -DNO_SSL)
    target_link_libraries(FigmaQML
      PRIVATE Qt6::Core Qt6::Quick Qt6::Network Qt6::Widgets Qt6::Concurrent Qt6::Core5Compat ${EXTRA})
endif()
