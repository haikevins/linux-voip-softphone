add_executable(lab_raii
    main.cpp
)

target_include_directories(lab_raii
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

project_set_warnings(lab_raii)
project_enable_sanitizers(lab_raii)
