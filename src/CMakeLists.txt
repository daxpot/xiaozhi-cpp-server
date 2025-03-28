add_subdirectory(common)
add_subdirectory(paraformer)
add_subdirectory(asr)
add_subdirectory(tts)
add_subdirectory(llm)
add_subdirectory(silero_vad)


#server library
add_library(server connection.cpp server.cpp)
target_link_libraries(server PUBLIC precomp)
target_link_libraries(server PUBLIC common)
target_link_libraries(server PUBLIC vad)
target_link_libraries(server PUBLIC asr)
target_link_libraries(server PUBLIC tts)
target_link_libraries(server PUBLIC llm)

