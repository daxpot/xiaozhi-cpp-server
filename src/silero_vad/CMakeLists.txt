add_library(silero_vad silero_vad.cpp)
target_compile_features(silero_vad PUBLIC cxx_std_20)
set_target_properties(silero_vad PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(silero_vad PUBLIC config_setting)
target_include_directories(silero_vad PUBLIC "${PROJECT_SOURCE_DIR}/extern/onnxruntime-linux-x64-1.21.0/include")
target_link_libraries(silero_vad PUBLIC "${PROJECT_SOURCE_DIR}/extern/onnxruntime-linux-x64-1.21.0/lib/libonnxruntime.so")


file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/extern/onnxruntime-linux-x64-1.21.0/include/*.h")
source_group(
    TREE "${PROJECT_SOURCE_DIR}/extern/onnxruntime-linux-x64-1.21.0/include/"
    PREFIX "Header Files"
    FILES ${HEADER_LIST}
)

add_library(vad vad.cpp)
target_compile_features(vad PUBLIC cxx_std_20)
set_target_properties(vad PROPERTIES CXX_EXTENSIONS OFF)
find_library(OPUS_LIBRARY NAMES opus)
target_link_libraries(vad PRIVATE ${OPUS_LIBRARY})
target_link_libraries(vad PRIVATE silero_vad)