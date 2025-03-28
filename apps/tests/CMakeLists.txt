file(GLOB_RECURSE cpp_files *.cpp)

foreach(cpp_file ${cpp_files})
    get_filename_component(file_name ${cpp_file} NAME_WE)
    add_executable("tests_${file_name}" ${cpp_file})
    target_link_libraries("tests_${file_name}" PRIVATE precomp)
    target_link_libraries("tests_${file_name}" PRIVATE common)
endforeach()

target_link_libraries(tests_asr PUBLIC asr)

target_link_libraries(tests_tts PUBLIC tts)

target_link_libraries(tests_llm PUBLIC llm)

target_link_libraries(tests_paraformer PUBLIC paraformer)
target_link_libraries(tests_paraformer PUBLIC ${OPUS_LIBRARY})
