cmake_minimum_required(VERSION 3.20)
project(nucleo_shared C CXX ASM)

file(GLOB CDC_READER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c") 

set(CUR_PROJECT_NAME nucleo_shared)

add_library(${CUR_PROJECT_NAME} STATIC ${CDC_READER_SOURCES})
target_include_directories(${CUR_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${CUR_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/nucleo_shared)
