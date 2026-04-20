set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)


# Prevent CMake from trying to link a test program
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Bare-metal: no system libraries, no syscalls
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")
