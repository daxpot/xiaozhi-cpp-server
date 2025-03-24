function(add_llm_with_config target_name)
    add_library(${target_name} ${ARGN})
    target_link_libraries(${target_name} PUBLIC config_setting)
    target_link_libraries(${target_name} PUBLIC common_request)
endfunction()

add_llm_with_config(llm_cozev3 cozev3.cpp)
add_llm_with_config(llm_dify dify.cpp)
add_llm_with_config(llm_openai openai.cpp)


add_library(llm base.cpp)
target_link_libraries(llm PUBLIC config_setting)
target_link_libraries(llm PUBLIC llm_cozev3)
target_link_libraries(llm PUBLIC llm_dify)
target_link_libraries(llm PUBLIC llm_openai)