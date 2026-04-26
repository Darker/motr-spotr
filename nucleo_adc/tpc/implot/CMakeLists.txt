cmake_minimum_required(VERSION 3.20)
project(implot C CXX ASM)

set(CUR_PROJECT_NAME implot)

add_library(${CUR_PROJECT_NAME}
    "${CMAKE_CURRENT_SOURCE_DIR}/implot/implot_items.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/implot/implot.cpp"
)

target_include_directories(${CUR_PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${CUR_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/imgui)
target_include_directories(${CUR_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/implot)
target_link_libraries(${CUR_PROJECT_NAME} PUBLIC imgui)