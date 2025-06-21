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

set(USE_SANITIZER
    "" CACHE STRING
    "Compile with a sanitizer. Options are: Address, Memory, Leak, Undefined, Thread, 'Address;Undefined'"
)

if(USE_SANITIZER MATCHES "([Aa]ddress)")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
elseif(USE_SANITIZER MATCHES "([Tt]hread)")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
elseif(USE_SANITIZER MATCHES "([Uu]ndefined)")
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
elseif(USE_SANITIZER MATCHES "([Ll]eak)")
    add_compile_options(-fsanitize=leak)
    add_link_options(-fsanitize=leak)
elseif(USE_SANITIZER MATCHES "([Mm]emory)")
    add_compile_options(-fsanitize=memory)
    add_link_options(-fsanitize=memory)
elseif(USE_SANITIZER MATCHES "([Aa]ddress),([Uu]ndefined)")
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
elseif(NOT "${USE_SANITIZER}" STREQUAL "" AND NOT "${USE_SANITIZER}" STREQUAL "None")
    message(FATAL_ERROR "Unsupported value of USE_SANITIZER: ${USE_SANITIZER}")
endif()

# Set the possible values of build type for cmake-gui/ccmake
set_property(CACHE USE_SANITIZER
    PROPERTY STRINGS "None" "Address" "Thread" "Undefined" "Leak" "Memory" "Address,Undefined"
)

add_compile_options(
    -Wall
    -Wextra
    -Wconversion
    -Wpedantic
    -Wno-unused-parameter
)

add_subdirectory(src)
add_subdirectory(src/cli)
