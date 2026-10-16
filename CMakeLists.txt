cmake_minimum_required(VERSION 3.22)

project(
    linux_voip_softphone
    VERSION 0.2.0
    DESCRIPTION "Linux VoIP softphone engineering portfolio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ENABLE_PJSIP "Enable PJSIP/PJSUA2 integration" OFF)
option(ENABLE_QT_GUI "Enable the Qt 6/QML desktop application" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Dependencies)
include(Sanitizers)
include(StaticAnalysis)

project_enable_clang_tidy()

if(ENABLE_PJSIP)
    project_setup_pjsip_dependency()
endif()

if(ENABLE_QT_GUI)
    project_setup_qt_dependency()
    qt_standard_project_setup()
endif()

add_subdirectory(src)
add_subdirectory(labs)

include(CTest)
if(BUILD_TESTING)
    project_setup_test_dependencies()
    add_subdirectory(tests)
endif()
