set(INK_LIB_SOURCES
    arena.c
    ast.c
    astgen.c
    common.c
    compile.c
    logging.c
    memory.c
    parse.c
    scanner.c
    source.c
    story.c
    stream.c
    symtab.c
    token.c
)

option(BUILD_SHARED "Build shared library" ON)

if(BUILD_SHARED)
    add_library(ink SHARED ${INK_LIB_SOURCES})
else()
    add_library(ink STATIC ${INK_LIB_SOURCES})
endif()

target_include_directories(ink PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

add_subdirectory(cli)
