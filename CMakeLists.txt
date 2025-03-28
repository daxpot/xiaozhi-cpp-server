cmake_minimum_required(VERSION 3.16)

project(
    xiaozhi-server
    VERSION 0.1
    DESCRIPTION "xiaozhi cpp server"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)          # 默认使用 C++20
set(CMAKE_CXX_EXTENSIONS OFF)       # 关闭编译器扩展（如 GNU 扩展）
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 强制要求 C++20，避免降级

include_directories("${PROJECT_SOURCE_DIR}/include")
find_package(Boost COMPONENTS log log_setup iostreams random json REQUIRED)
find_package(OpenSSL REQUIRED)
find_library(OGG_LIBRARY NAMES ogg)
find_library(OPUS_LIBRARY NAMES opus)
find_library(ONNXRUNTIME_LIB NAMES onnxruntime PATHS "extern/onnxruntime-linux-x64-1.21.0/lib"  NO_DEFAULT_PATH)
include_directories("${PROJECT_SOURCE_DIR}/extern/onnxruntime-linux-x64-1.21.0/include")

add_subdirectory(extern/kaldi-native-fbank)
include_directories("${PROJECT_SOURCE_DIR}/extern/kaldi-native-fbank")
add_subdirectory(extern/yaml-cpp)

add_library(precomp INTERFACE)
target_link_libraries(precomp INTERFACE Boost::log Boost::json yaml-cpp::yaml-cpp)
target_precompile_headers(precomp INTERFACE "${PROJECT_SOURCE_DIR}/include/xz-cpp-server/precomp.h")

add_subdirectory(apps)
add_subdirectory(src)

include(CTest)
add_subdirectory(extern/Catch2)
add_subdirectory(tests)

file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/xz-cpp-server/**/*.h" "${PROJECT_SOURCE_DIR}/include/xz-cpp-server/**/*.hpp")
source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${HEADER_LIST}
)