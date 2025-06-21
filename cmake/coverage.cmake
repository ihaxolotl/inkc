include(CMakeParseArguments)

# Check prereqs
find_program(GCOV_PATH gcov)
find_program(GCOVR_PATH gcovr)

if(NOT GCOV_PATH)
    message(FATAL_ERROR "gcov not found! Aborting...")
endif() # NOT GCOV_PATH

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
    if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 3)
        message(FATAL_ERROR "Clang version must be 3.0.0 or greater! Aborting...")
    endif()
endif()

set(COVERAGE_COMPILER_FLAGS
    "-g -fprofile-arcs -ftest-coverage"
    CACHE INTERNAL ""
)

set(CMAKE_C_FLAGS_COVERAGE
    ${COVERAGE_COMPILER_FLAGS}
    CACHE STRING "Flags used by the C compiler during coverage builds."
    FORCE
)

set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
    ""
    CACHE STRING "Flags used for linking binaries during coverage builds."
    FORCE
)

set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
    ""
    CACHE STRING "Flags used by the shared libraries linker during coverage builds."
    FORCE
)

mark_as_advanced(
    CMAKE_C_FLAGS_COVERAGE
    CMAKE_EXE_LINKER_FLAGS_COVERAGE
    CMAKE_SHARED_LINKER_FLAGS_COVERAGE
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Code coverage results with an optimised (non-Debug) build may be misleading")
endif() # NOT CMAKE_BUILD_TYPE STREQUAL "Debug"

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    link_libraries(gcov)
endif()

# Defines a target for running and collection code coverage information
# Builds dependencies, runs the given executable and outputs reports.
# NOTE! The executable should always have a ZERO as exit code otherwise
# the coverage generation will not complete.
#
# setup_target_for_coverage_gcovr_xml(
#     NAME ctest_coverage                    # New target name
#     EXECUTABLE ctest -j ${PROCESSOR_COUNT} # Executable in PROJECT_BINARY_DIR
#     DEPENDENCIES executable_target         # Dependencies to build first
#     BASE_DIRECTORY "../"                   # Base directory for report
#                                            #  (defaults to PROJECT_SOURCE_DIR)
#     EXCLUDE "src/dir1/*" "src/dir2/*"      # Patterns to exclude (can be relative
#                                            #  to BASE_DIRECTORY, with CMake 3.4+)
# )
function(setup_target_for_coverage_gcovr_xml)

    set(options NONE)
    set(oneValueArgs BASE_DIRECTORY NAME)
    set(multiValueArgs EXCLUDE EXECUTABLE EXECUTABLE_ARGS DEPENDENCIES)
    cmake_parse_arguments(Coverage "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT GCOVR_PATH)
        message(FATAL_ERROR "gcovr not found! Aborting...")
    endif() # NOT GCOVR_PATH

    # Set base directory (as absolute path), or default to PROJECT_SOURCE_DIR
    if(${Coverage_BASE_DIRECTORY})
        get_filename_component(BASEDIR ${Coverage_BASE_DIRECTORY} ABSOLUTE)
    else()
        set(BASEDIR ${PROJECT_SOURCE_DIR})
    endif()

    # Collect excludes (CMake 3.4+: Also compute absolute paths)
    set(GCOVR_EXCLUDES "")
    foreach(EXCLUDE ${Coverage_EXCLUDE} ${COVERAGE_EXCLUDES} ${COVERAGE_GCOVR_EXCLUDES})
        if(CMAKE_VERSION VERSION_GREATER 3.4)
            get_filename_component(EXCLUDE ${EXCLUDE} ABSOLUTE BASE_DIR ${BASEDIR})
        endif()
        list(APPEND GCOVR_EXCLUDES "${EXCLUDE}")
    endforeach()
    list(REMOVE_DUPLICATES GCOVR_EXCLUDES)

    # Combine excludes to several -e arguments
    set(GCOVR_EXCLUDE_ARGS "")
    foreach(EXCLUDE ${GCOVR_EXCLUDES})
        list(APPEND GCOVR_EXCLUDE_ARGS "-e")
        list(APPEND GCOVR_EXCLUDE_ARGS "${EXCLUDE}")
    endforeach()

    add_custom_target(${Coverage_NAME}
        # Run tests
        ${Coverage_EXECUTABLE} ${Coverage_EXECUTABLE_ARGS}

        # Running gcovr
        COMMAND ${GCOVR_PATH} --xml
        -r ${BASEDIR} ${GCOVR_EXCLUDE_ARGS}
        --object-directory=${PROJECT_BINARY_DIR}
        -o ${Coverage_NAME}.xml
        BYPRODUCTS ${Coverage_NAME}.xml
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        DEPENDS ${Coverage_DEPENDENCIES}
        VERBATIM # Protect arguments to commands
        COMMENT "Running gcovr to produce Cobertura code coverage report."
    )

    # Show info where to find the report
    add_custom_command(TARGET ${Coverage_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMENT "Cobertura code coverage report saved in ${Coverage_NAME}.xml."
    )
endfunction() # setup_target_for_coverage_gcovr_xml

# Defines a target for running and collection code coverage information
# Builds dependencies, runs the given executable and outputs reports.
# NOTE! The executable should always have a ZERO as exit code otherwise
# the coverage generation will not complete.
#
# setup_target_for_coverage_gcovr_html(
#     NAME ctest_coverage                    # New target name
#     EXECUTABLE ctest -j ${PROCESSOR_COUNT} # Executable in PROJECT_BINARY_DIR
#     DEPENDENCIES executable_target         # Dependencies to build first
#     BASE_DIRECTORY "../"                   # Base directory for report
#                                            #  (defaults to PROJECT_SOURCE_DIR)
#     EXCLUDE "src/dir1/*" "src/dir2/*"      # Patterns to exclude (can be relative
#                                            #  to BASE_DIRECTORY, with CMake 3.4+)
# )
function(setup_target_for_coverage_gcovr_html)

    set(options NONE)
    set(oneValueArgs BASE_DIRECTORY NAME)
    set(multiValueArgs EXCLUDE EXECUTABLE EXECUTABLE_ARGS DEPENDENCIES)
    cmake_parse_arguments(Coverage "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT GCOVR_PATH)
        message(FATAL_ERROR "gcovr not found! Aborting...")
    endif() # NOT GCOVR_PATH

    # Set base directory (as absolute path), or default to PROJECT_SOURCE_DIR
    if(${Coverage_BASE_DIRECTORY})
        get_filename_component(BASEDIR ${Coverage_BASE_DIRECTORY} ABSOLUTE)
    else()
        set(BASEDIR ${PROJECT_SOURCE_DIR})
    endif()

    # Collect excludes (CMake 3.4+: Also compute absolute paths)
    set(GCOVR_EXCLUDES "")
    foreach(EXCLUDE ${Coverage_EXCLUDE} ${COVERAGE_EXCLUDES} ${COVERAGE_GCOVR_EXCLUDES})
        if(CMAKE_VERSION VERSION_GREATER 3.4)
            get_filename_component(EXCLUDE ${EXCLUDE} ABSOLUTE BASE_DIR ${BASEDIR})
        endif()
        list(APPEND GCOVR_EXCLUDES "${EXCLUDE}")
    endforeach()
    list(REMOVE_DUPLICATES GCOVR_EXCLUDES)

    # Combine excludes to several -e arguments
    set(GCOVR_EXCLUDE_ARGS "")
    foreach(EXCLUDE ${GCOVR_EXCLUDES})
        list(APPEND GCOVR_EXCLUDE_ARGS "-e")
        list(APPEND GCOVR_EXCLUDE_ARGS "${EXCLUDE}")
    endforeach()

    add_custom_target(${Coverage_NAME}
        # Run tests
        ${Coverage_EXECUTABLE} ${Coverage_EXECUTABLE_ARGS}

        # Create folder
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/${Coverage_NAME}

        # Running gcovr
        COMMAND ${GCOVR_PATH} --html --html-details
        -r ${BASEDIR} ${GCOVR_EXCLUDE_ARGS}
        --object-directory=${PROJECT_BINARY_DIR}
        -o ${Coverage_NAME}/index.html

        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        DEPENDS ${Coverage_DEPENDENCIES}
        VERBATIM # Protect arguments to commands
        COMMENT "Running gcovr to produce HTML code coverage report."
    )
    set_target_properties(${Coverage_NAME} PROPERTIES
        ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/${Coverage_NAME})

    # Show info where to find the report
    add_custom_command(TARGET ${Coverage_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMENT "Open ./${Coverage_NAME}/index.html in your browser to view the coverage report."
    )

endfunction() # setup_target_for_coverage_gcovr_html

function(append_coverage_compiler_flags)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}" PARENT_SCOPE)
    message(STATUS "Appending code coverage compiler flags: ${COVERAGE_COMPILER_FLAGS}")
endfunction() # append_coverage_compiler_flags
