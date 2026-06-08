cmake_minimum_required(VERSION 3.20)

project(CPU_ASM_EMULATOR LANGUAGES CXX)

add_executable(CPU-ASM-EMULATOR
    CPU-ASM-EMULATOR/CPU-ASM-EMULATOR.cpp
    CPU-ASM-EMULATOR/assembler.cpp
    CPU-ASM-EMULATOR/cpu.cpp
)

target_compile_features(CPU-ASM-EMULATOR PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(CPU-ASM-EMULATOR PRIVATE /W3)
else()
    target_compile_options(CPU-ASM-EMULATOR PRIVATE -Wall -Wextra -Wpedantic)
endif()
