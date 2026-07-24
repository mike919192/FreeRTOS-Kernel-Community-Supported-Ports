cmake_minimum_required(VERSION 3.15)
project(ARM_CA9_Zynq7000)

add_library(freertos_kernel_port STATIC)

target_sources(freertos_kernel_port
    PRIVATE
    port.c
    port_asm_vectors.S
    portASM.S
    portDefaults.h
    portZynq7000.c
    portmacro.h)

add_library(freertos_kernel_port_headers INTERFACE)

target_include_directories(freertos_kernel_port_headers INTERFACE .)

target_link_libraries(freertos_kernel_port
    PRIVATE
    freertos_kernel_port_headers
    freertos_kernel_include)

target_link_libraries(freertos_kernel_port
    INTERFACE
    xil_bsp)
