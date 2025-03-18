
add_library(asr_bytedancev2 bytedancev2.cpp)
target_compile_features(asr_bytedancev2 PUBLIC cxx_std_20)
set_target_properties(asr_bytedancev2 PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(asr_bytedancev2 PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(asr_bytedancev2 PUBLIC Boost::log)
target_link_libraries(asr_bytedancev2 PUBLIC common_tools)
find_package(OpenSSL REQUIRED)
target_link_libraries(asr_bytedancev2 PRIVATE OpenSSL::SSL)
target_link_libraries(asr_bytedancev2 PUBLIC Boost::json)

find_library(OGG_LIBRARY NAMES ogg)
target_link_libraries(asr_bytedancev2 PRIVATE ${OGG_LIBRARY})

add_library(asr base.cpp)
target_compile_features(asr PUBLIC cxx_std_20)
set_target_properties(asr PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(asr PUBLIC config_setting)
target_link_libraries(asr PUBLIC asr_bytedancev2)