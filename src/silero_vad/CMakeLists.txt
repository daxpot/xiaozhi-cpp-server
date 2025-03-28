add_library(vad vad.cpp silero_vad.cpp)

target_link_libraries(vad PRIVATE precomp)
target_link_libraries(vad PRIVATE ${OPUS_LIBRARY})
target_link_libraries(vad PRIVATE common)
target_link_libraries(vad PRIVATE ${ONNXRUNTIME_LIB})