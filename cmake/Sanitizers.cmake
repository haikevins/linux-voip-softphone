function(project_enable_sanitizers target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN)
            message(WARNING "Sanitizer configuration in this project targets GCC/Clang.")
        endif()
        return()
    endif()

    if(ENABLE_ASAN AND ENABLE_TSAN)
        message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer cannot be enabled together.")
    endif()

    set(sanitizers "")

    if(ENABLE_ASAN)
        list(APPEND sanitizers "address")
    endif()

    if(ENABLE_UBSAN)
        list(APPEND sanitizers "undefined")
    endif()

    if(ENABLE_TSAN)
        list(APPEND sanitizers "thread")
    endif()

    if(sanitizers)
        list(JOIN sanitizers "," sanitizer_list)
        target_compile_options(${target} PRIVATE
            -fsanitize=${sanitizer_list}
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=${sanitizer_list}
            -fno-omit-frame-pointer
        )
    endif()
endfunction()
