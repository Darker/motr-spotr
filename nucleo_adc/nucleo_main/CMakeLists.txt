cmake_minimum_required(VERSION 3.20)
project(nucleo_main C CXX ASM)

file(GLOB NUCLEO_MAIN_SOURCES "src/*.cpp" "src/*.c") 

add_executable(${PRODUCT_NAME} ${NUCLEO_MAIN_SOURCES})

if(COMPILER_DEFINES_ARM)
    target_link_libraries(${PRODUCT_NAME} PRIVATE
        nucleo_settings
    )
    add_dependencies(${PRODUCT_NAME} copy_linker_script)
endif()