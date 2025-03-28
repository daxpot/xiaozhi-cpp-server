file(GLOB_RECURSE cpp_files *.cpp)
add_library(common ${cpp_files})

target_link_libraries(common PRIVATE precomp)
target_link_libraries(common PRIVATE OpenSSL::SSL)
target_link_libraries(common PRIVATE ${OPUS_LIBRARY})
target_link_libraries(common PRIVATE Boost::iostreams)
target_link_libraries(common PRIVATE Boost::random)
target_link_libraries(common PRIVATE Boost::log_setup)