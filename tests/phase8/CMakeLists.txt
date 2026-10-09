# This test suite may be built independently or via tests/CMakeLists.txt.
# Do not invoke a second project()/include(CTest) inside the parent project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.16)
    project(voip_phase8_queue_tests LANGUAGES CXX)
    include(CTest)
endif()

if(DEFINED BUILD_TESTING AND NOT BUILD_TESTING)
    return()
endif()

find_package(Threads REQUIRED)

# TSan is opt-in and deliberately limited to the standalone concurrency
# suite. It must not partially instrument a canonical Qt/PJSIP build or be
# combined with its ASan/UBSan configuration.
option(PHASE8_ENABLE_TSAN "Build standalone Phase 8 tests with ThreadSanitizer" OFF)
if(PHASE8_ENABLE_TSAN)
    if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        message(FATAL_ERROR
            "PHASE8_ENABLE_TSAN requires standalone -S tests/phase8; "
            "do not enable it for the canonical Qt/PJSIP build")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        message(FATAL_ERROR "Phase 8 TSan requires a GCC or Clang C++ compiler")
    endif()
    string(CONCAT _phase8_existing_flags "${CMAKE_CXX_FLAGS} "
        "${CMAKE_EXE_LINKER_FLAGS} " "${CMAKE_SHARED_LINKER_FLAGS}")
    if(_phase8_existing_flags MATCHES "-fsanitize=[^ ]*(address|undefined)")
        message(FATAL_ERROR "TSan must use a separate build without ASan/UBSan")
    endif()
    # The compiler probe must link libtsan as well as instrument its object.
    # check_cxx_compiler_flag() alone omits that link flag and falsely fails.
    include(CheckCXXCompilerFlag)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=thread)
    check_cxx_compiler_flag("-fsanitize=thread" PHASE8_TSAN_COMPILER_SUPPORTED)
    cmake_pop_check_state()
    if(NOT PHASE8_TSAN_COMPILER_SUPPORTED)
        message(FATAL_ERROR
            "Compiler/linker cannot build -fsanitize=thread executables")
    endif()
endif()

function(phase8_add_test target source ctest_name)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    )
    target_link_libraries(${target} PRIVATE Threads::Threads)
    if(PHASE8_ENABLE_TSAN)
        target_compile_options(${target} PRIVATE
            -fsanitize=thread -fno-omit-frame-pointer -O1 -g)
        target_link_options(${target} PRIVATE -fsanitize=thread)
    endif()
    add_test(NAME ${ctest_name} COMMAND ${target})
    set_tests_properties(${ctest_name} PROPERTIES TIMEOUT 30)
endfunction()

phase8_add_test(phase8_thread_safe_queue_tests
    test_thread_safe_queue.cpp Phase8ThreadSafeQueueTest)
phase8_add_test(phase8_serial_task_worker_tests
    test_serial_task_worker.cpp Phase8SerialTaskWorkerTest)
phase8_add_test(phase8_thread_ownership_tests
    test_thread_ownership.cpp Phase8ThreadOwnershipTest)
phase8_add_test(phase8_mpmc_stress_tests
    test_mpmc_stress.cpp Phase8MpmcStressTest)
# Pure C++ polling policy shared with production PJSIP wait_for_call_cleanup().
# The deterministic test never creates an Endpoint or opens a network socket.
phase8_add_test(phase8_pjsip_call_slot_cleanup_tests
    test_pjsip_call_slot_cleanup.cpp Phase8PjsipCallSlotCleanupTest)

# Generic callback-in-flight accounting model; no PJSUA2 calls and no
# production callback hook is wired in this commit.
phase8_add_test(phase8_pjsip_callback_activity_tests
    test_pjsip_callback_activity.cpp Phase8PjsipCallbackActivityTest)

# Leave headroom for slower, instrumented contention runs.
set_tests_properties(Phase8MpmcStressTest PROPERTIES TIMEOUT 90)

# Commit 12: inspect the actual production SIP callback entry points and
# teardown ordering. Run this Python source audit without TSan: it neither
# links nor instruments PJSUA2 and must not inflate the TSan six-C++-test gate.
if(NOT PHASE8_ENABLE_TSAN)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME Phase8PjsipProductionAuditTest
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_pjsip_production_lifetime_audit.py")
    set_tests_properties(Phase8PjsipProductionAuditTest PROPERTIES TIMEOUT 30)
endif()
