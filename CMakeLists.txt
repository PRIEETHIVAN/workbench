cmake_minimum_required(VERSION 3.20)
project(UnifiedEngWorkbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /O2 /MP)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    set(WORKBENCH_BUILD_FLAGS "/W4 /O2 /MP")
else()
    add_compile_options(-Wall -Wextra -O2 -pthread)
    set(WORKBENCH_BUILD_FLAGS "-Wall -Wextra -O2 -pthread")
endif()

# ── Threading ─────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/fea/fem_solver.cpp
    src/viz/stl_parser.cpp
    src/viz/csv_parser.cpp
    src/parallel/batch_solver.cpp
    src/embedded/device_adapter.cpp
    src/backprop/backprop.cpp
)

add_executable(Launch ${SOURCES})
target_compile_definitions(Launch PRIVATE
    WORKBENCH_COMPILER_INFO="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    WORKBENCH_BUILD_FLAGS="${WORKBENCH_BUILD_FLAGS}"
)

target_include_directories(Launch PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server
)

target_link_libraries(Launch PRIVATE Threads::Threads)

# Avoid requiring MinGW runtime DLLs beside the executable in a ZIP release.
if(MINGW)
    target_link_options(Launch PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Windows: link ws2_32 for httplib socket support
if(WIN32)
    target_link_libraries(Launch PRIVATE ws2_32 mswsock)
endif()

# ── Copy frontend to build dir ─────────────────────────────────────────────────
add_custom_command(TARGET Launch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/frontend"
        "$<TARGET_FILE_DIR:Launch>/frontend"
    COMMENT "Copying frontend to build directory"
)

# Also copy data directory
add_custom_command(TARGET Launch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:Launch>/data"
    COMMENT "Copying data to build directory"
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS Launch DESTINATION .)
install(DIRECTORY src/frontend/ DESTINATION frontend)
install(DIRECTORY data/ DESTINATION data)

include(CTest)
if(BUILD_TESTING)
    add_executable(workbench_tests
        tests/test_workbench.cpp
        src/fea/fem_solver.cpp
        src/viz/stl_parser.cpp
        src/viz/csv_parser.cpp
        src/parallel/batch_solver.cpp
        src/embedded/device_adapter.cpp
        src/backprop/backprop.cpp
    )
    target_include_directories(workbench_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(workbench_tests PRIVATE Threads::Threads)
    target_compile_definitions(workbench_tests PRIVATE
        WORKBENCH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(WIN32)
        target_link_libraries(workbench_tests PRIVATE ws2_32 mswsock)
    endif()
    if(MINGW)
        target_link_options(workbench_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME workbench_core COMMAND workbench_tests)
    set_tests_properties(workbench_core PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# ── Build info ────────────────────────────────────────────────────────────────
message(STATUS "=== Unified Engineering Workbench ===")
message(STATUS "C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "Threads      : ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "=====================================")
