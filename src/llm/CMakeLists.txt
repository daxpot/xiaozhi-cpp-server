file(GLOB_RECURSE cpp_files *.cpp)
add_library(llm ${cpp_files})

target_link_libraries(llm PRIVATE precomp)
target_link_libraries(llm PRIVATE common)