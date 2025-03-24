include(Catch)

function(add_target_with_config target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE Catch2::Catch2WithMain)
    catch_discover_tests(${target_name})
    
endfunction()

file(GLOB_RECURSE cpp_files *.cpp)

foreach(cpp_file ${cpp_files})
    file(RELATIVE_PATH file_path "${PROJECT_SOURCE_DIR}/tests" "${cpp_file}")
    string(REPLACE "/" "_" file_with_path ${file_path})
    get_filename_component(name_with_path ${file_with_path} NAME_WE)
    add_target_with_config(${name_with_path} ${cpp_file})
    message(STATUS "Add test: ${name_with_path}")
endforeach()

target_link_libraries(config_test_setting PUBLIC config_setting)
target_link_libraries(common_test_request PUBLIC common_request)
target_link_libraries(test_find_last_segment PUBLIC common_tools)