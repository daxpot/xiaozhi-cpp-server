
add_library(asr_doubao doubao.cpp)
target_compile_features(asr_doubao PUBLIC cxx_std_20)
set_target_properties(asr_doubao PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(asr_doubao PUBLIC config_setting)
target_link_libraries(asr_doubao PUBLIC common_tools)
find_package(OpenSSL REQUIRED)
target_link_libraries(asr_doubao PRIVATE OpenSSL::SSL)
target_link_libraries(asr_doubao PUBLIC Boost::json)

find_library(OGG_LIBRARY NAMES ogg)
target_link_libraries(asr_doubao PRIVATE ${OGG_LIBRARY})