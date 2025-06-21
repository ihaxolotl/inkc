cmake_minimum_required(VERSION 3.17)

project(Ink
    VERSION 0.1
    DESCRIPTION "Ink narrative scripting language in C99"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$<0:>${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "$<0:>${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "$<0:>${CMAKE_BINARY_DIR}/lib")

include(CMakeDependentOption)
include(CTest)
include(cmake/sanitizers.cmake)

if (ENABLE_COVERAGE)
    include(cmake/coverage.cmake)
    append_coverage_compiler_flags()
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wconversion
    -Wpedantic
    -Wno-unused-parameter
)

add_subdirectory(src)

if(ENABLE_TESTING)
    include(cmake/cmocka.cmake)
    add_executable(ink_tests)

    target_sources(ink_tests PRIVATE testing/main.c)

    target_include_directories(ink_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ink_tests PRIVATE ink_static m cmocka)
    set_target_properties(ink_tests PROPERTIES OUTPUT_NAME ink-tests)

    register_cmocka_test(Ink.Test ink_tests)
endif()

if(ENABLE_COVERAGE)
    setup_target_for_coverage_gcovr_html(
        NAME coverage-html
        EXECUTABLE ctest
        DEPENDENCIES ink_tests
    )

    add_custom_target(coverage DEPENDS coverage-html)
endif()
