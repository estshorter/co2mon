if(WIN32)
    add_library(hidapi SHARED ${PROJECT_SOURCE_DIR}/include/hidapi/windows/hid.c)
    target_link_libraries(hidapi setupapi)
else()
    add_library(hidapi SHARED ${PROJECT_SOURCE_DIR}/include/hidapi/linux/hid.c)
endif()

target_include_directories(hidapi PUBLIC ../include/hidapi/hidapi)
target_compile_features(hidapi PRIVATE cxx_std_17)

install(TARGETS hidapi 
        LIBRARY DESTINATION lib)  