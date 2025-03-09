function(add_target_with_config target_name)
    add_library(${target_name} ${ARGN} ${HEADER_LIST})
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()

#config setting
add_target_with_config(config_setting config/setting.cpp)
target_link_libraries(config_setting PUBLIC yaml-cpp::yaml-cpp)
target_link_libraries(config_setting PUBLIC Boost::log)

add_subdirectory(silero_vad)

#connection library
add_target_with_config(connection connection.cpp)
target_link_libraries(connection PUBLIC config_setting)
target_link_libraries(connection PUBLIC vad)

#server library
add_target_with_config(server server.cpp)
target_link_libraries(server PUBLIC connection)

