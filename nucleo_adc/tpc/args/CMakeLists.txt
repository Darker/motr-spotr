cmake_minimum_required(VERSION 3.20)
project(args CXX)

set(CUR_PROJECT_NAME args)

add_library(${CUR_PROJECT_NAME} STATIC empty.cpp)
target_include_directories(${CUR_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

