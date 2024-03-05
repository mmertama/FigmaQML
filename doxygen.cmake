function (add_doxygen MAIN_TARGET)

    find_program(DOCX doxygen)
    if(NOT DOCX)
        message(FATAL_ERROR "Doxygen is not found - try apt-get install doxygen")
    endif()

    find_program(GRPHZ dot)
    if(NOT GRPHZ)
        message(FATAL_ERROR "graphviz for doxygen is not found - try apt-get install graphviz")
    endif()

    find_program(SPELL aspell)
    if(NOT SPELL)
        message(FATAL_ERROR "aspell for doxygen is not found - try apt-get install aspell")
    endif()


    find_package(Doxygen REQUIRED dot)
    set(DOXYGEN_EXTRACT_ALL FALSE)
    set(DOXYGEN_INTERNAL_DOCS NO)
    set(DOXYGEN_EXTRACT_PRIVATE NO)
    set(DOXYGEN_BUILTIN_STL_SUPPORT TRUE)

    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md")
    set(DOXYGEN_DOT_IMAGE_FORMAT svg) # use vector graphics, not png
    set(DOXYGEN_DOT_TRANSPARENT YES) # especially good for dark mode (at least in firefox, currently not chrome)

    set(DOXYGEN_GENERATE_TREEVIEW YES)
    if(NOT DEFINED DOXYGEN_PROJECT_NAME)
        set(DOXYGEN_PROJECT_NAME ${MAIN_TARGET})
    endif()
    set(DOXYGEN_MARKDOWN_SUPPORT YES)
    set(ENABLE_PREPROCESSING TRUE)

    if(NOT DEFINED DOXYGEN_FILE_PATTERNS)
        set(DOXYGEN_FILE_PATTERNS           *.c *.cc *.cxx *.cpp *.c++ *.ii *.ixx *.ipp *.i++ *.inl *.h *.hh *.hxx *.hpp *.h++ *.inc *.md)
    endif()

    #set(DOXYGEN_WARN_NO_PARAMDOC        YES)

    set(DOXYGEN_RECURSIVE YES)

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/README.md")
        message(FATAL_ERROR "${CMAKE_SOURCE_DIR}/README.md not found!")
    endif()

    set(DOXYGEN_HTML_OUTPUT            ${CMAKE_BINARY_DIR}/html)
    set(DOXYGEN_JAVADOC_AUTOBRIEF      YES)
    set(DOXYGEN_GENERATE_HTML          YES)
    set(DOXYGEN_HAVE_DOT               YES)

    set(DOXYGEN_DOT_MULTI_TARGETS  YES)

    file(MAKE_DIRECTORY ${DOXYGEN_HTML_OUTPUT})

    if(DEFINED DOXYGEN_DOCUMENTS)
        doxygen_add_docs(doxy_docs
            USE_STAMP_FILE
            ${CMAKE_SOURCE_DIR}/README.md
            ${DOXYGEN_DOCUMENTS})
    else()
        doxygen_add_docs(DOXY_TARGET
            ${CMAKE_SOURCE_DIR}/README.md
            ${CMAKE_CURRENT_SOURCE_DIR}/.})
    endif()

    add_custom_target(doxy)

    if(DOXYGEN_HAS_SPELL)
        set(SPELL_EXCLUDE " --exclude=doxygen.cmake --exclude-dir=aspell --exclude=graph_legend.html  --exclude=\*.png --exclude=\*.ttf --exclude=\*.svg ")
        foreach(FE ${DOXYGEN_SPELL_EXCLUDE_DIRS})
            set(SPELL_EXCLUDE "${SPELL_EXCLUDE} --exclude-dir=${FE} ")
        endforeach()
        foreach(FE ${DOXYGEN_SPELL_EXCLUDE_FILES})
            set(SPELL_EXCLUDE "${SPELL_EXCLUDE} --exclude=${FE} ")
        endforeach()
        set(DICTIONARY "${CMAKE_SOURCE_DIR}/aspell/spell_words.txt")
        file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/aspell)
        if(NOT EXISTS ${DICTIONARY} )
            file(WRITE ${DICTIONARY}  "personal_ws-1.1 en 84 utf-8\nforwhich\nPrivateBase\nProtectedBase\nPublicBase\nTempl\nusedClass")
        endif()
        set(DICT "${CMAKE_SOURCE_DIR}/aspell/dict.txt")
        set(REPL "${CMAKE_SOURCE_DIR}/aspell/repl.txt")
        # if spellchecker lists any words, either fix that or add into ${DICTIONARY}
        set(SPELL_CMD "find . -type f -name '*.html' -exec cat {} \; | ${SPELL} list -H -p ${DICTIONARY} | sort | uniq | while read -r word; do grep -r ${SPELL_EXCLUDE} -n -m 1  \"\$word\" ${CMAKE_SOURCE_DIR}; echo \"when looking for $word\"; done")
        set(SPELL_CMD_TEST_0 "if [[ $( ${SPELL_CMD} | wc -l) -ne 0 ]]; then echo Spelling errors:; fi")
        set(SPELL_CMD_TEST_1 "if [[ $( ${SPELL_CMD} | wc -l) -ne 0 ]]; then exit 1; fi")
        add_custom_target(doxy_spell
            COMMAND bash -c "${SPELL_CMD_TEST_0}"
            COMMAND bash -c "${SPELL_CMD}"
            COMMAND bash -c "${SPELL_CMD_TEST_1}"
            COMMAND echo ""
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/html"
            VERBATIM
        )
        add_dependencies(doxy doxy_spell)
        add_dependencies(doxy_spell doxy_docs)
    else()
        add_dependencies(doxy doxy_docs)
    endif()





    ## this is some Nonja spesific nonsense I figured out this workaround
    ## Can be bug in the version Im using, nevertheless its was annoying as Ninja wont find doxy_docs target without this
    ## It also can be some Qt related stuff...
    #if(CMAKE_GENERATOR STREQUAL "Ninja")
    #    add_custom_command(TARGET ${MAIN_TARGET} POST_BUILD
    #        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target CMakeFiles/doxy_docs
    #        VERBATIM
    #    )
    #else()
    # alternative to that hack is to generate a dummy target!
    add_dependencies(${MAIN_TARGET} doxy)
    #endif()


endfunction()
