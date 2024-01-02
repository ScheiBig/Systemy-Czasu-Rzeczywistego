cmake_minimum_required(VERSION 3.0.0)
project(Zadanie1 VERSION 0.1.0 LANGUAGES C)

add_compile_options(
    "-pedantic"
    "-Wall"
    "-Werror"
    # "-fmax-errors=16"
    "-pthread"
)

include(CTest)
enable_testing()

add_executable(Zadanie1 zadanie1/main.c)

target_link_libraries(Zadanie1
    "pthread"
    "rt"
    "m"
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
