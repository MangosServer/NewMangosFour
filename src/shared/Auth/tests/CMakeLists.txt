if(NOT WIN32)
    return()
endif()

add_executable(openssl_provider_release_layout_test
    OpenSSLProviderReleaseLayoutTest.cpp)

target_link_libraries(openssl_provider_release_layout_test
    PRIVATE
        shared
        mangos_openssl)

add_test(
    NAME openssl_provider_release_layout
    COMMAND openssl_provider_release_layout_test)

set_tests_properties(openssl_provider_release_layout PROPERTIES
    ENVIRONMENT "OPENSSL_MODULES="
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:openssl_provider_release_layout_test>")

if(WIN32)
    get_filename_component(MANGOS_SSL_ROOT "${OPENSSL_INCLUDE_DIR}" DIRECTORY)

    find_file(MANGOS_TEST_OPENSSL_CRYPTO_DLL
        NAMES libcrypto-3-x64.dll libcrypto-3.dll
        HINTS "${MANGOS_SSL_ROOT}" "${OPENSSL_ROOT_DIR}"
        PATH_SUFFIXES bin ""
        NO_DEFAULT_PATH)

    find_file(MANGOS_TEST_OPENSSL_LEGACY_DLL
        NAMES legacy.dll
        HINTS "${MANGOS_SSL_ROOT}" "${OPENSSL_ROOT_DIR}"
        PATH_SUFFIXES bin/ossl-modules bin ossl-modules lib/ossl-modules ""
        NO_DEFAULT_PATH)

    if(MANGOS_TEST_OPENSSL_CRYPTO_DLL)
        add_custom_command(TARGET openssl_provider_release_layout_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANGOS_TEST_OPENSSL_CRYPTO_DLL}"
                "$<TARGET_FILE_DIR:openssl_provider_release_layout_test>")
    endif()

    if(MANGOS_TEST_OPENSSL_LEGACY_DLL)
        add_custom_command(TARGET openssl_provider_release_layout_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:openssl_provider_release_layout_test>/ossl-modules"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANGOS_TEST_OPENSSL_LEGACY_DLL}"
                "$<TARGET_FILE_DIR:openssl_provider_release_layout_test>/ossl-modules/legacy.dll")
    else()
        message(WARNING
            "OpenSSL legacy.dll was not found; openssl_provider_release_layout will fail on Windows")
    endif()
endif()
