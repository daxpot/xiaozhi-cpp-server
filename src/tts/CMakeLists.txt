file(GLOB_RECURSE cpp_files *.cpp)
add_library(tts ${cpp_files})

target_link_libraries(tts PUBLIC config_setting)
target_link_libraries(tts PUBLIC common_request)
target_link_libraries(tts PUBLIC common_tools)
target_link_libraries(tts PUBLIC yaml-cpp::yaml-cpp)
