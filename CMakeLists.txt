cmake_minimum_required(VERSION 3.16)

project(
    xiaozhi-server
    VERSION 0.1
    DESCRIPTION "xiaozhi cpp server"
    LANGUAGES CXX
)

add_subdirectory(extern/yaml-cpp)
add_subdirectory(extern/json)
add_subdirectory(apps)
add_subdirectory(src)

include(CTest)
add_subdirectory(extern/Catch2)
add_subdirectory(tests)