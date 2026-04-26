cmake_minimum_required(VERSION 3.20)
project(nucleo_main C CXX ASM)

file(GLOB NUCLEO_MAIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c") 

add_library(nucleo_main ${NUCLEO_MAIN_SOURCES})
target_include_directories(nucleo_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_sources(${PRODUCT_NAME} PRIVATE ${NUCLEO_MAIN_SOURCES})
target_link_libraries(nucleo_main PRIVATE
    nucleo_shared
)
if(COMPILER_DEFINES_ARM)
    target_link_libraries(nucleo_main PUBLIC
        stm32cubemx
    )
    add_dependencies(nucleo_main copy_linker_script)

    target_include_directories(nucleo_main PRIVATE ${CMAKE_SOURCE_DIR}/stm32cubemx/nucleo_settings/Core)
    target_include_directories(nucleo_main PRIVATE ${CMAKE_SOURCE_DIR}/stm32cubemx/nucleo_settings/Core/Inc)
    target_include_directories(nucleo_main PRIVATE ${CMAKE_SOURCE_DIR}/stm32cubemx/nucleo_settings/Drivers/STM32F4xx_HAL_Driver/Inc)
    target_include_directories(nucleo_main PRIVATE ${CMAKE_SOURCE_DIR}/stm32cubemx/nucleo_settings/Drivers/CMSIS/Device/ST/STM32F4xx/Include)
else()
    target_link_libraries(nucleo_main PRIVATE nucleo_mock)
endif()