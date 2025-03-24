add_executable(web_server web_server.cpp)
target_link_libraries(web_server PUBLIC server)
target_link_libraries(web_server PUBLIC config_logger)

add_subdirectory(tests)