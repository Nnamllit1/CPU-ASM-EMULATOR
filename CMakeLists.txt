cmake_minimum_required(VERSION 3.20)

project(CPU_ASM_EMULATOR LANGUAGES CXX)

include(CTest)

option(CPU_ASM_BUILD_DESKTOP "Build the SDL3/ImGui console development environment" OFF)

add_library(console-core
    console/console_machine.cpp
    console/disassembler.cpp
    console/hardware_profile.cpp
    console/project.cpp
)
target_include_directories(console-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(console-core PUBLIC cxx_std_20)

add_library(experimental-assembler
    console/source_builder.cpp
    desktop/desktop_globals.cpp
    CPU-ASM-EMULATOR/assembler.cpp
)
target_include_directories(experimental-assembler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(experimental-assembler PUBLIC cxx_std_20)

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

if(BUILD_TESTING)
    add_executable(console-core-tests tests/console_core_tests.cpp)
    target_link_libraries(console-core-tests PRIVATE console-core experimental-assembler)
    add_test(NAME console-core-tests COMMAND console-core-tests)
endif()

if(CPU_ASM_BUILD_DESKTOP)
    include(FetchContent)

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.10
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.8
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3 imgui)

    add_library(imgui-sdl3 STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(imgui-sdl3 PUBLIC ${imgui_SOURCE_DIR})
    target_link_libraries(imgui-sdl3 PUBLIC SDL3::SDL3)
    target_compile_features(imgui-sdl3 PUBLIC cxx_std_20)

    add_executable(CPU-ASM-CONSOLE
        desktop/main.cpp
    )
    target_include_directories(CPU-ASM-CONSOLE PRIVATE CPU-ASM-EMULATOR)
    target_link_libraries(CPU-ASM-CONSOLE PRIVATE console-core experimental-assembler imgui-sdl3 SDL3::SDL3)
    target_compile_features(CPU-ASM-CONSOLE PRIVATE cxx_std_20)
endif()
