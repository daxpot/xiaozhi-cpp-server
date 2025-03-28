file(GLOB_RECURSE cpp_files *.cpp)
add_library(tts ${cpp_files})

target_link_libraries(tts PRIVATE precomp)
target_link_libraries(tts PRIVATE common)
target_link_libraries(tts PRIVATE ${OPUS_LIBRARY})