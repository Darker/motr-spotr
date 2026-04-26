cmake_minimum_required(VERSION 3.20)
project(MotrSpotr C CXX ASM)

set(PRODUCT_NAME motr_spotr)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
# -------------------------------------------------------------
# Detect whether we are using ARM embedded GCC
# -------------------------------------------------------------
include(CheckCXXSourceCompiles)
enable_language(C ASM CXX)
check_cxx_source_compiles("
    #ifndef __arm__
    #error Not ARM
    #endif
    int main() { return 0; }
" COMPILER_DEFINES_ARM)

set(OPENOCD_BIN "" CACHE FILEPATH "Path to openocd executable")
set(OPENOCD_SCRIPTS "" CACHE PATH "Path to OpenOCD scripts directory")

message(STATUS "ARM compiler detected: ${COMPILER_DEFINES_ARM}")

add_custom_target(copy_linker_script
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/stm32cubemx/nucleo_settings/STM32F446XX_FLASH.ld
        ${CMAKE_SOURCE_DIR}/STM32F446XX_FLASH.ld
    COMMENT "Copying linker script to project root"
)
add_subdirectory("nucleo_shared")
add_subdirectory("nucleo_mock")
add_subdirectory("nucleo_main")
add_subdirectory("tpc/imgui")
add_subdirectory("tpc/implot")


# -------------------------------------------------------------
# Host build (Windows/Linux/macOS)
# -------------------------------------------------------------
if(NOT COMPILER_DEFINES_ARM)
    message(STATUS "Building for host platform")
    add_subdirectory("tpc/glfw/glfw")

    add_subdirectory("reader_lib")
    add_subdirectory("adc_reader")
    target_compile_features(adc_reader PRIVATE cxx_std_20)
    target_compile_definitions(adc_reader PRIVATE HOST_BUILD=1)
else()
    # Embedded build (STM32F446RE)
    # -------------------------------------------------------------
    message(STATUS "Building for STM32F446RE")
    add_subdirectory("stm32cubemx/nucleo_settings")
    # This is here because nucleo_settings main.c referencers generated headers
    # from the nucleo main
    target_include_directories(nucleo_settings PRIVATE 
        nucleo_main/include 
        stm32cubemx/nucleo_settings/Core
        stm32cubemx/nucleo_settings/Drivers/STM32F4xx_HAL_Driver/Inc
    )

    # target_sources(nucleo_settings PRIVATE 
    #     stm32cubemx/nucleo_settings/Core/Src/usb_events.c
    # )

    target_link_libraries(nucleo_settings nucleo_main)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/stm32cubemx/nucleo_settings/startup_stm32f446xx.s
        PROPERTIES LANGUAGE ASM
    )


    # target_compile_options(${PRODUCT_NAME} PRIVATE
    #     -mcpu=cortex-m4
    #     -mthumb
    #     -mfpu=fpv4-sp-d16
    #     -mfloat-abi=hard
    #     -ffunction-sections
    #     -fdata-sections
    #     -Wall -Wextra
    # )

    # target_link_options(${PRODUCT_NAME} PRIVATE
    #     -T${CMAKE_SOURCE_DIR}/stm32f446re.ld
    #     -Wl,--gc-sections
    #     -Wl,-Map=${PRODUCT_NAME}.map
    # )

    # # Add HAL/LL/CMSIS sources here
    # target_sources(${PRODUCT_NAME} PRIVATE
    #     Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    #     Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
    #     Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    #     Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
    #     startup_stm32f446xx.s
    # )

    # target_include_directories(${PRODUCT_NAME} PRIVATE
    #     Core/Inc
    #     Drivers/CMSIS/Include
    #     Drivers/CMSIS/Device/ST/STM32F4xx/Include
    #     Drivers/STM32F4xx_HAL_Driver/Inc
    # )

    # -------------------------------------------------------------
    # Produce .bin and .hex
    # -------------------------------------------------------------

    add_custom_target(build_binaries ALL
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:nucleo_settings> ${CMAKE_BINARY_DIR}/motr_spotr.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:nucleo_settings> ${CMAKE_BINARY_DIR}/motr_spotr.hex
        DEPENDS nucleo_settings
    )

    # Validate
    if(NOT EXISTS "${OPENOCD_BIN}")
        message(FATAL_ERROR "OPENOCD_BIN not found at ${OPENOCD_BIN}. Example: -DOPENOCD_BIN=C:/msys64/usr/bin/openocd.exe")
    endif()

    if(NOT EXISTS "${OPENOCD_SCRIPTS}")
        message(FATAL_ERROR "OPENOCD_SCRIPTS is not set or does not exist. Example: -DOPENOCD_SCRIPTS=C:/msys64/usr/share/openocd/scripts")
    endif()

    # -------------------------------------------------------------
    # Flash target (OpenOCD)
    # -------------------------------------------------------------
    add_custom_target(flash_nucleo
        COMMAND ${OPENOCD_BIN}
            -f ${OPENOCD_SCRIPTS}/interface/stlink.cfg
            -f ${OPENOCD_SCRIPTS}/target/stm32f4x.cfg
            -c "program motr_spotr.bin verify reset exit 0x08000000"
        DEPENDS build_binaries
        COMMENT "Flashing STM32F446RE via OpenOCD"
    )
    set_target_properties(flash_nucleo PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
