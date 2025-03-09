include(Catch)

function(add_target_with_config target_name)
    add_executable(${target_name} ${ARGN})
    target_link_libraries(${target_name} PRIVATE Catch2::Catch2WithMain)
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
    catch_discover_tests(${target_name})
    
endfunction()

file(GLOB_RECURSE cpp_files *.cpp)

foreach(cpp_file ${cpp_files})
    get_filename_component(file_name ${cpp_file} NAME_WE)
    add_target_with_config(${file_name} ${cpp_file})
endforeach()

target_link_libraries(test_setting PUBLIC config_setting)