add_executable(web_server web_server.cpp)
target_link_libraries(web_server PUBLIC server)

add_subdirectory(tests)