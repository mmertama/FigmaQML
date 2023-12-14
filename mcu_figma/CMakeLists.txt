cmake_minimum_required (VERSION 3.21.1)

project(mcu_figma VERSION 0.0.1 LANGUAGES C CXX ASM)

# Qt bug workaround
set(BUG_199 TRUE)

# This must be before the fetch content below!
find_package(Qul PATHS ${FIGMAQML_QTC_DIR}/QtMCUs/${FIGMAQML_MCU_VER}/lib/cmake/Qul REQUIRED)

qul_add_target(${PROJECT_NAME} QML_PROJECT ${PROJECT_NAME}.qmlproject GENERATE_ENTRYPOINT)

#FREERTOS apps requires this
set_target_properties(${PROJECT_NAME} PROPERTIES FREERTOS_PROVIDE_MEMORY_ALLOCATOR_OVERLOADS TRUE)

if(BUG_199)
# Using workaround for a QT MCU bug: see https://bugreports.qt.io/browse/QTMCU-199"
target_compile_definitions(${PROJECT_NAME} PUBLIC
      BUG_199=1
  )
endif()

message("Platform is ${MCU_PLATFORM}")

target_sources(${PROJECT_NAME} PUBLIC
    ${PROJECT_NAME}.qmlproject
    ${PROJECT_NAME}.qml
    )

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

app_target_setup_os(${PROJECT_NAME})

