add_library(vad vad.cpp silero_vad.cpp)

target_link_libraries(vad PRIVATE ${OPUS_LIBRARY})
target_link_libraries(vad PRIVATE common_tools)
target_link_libraries(vad PUBLIC config_setting)
target_link_libraries(vad PUBLIC ${ONNXRUNTIME_LIB})