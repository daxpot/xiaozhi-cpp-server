file(GLOB_RECURSE cpp_files *.cpp)

foreach(cpp_file ${cpp_files})
    get_filename_component(file_name ${cpp_file} NAME_WE)
    add_executable("tests_${file_name}" ${cpp_file})
endforeach()

target_link_libraries(tests_asr PUBLIC config_logger)
target_link_libraries(tests_asr PUBLIC asr)

target_link_libraries(tests_tts PUBLIC config_logger)
target_link_libraries(tests_tts PUBLIC tts)

target_link_libraries(tests_llm PUBLIC config_logger)
target_link_libraries(tests_llm PUBLIC llm)
