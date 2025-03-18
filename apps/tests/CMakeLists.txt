function(add_target_with_config target_name)
    add_executable(${target_name} ${ARGN})
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
    
endfunction()

file(GLOB_RECURSE cpp_files *.cpp)

foreach(cpp_file ${cpp_files})
    get_filename_component(file_name ${cpp_file} NAME_WE)
    add_target_with_config("tests_${file_name}" ${cpp_file})
endforeach()

target_link_libraries(tests_asr PUBLIC config_logger)
target_link_libraries(tests_asr PUBLIC asr_bytedancev2)

target_link_libraries(tests_tts PUBLIC config_logger)
target_link_libraries(tests_tts PUBLIC tts_bytedancev3)

target_link_libraries(tests_cozev3 PUBLIC config_logger)
target_link_libraries(tests_cozev3 PUBLIC llm_cozev3)

target_link_libraries(tests_dify PUBLIC config_logger)
target_link_libraries(tests_dify PUBLIC llm_dify)
