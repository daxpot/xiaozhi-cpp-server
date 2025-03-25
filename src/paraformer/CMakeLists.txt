file(GLOB_RECURSE cpp_files *.cpp)
add_library(paraformer ${cpp_files})

target_link_libraries(paraformer PUBLIC Boost::log)
target_link_libraries(paraformer PUBLIC Boost::json)
target_link_libraries(paraformer PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(paraformer PUBLIC ${ONNXRUNTIME_LIB})
target_link_libraries(paraformer PUBLIC kaldi-native-fbank-core)