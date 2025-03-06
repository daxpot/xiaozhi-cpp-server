include(Catch)

function(add_target_with_config target_name)
    file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/xz-cpp-server/**/*.h")
    add_executable(${target_name} ${ARGN} ${HEADER_LIST})
    target_link_libraries(${target_name} PRIVATE Catch2::Catch2WithMain)
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(${target_name} PUBLIC "${PROJECT_SOURCE_DIR}/include")
    source_group(
        TREE "${PROJECT_SOURCE_DIR}/include"
        PREFIX "Header Files"
        FILES ${HEADER_LIST}
    )
    catch_discover_tests(${target_name})
    
endfunction()

file(GLOB_RECURSE cpp_files *.cpp)

foreach(cpp_file ${cpp_files})
    get_filename_component(file_name ${cpp_file} NAME_WE)
    add_target_with_config(${file_name} ${cpp_file})
endforeach()

target_link_libraries(test_setting PUBLIC config_setting)