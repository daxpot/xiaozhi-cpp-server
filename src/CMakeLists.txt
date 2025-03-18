function(add_target_with_config target_name)
    add_library(${target_name} ${ARGN} ${HEADER_LIST})
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()

#config logger
add_target_with_config(config_logger config/logger.cpp)
target_link_libraries(config_logger PUBLIC Boost::log Boost::log_setup)

#common tools
add_target_with_config(common_tools common/tools.cpp)
target_link_libraries(common_tools PRIVATE Boost::iostreams)
target_link_libraries(common_tools PRIVATE Boost::random)

#common request
add_target_with_config(common_request common/request.cpp)
find_package(OpenSSL REQUIRED)
target_link_libraries(common_request PUBLIC OpenSSL::SSL)
target_link_libraries(common_request PUBLIC Boost::log)
target_link_libraries(common_request PUBLIC Boost::json)

#config setting
add_target_with_config(config_setting config/setting.cpp)
target_link_libraries(config_setting PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(config_setting PUBLIC Boost::log)

add_subdirectory(asr)
add_subdirectory(tts)
add_subdirectory(llm)
add_subdirectory(silero_vad)

#connection library
add_target_with_config(connection connection.cpp)
target_link_libraries(connection PUBLIC config_setting)
target_link_libraries(connection PUBLIC common_tools)
target_link_libraries(connection PUBLIC vad)
target_link_libraries(connection PUBLIC asr)
target_link_libraries(connection PUBLIC tts)
target_link_libraries(connection PUBLIC llm)

#server library
add_target_with_config(server server.cpp)
target_link_libraries(server PUBLIC connection)

