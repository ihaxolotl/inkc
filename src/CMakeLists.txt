set(ink_sources
    arena.c
    ast.c
    astgen.c
    common.c
    compile.c
    gc.c
    logging.c
    memory.c
    object.c
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
    add_library(ink SHARED ${ink_sources})
else()
    add_library(ink STATIC ${ink_sources})
endif()

target_include_directories(ink PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_compile_definitions(ink PRIVATE BUILDING_INKLIB)

set_target_properties(ink PROPERTIES
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_subdirectory(cli)
