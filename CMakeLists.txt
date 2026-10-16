cmake_minimum_required(VERSION 3.22)

project(
    linux_voip_softphone
    VERSION 1.0.0
    DESCRIPTION "Linux VoIP softphone engineering portfolio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
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

find_package(Threads REQUIRED)

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
add_subdirectory(tools)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME Phase9CrashGeneratorHelpTest
        COMMAND $<TARGET_FILE:crash_generator> --help
    )
    set_tests_properties(Phase9CrashGeneratorHelpTest PROPERTIES
        LABELS "debugging;phase9"
    )

    # Re-run the fixed lifetime path in every normal/sanitizer/TSan CTest job.
    # The intentionally crashing null-deref path is verified separately by GDB.
    add_test(
        NAME Phase9RepairedLifetimeRegressionTest
        COMMAND $<TARGET_FILE:crash_generator> use-after-free
    )
    set_tests_properties(Phase9RepairedLifetimeRegressionTest PROPERTIES
        LABELS "debugging;phase9;regression"
    )

    project_setup_test_dependencies()
    add_subdirectory(tests)
endif()

# Phase 11 release package metadata. The final annotated v1.0.0 tag is created
# only after the Phase 11 branch is merged and post-merge verification passes.
set(CPACK_PACKAGE_NAME "linux-voip-softphone")
set(CPACK_PACKAGE_VENDOR "linux-voip-softphone")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
include(CPack)
