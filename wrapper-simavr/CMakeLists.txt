cmake_minimum_required(VERSION 3.5)
project(repl-simavr C)

find_package(libwebsockets CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
    repl.c
)
target_compile_options(${PROJECT_NAME} PUBLIC
    -Wall
)

# To use simavr
target_include_directories(${PROJECT_NAME} PUBLIC
    "/usr/local/include"
)
target_link_directories(${PROJECT_NAME} PUBLIC
    "/usr/local/lib"
)
target_link_libraries(${PROJECT_NAME}
    simavr
)

# To use libwebsockets
if (websockets_shared)
    target_link_libraries(${PROJECT_NAME}
        websockets_shared
        ${LIBWEBSOCKETS_DEP_LIBS}
    )
    add_dependencies(${PROJECT_NAME} websockets_shared)
else()
    target_link_libraries(${PROJECT_NAME}
        websockets
        ${LIBWEBSOCKETS_DEP_LIBS}
    )
endif()
