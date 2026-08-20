cmake_minimum_required(VERSION 3.24)
project(netwar VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NETWAR_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(sim)
add_subdirectory(cli)

if (NETWAR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
