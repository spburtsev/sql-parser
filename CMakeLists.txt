cmake_minimum_required(VERSION 3.25)
project(sql_parser LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER "clang")

add_compile_options(-Wall -Wextra -Wpedantic)

# Shared library
add_library(sqlparser SHARED
    parser.c
    lexer.c
    arena.c
)
target_include_directories(sqlparser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sqlparser PROPERTIES
    C_VISIBILITY_PRESET hidden
)

# Test executable
add_executable(sql_tests
    munit.c
    munit/munit.c
)
target_link_libraries(sql_tests PRIVATE sqlparser)
