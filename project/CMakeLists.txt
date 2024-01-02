cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_STANDARD 11)

project(SCR LANGUAGES C)

add_compile_options(
    "-pedantic"
    "-Wall"
    "-Werror"
    "-fmax-errors=16"
    "-fdiagnostics-color=always"

    "-pthread"
)

add_executable(SCR
    main.c
    logger.c
)

target_link_libraries(SCR
    "pthread"
    "rt"
)
