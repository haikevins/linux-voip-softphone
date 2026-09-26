function(project_enable_clang_tidy)
    if(NOT ENABLE_CLANG_TIDY)
        return()
    endif()

    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY
        "${CLANG_TIDY_EXECUTABLE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
        PARENT_SCOPE
    )
endfunction()
