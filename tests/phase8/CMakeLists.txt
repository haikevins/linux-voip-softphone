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

function(phase8_add_test target source ctest_name)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    )
    target_link_libraries(${target} PRIVATE Threads::Threads)
    add_test(NAME ${ctest_name} COMMAND ${target})
    set_tests_properties(${ctest_name} PROPERTIES TIMEOUT 30)
endfunction()

phase8_add_test(phase8_thread_safe_queue_tests
    test_thread_safe_queue.cpp Phase8ThreadSafeQueueTest)
phase8_add_test(phase8_serial_task_worker_tests
    test_serial_task_worker.cpp Phase8SerialTaskWorkerTest)
phase8_add_test(phase8_thread_ownership_tests
    test_thread_ownership.cpp Phase8ThreadOwnershipTest)
