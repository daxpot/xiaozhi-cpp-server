
add_library(asr_bytedancev2 bytedancev2.cpp)
target_link_libraries(asr_bytedancev2 PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(asr_bytedancev2 PUBLIC common_tools)
target_link_libraries(asr_bytedancev2 PRIVATE OpenSSL::SSL)
target_link_libraries(asr_bytedancev2 PUBLIC Boost::json)

target_link_libraries(asr_bytedancev2 PRIVATE ${OGG_LIBRARY})

add_library(asr base.cpp)
target_link_libraries(asr PUBLIC config_setting)
target_link_libraries(asr PUBLIC asr_bytedancev2)