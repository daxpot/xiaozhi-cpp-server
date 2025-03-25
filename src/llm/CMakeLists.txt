file(GLOB_RECURSE cpp_files *.cpp)
add_library(llm ${cpp_files})

target_link_libraries(llm PUBLIC config_setting)
target_link_libraries(llm PUBLIC common_request)