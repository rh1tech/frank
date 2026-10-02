cmake_minimum_required(VERSION 3.13)
set(FW_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(PICO_BOARD_HEADER_DIRS ${FW_ROOT}/boards CACHE PATH "" FORCE)
set(PICO_BOARD "frank_core2_proto_master" CACHE STRING "" FORCE)
set(CPU_SPEED "252" CACHE STRING "")
include(${FW_ROOT}/pico_sdk_import.cmake)
project(frank-core2-probe C CXX ASM)
pico_sdk_init()
add_executable(frank-core2-probe src/main.c)
target_compile_definitions(frank-core2-probe PRIVATE
    CPU_CLOCK_MHZ=${CPU_SPEED}
    CPU_VOLTAGE=VREG_VOLTAGE_1_50
    PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=20000
)
target_link_libraries(frank-core2-probe pico_stdlib hardware_clocks hardware_vreg)
pico_enable_stdio_uart(frank-core2-probe 0)
pico_enable_stdio_usb(frank-core2-probe 1)
pico_add_extra_outputs(frank-core2-probe)
