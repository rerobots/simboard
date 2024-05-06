cmake_minimum_required(VERSION 3.5)
project(repl-simavr C)

add_executable(${PROJECT_NAME}
    repl.c
)
target_compile_options(${PROJECT_NAME} PUBLIC
    -Wall
)

target_include_directories(${PROJECT_NAME} PUBLIC
    "/usr/local/include"
)
target_link_directories(${PROJECT_NAME} PUBLIC
    "/usr/local/lib"
)
target_link_libraries(${PROJECT_NAME}
    simavr
)
