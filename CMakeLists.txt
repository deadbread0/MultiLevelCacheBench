cmake_minimum_required(VERSION 3.12)
project(CacheProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# ctest - start tests for all caches
enable_testing()

function(add_cache_test TARGET_NAME CACHE_DEFINE)
    add_executable(${TARGET_NAME}
        src/gtests.cpp
        src/all_caches.cpp
        src/some_func.cpp
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE ${CACHE_DEFINE})

    target_include_directories(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gmock
        Threads::Threads
    )

    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

add_cache_test(test_lirs LIRS_TEST)
add_cache_test(test_lfu  LFU_TEST)
add_cache_test(test_arc  ARC_TEST)
add_cache_test(test_2q   TWO_Q_TEST)

add_executable(bench_heats
    src/bench.cpp
    src/some_func.cpp
)

target_include_directories(bench_heats PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(bench_heats PRIVATE
    Threads::Threads
)

set_target_properties(bench_heats PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks"
)