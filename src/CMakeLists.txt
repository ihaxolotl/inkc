###############
# Ink library #
###############

add_library(ink STATIC)

target_sources(ink PRIVATE
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

target_include_directories(ink PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

####################
# Ink compiler CLI #
####################

add_executable(inkc)

target_sources(inkc PRIVATE
    cli/main.c
    cli/option.c
)

target_include_directories(inkc PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(inkc PRIVATE ink -lm)
