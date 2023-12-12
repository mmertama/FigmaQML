set(FIGMAQML_DIR ${CMAKE_CURRENT_LIST_DIR})
function(FigmaQml_AddQml)
    set(OPTIONS)
    set(SINGLE_VALUE_ARGS)
    set(MULTI_VALUE_ARGS QMLS MODULES HEADERS IMAGES)

    cmake_parse_arguments(ADD_FUNC
        "${OPTIONS}"
        "${SINGLE_VALUE_ARGS}"
        "${MULTI_VALUE_ARGS}"
         ${ARGN})

        # QML
        if(DEFINED ADD_FUNC_QMLS AND NOT ADD_FUNC_QMLS STREQUAL "")
            foreach(ARG IN LISTS ADD_FUNC_QMLS)
                get_filename_component(FNAME ${ARG} NAME )
                file(COPY ${ARG} DESTINATION "${FIGMAQML_DIR}/qml")
                set(DYNAMIC_LOADED_QML_FILES "${DYNAMIC_LOADED_QML_FILES},\n            \"qml/${FNAME}\"")

                add_custom_command(
                    OUTPUT "${FIGMAQML_DIR}/qml/${FNAME}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ARG} ${FIGMAQML_DIR}/qml
                    COMMENT "Copying ${ARG} to ${FIGMAQML_DIR}/qml"
                )
            endforeach()
            add_custom_target(
                qml_changed ALL
                COMMAND ${CMAKE_COMMAND} -E touch ${FIGMAQML_DIR}/FigmaQmlInterface.qmlproject
                DEPENDS ${ADD_FUNC_QMLS}
                COMMENT "Touch ${FIGMAQML_DIR}/FigmaQmlInterface.qmlproject"
            )
        endif()

        # MODULES
        if(DEFINED ADD_FUNC_MODULES AND NOT ADD_FUNC_MODULES STREQUAL "")
            message(FATAL_ERROR "mozules is <${ADD_FUNC_MODULES}>")
            foreach(ARG IN LISTS ADD_FUNC_MODULES)
                list(APPEND DYNAMIC_LOADED_MODULES "\"${ARG}\"")
            endforeach()

            list(JOIN DYNAMIC_LOADED_MODULES ",\n            " DYNAMIC_LOADED_MODULES)
            set(DYNAMIC_LOADED_MODULES ",\n            ${DYNAMIC_LOADED_MODULES}\n")
        endif()

        # HEADERS
        if(DEFINED ADD_FUNC_HEADERS AND NOT ADD_FUNC_HEADERS STREQUAL "")
            foreach(ARG IN LISTS ADD_FUNC_HEADERS)
                list(APPEND DYNAMIC_LOADED_CPP_FILES "\"${ARG}\"" )
            endforeach()

            list(JOIN DYNAMIC_LOADED_CPP_FILES ",\n            " DYNAMIC_LOADED_CPP_FILES)
            set(DYNAMIC_LOADED_CPP_FILES ",\n            ${DYNAMIC_LOADED_CPP_FILES}\n")
        endif()

        # IMAGES

        if(DEFINED ADD_FUNC_IMAGES AND NOT ADD_FUNC_IMAGES STREQUAL "")
            foreach(ARG IN LISTS ADD_FUNC_IMAGES)
                list(APPEND DYNAMIC_LOADED_IMAGE_FILES "\"${ARG}\"")
            endforeach()

            list(JOIN DYNAMIC_LOADED_IMAGE_FILES ",\n            " DYNAMIC_LOADED_IMAGE_FILES)
            set(DYNAMIC_LOADED_IMAGE_FILES ",\n ${DYNAMIC_LOADED_IMAGE_FILES}\n")
        endif()

        # configure
        configure_file(${FIGMAQML_DIR}/FigmaQmlInterface.qmlproject.in ${FIGMAQML_DIR}/FigmaQmlInterface.qmlproject @ONLY)

        if(NOT EXISTS ${FIGMAQML_DIR}/FigmaQmlInterface.qmlproject)
            message(FATAL_ERROR "create ${FIGMAQML_DIR}/FigmaQmlInterface.qmlproject failed!")
        endif()

endfunction()
