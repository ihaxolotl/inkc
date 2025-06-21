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

add_library(ink_static STATIC)

target_sources(ink_static PRIVATE ${INK_LIB_SOURCES})

target_include_directories(ink_static PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(ink_static PROPERTIES OUTPUT_NAME ink)

add_library(ink_shared SHARED)

target_sources(ink_shared PRIVATE ${INK_LIB_SOURCES})

target_include_directories(ink_shared PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(ink_shared PROPERTIES OUTPUT_NAME ink)

add_subdirectory(cli)
