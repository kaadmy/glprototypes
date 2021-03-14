
add_executable(drms
    ../glad/glad.c
    main.c
    )

set_source_files_properties(../glad/glad.c
    PROPERTIES
    COMPILE_FLAGS -Wno-pedantic
    )

set_target_properties(drms
    PROPERTIES
    C_STANDARD 99
    )

target_compile_options(drms
    PUBLIC
    ${_GLPROTOTYPES_CFLAGS}
    )

target_link_libraries(drms
    m
    dl
    glfw
    )
