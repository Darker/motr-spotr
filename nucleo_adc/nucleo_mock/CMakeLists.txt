cmake_minimum_required(VERSION 3.20)
# This project contains mock headers so that clangd doesn't go crazy when
# It is looking at the main nucleo project with non-stm32 toolchain
project(nucleo_mock C CXX ASM)

file(GLOB CDC_READER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c") 

set(CUR_PROJECT_NAME nucleo_mock)

add_library(${CUR_PROJECT_NAME} STATIC ${CDC_READER_SOURCES})
target_include_directories(${CUR_PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/mocked)
# target_include_directories(${CUR_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/nucleo_shared)
