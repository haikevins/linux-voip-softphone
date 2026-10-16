add_executable(crash_generator
    main.cpp
)

project_set_warnings(crash_generator)
project_enable_sanitizers(crash_generator)

# This target intentionally contains undefined behavior for the Phase 9
# debugging lab. Keep project-wide clang-tidy from treating the deliberate
# null-dereference scenario as a production-code defect.
set_property(TARGET crash_generator PROPERTY CXX_CLANG_TIDY "")
