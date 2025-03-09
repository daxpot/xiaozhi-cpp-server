cmake_minimum_required(VERSION 3.16)

project(
    xiaozhi-server
    VERSION 0.1
    DESCRIPTION "xiaozhi cpp server"
    LANGUAGES CXX
)

include_directories("${PROJECT_SOURCE_DIR}/include")
find_package(Boost COMPONENTS log log_setup REQUIRED)

add_subdirectory(extern/yaml-cpp)
add_subdirectory(extern/json)
add_subdirectory(apps)
add_subdirectory(src)

include(CTest)
add_subdirectory(extern/Catch2)
add_subdirectory(tests)

file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/xz-cpp-server/**/*.h")
source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${HEADER_LIST}
)