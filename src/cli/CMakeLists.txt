add_executable(ink_cli)

target_sources(ink_cli PRIVATE
    main.c
    option.c
)

target_include_directories(ink_cli PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(ink_cli PRIVATE
    ink
    $<$<NOT:$<PLATFORM_ID:Windows>>:m>
)

set_target_properties(ink_cli PROPERTIES OUTPUT_NAME inkc)
