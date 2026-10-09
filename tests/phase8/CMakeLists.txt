cmake_minimum_required(VERSION 3.16)
project(voip_phase8_queue_tests LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

add_executable(phase8_thread_safe_queue_tests
    test_thread_safe_queue.cpp
)
target_compile_features(phase8_thread_safe_queue_tests PRIVATE cxx_std_20)
target_include_directories(phase8_thread_safe_queue_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
)
target_link_libraries(phase8_thread_safe_queue_tests PRIVATE Threads::Threads)

add_executable(phase8_serial_task_worker_tests
    test_serial_task_worker.cpp
)
target_compile_features(phase8_serial_task_worker_tests PRIVATE cxx_std_20)
target_include_directories(phase8_serial_task_worker_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
)
target_link_libraries(phase8_serial_task_worker_tests PRIVATE Threads::Threads)

if(BUILD_TESTING)
    add_test(NAME Phase8ThreadSafeQueueTest
        COMMAND phase8_thread_safe_queue_tests)
    set_tests_properties(Phase8ThreadSafeQueueTest PROPERTIES TIMEOUT 30)
    add_test(NAME Phase8SerialTaskWorkerTest
        COMMAND phase8_serial_task_worker_tests)
    set_tests_properties(Phase8SerialTaskWorkerTest PROPERTIES TIMEOUT 30)
endif()
