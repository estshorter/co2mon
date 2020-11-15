add_executable(co2logger main.cpp)
target_include_directories(co2logger PRIVATE ../include/cpp-httplib)
target_include_directories(co2logger PRIVATE ../include/json/single_include/nlohmann)
target_include_directories(co2logger PRIVATE ../)

target_link_libraries(co2logger hidapi)
target_compile_features(co2logger PRIVATE cxx_std_17)
if(NOT WIN32)
	find_package(Threads REQUIRED)
	target_link_libraries(co2logger Threads::Threads)
endif()

add_custom_command(TARGET co2logger POST_BUILD     
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  
         ${PROJECT_SOURCE_DIR}/logger/configs.json 
        $<TARGET_FILE_DIR:co2logger>)              

add_custom_command(TARGET co2logger POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  
        $<TARGET_FILE_DIR:hidapi>/hidapi.dll    
        $<TARGET_FILE_DIR:co2logger>)          