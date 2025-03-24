add_library(tts_bytedancev3 bytedancev3.cpp)
target_link_libraries(tts_bytedancev3 PUBLIC common_tools)
target_link_libraries(tts_bytedancev3 PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(tts_bytedancev3 PUBLIC common_request)

add_library(tts_edge edge.cpp)
target_link_libraries(tts_edge PUBLIC common_tools)
target_link_libraries(tts_edge PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(tts_edge PUBLIC common_request)

add_library(tts base.cpp)
target_link_libraries(tts PUBLIC config_setting)
target_link_libraries(tts PUBLIC tts_bytedancev3)
target_link_libraries(tts PUBLIC tts_edge)