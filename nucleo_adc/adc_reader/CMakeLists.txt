cmake_minimum_required(VERSION 3.20)
project(adc_reader C CXX ASM)

file(GLOB CDC_READER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c") 

set(CUR_PROJECT_NAME adc_reader)

add_executable(${CUR_PROJECT_NAME} ${CDC_READER_SOURCES})
target_include_directories(${CUR_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${CUR_PROJECT_NAME} PUBLIC implot imgui)
target_link_libraries(${CUR_PROJECT_NAME} PRIVATE glfw nucleo_shared reader_lib args)