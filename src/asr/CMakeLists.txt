file(GLOB_RECURSE cpp_files *.cpp)
add_library(asr ${cpp_files})

target_link_libraries(asr PRIVATE precomp)
target_link_libraries(asr PRIVATE common)
target_link_libraries(asr PRIVATE OpenSSL::SSL)
target_link_libraries(asr PRIVATE ${OGG_LIBRARY})
target_link_libraries(asr PRIVATE ${OPUS_LIBRARY})
target_link_libraries(asr PRIVATE paraformer)
