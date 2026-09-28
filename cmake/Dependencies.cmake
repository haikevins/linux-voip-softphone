function(project_setup_test_dependencies)
    find_package(GTest REQUIRED)
endfunction()

function(project_setup_pjsip_dependency)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(
        PJSIP
        REQUIRED
        "libpjproject>=2.17"
    )

    # PJPROJECT's default POSIX build installs static archives. For static
    # consumers, pkg-config keeps transitive dependencies (OpenSSL, ALSA,
    # bundled codecs, SRTP, etc.) in Libs.private, exposed by --static.
    # FindPkgConfig provides the corresponding PJSIP_STATIC_* variables.
    if(NOT TARGET PJSIP::PJSIP)
        add_library(PJSIP::PJSIP INTERFACE IMPORTED GLOBAL)
        set_target_properties(
            PJSIP::PJSIP
            PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${PJSIP_INCLUDE_DIRS}"
                INTERFACE_COMPILE_OPTIONS "${PJSIP_CFLAGS_OTHER}"
                INTERFACE_LINK_DIRECTORIES "${PJSIP_STATIC_LIBRARY_DIRS}"
                INTERFACE_LINK_LIBRARIES "${PJSIP_STATIC_LIBRARIES}"
                INTERFACE_LINK_OPTIONS "${PJSIP_STATIC_LDFLAGS_OTHER}"
        )
    endif()

    message(STATUS "PJSIP support enabled: libpjproject ${PJSIP_VERSION}")
endfunction()
