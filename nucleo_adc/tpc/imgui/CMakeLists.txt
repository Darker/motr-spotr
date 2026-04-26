cmake_minimum_required(VERSION 3.20)
project(imgui C CXX ASM)

set(CUR_PROJECT_NAME imgui)

set(IMGUI_BACKENDS "")

if (WIN32)
    # Win32 + OpenGL3
    list(APPEND IMGUI_BACKENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_win32.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp
    )

elseif (UNIX AND NOT APPLE)
    # Linux + GLFW + OpenGL3
    list(APPEND IMGUI_BACKENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    )

else()
    message(ERROR "Unsupported platform for ImGui backend selection")
endif()


# file(GLOB_RECUR IMGUI_BACKENDS 
#     "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/*.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/*.c"
# )

add_library(${CUR_PROJECT_NAME}
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
    ${IMGUI_BACKENDS}
)
target_include_directories(${CUR_PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${CUR_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_link_libraries(${CUR_PROJECT_NAME} PRIVATE glfw)