#config logger
add_library(config_logger config/logger.cpp)
target_link_libraries(config_logger PUBLIC Boost::log Boost::log_setup)

#common tools
add_library(common_tools common/tools.cpp)
target_link_libraries(common_tools PRIVATE Boost::iostreams)
target_link_libraries(common_tools PRIVATE Boost::random)
target_link_libraries(common_tools PUBLIC Boost::log)
target_link_libraries(common_tools PRIVATE ${OPUS_LIBRARY})

#common request
add_library(common_request common/request.cpp)
target_link_libraries(common_request PUBLIC OpenSSL::SSL)
target_link_libraries(common_request PUBLIC Boost::log)
target_link_libraries(common_request PUBLIC Boost::json)

#config setting
add_library(config_setting config/setting.cpp)
target_link_libraries(config_setting PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(config_setting PUBLIC Boost::log)

add_subdirectory(asr)
add_subdirectory(tts)
add_subdirectory(llm)
add_subdirectory(silero_vad)

#connection library
add_library(connection connection.cpp)
target_link_libraries(connection PUBLIC config_setting)
target_link_libraries(connection PUBLIC common_tools)
target_link_libraries(connection PUBLIC vad)
target_link_libraries(connection PUBLIC asr)
target_link_libraries(connection PUBLIC tts)
target_link_libraries(connection PUBLIC llm)

#server library
add_library(server server.cpp)
target_link_libraries(server PUBLIC connection)

