
cmake_minimum_required(VERSION 3.7)

project(GLPrototypes
    VERSION 0.1.0
    LANGUAGES C
    )

set(_GLPROTOTYPES_CFLAGS -Wall -Wextra -pedantic -g)

find_package(glfw3 REQUIRED)

# Dual resolution multisample

add_subdirectory(drms)
