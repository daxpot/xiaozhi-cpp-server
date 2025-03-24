add_library(silero_vad silero_vad.cpp)

target_link_libraries(silero_vad PUBLIC config_setting)
target_link_libraries(silero_vad PUBLIC ${ONNXRUNTIME_LIB})

add_library(vad vad.cpp)
target_link_libraries(vad PRIVATE ${OPUS_LIBRARY})
target_link_libraries(vad PRIVATE silero_vad)
target_link_libraries(vad PRIVATE common_tools)