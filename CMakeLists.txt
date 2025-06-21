cmake_minimum_required(VERSION 3.17)

project(Ink
    VERSION 0.1
    DESCRIPTION "Ink narrative scripting language in C99"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -Wall
    -Wextra
    -Wconversion
    -Wpedantic
    -Wno-unused-parameter
)

add_subdirectory(src)
