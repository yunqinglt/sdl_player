cmake_minimum_required(VERSION 3.21)

project(sdl-player VERSION 0.2.0 LANGUAGES C)

option(SDL_PLAYER_FETCH_SDL2
       "Download and build SDL2 when no system package can be found" ON)
option(SDL_PLAYER_BUILD_TESTS "Build platform-independent UI tests" ON)
set(SDL_PLAYER_PIXEL_FORMAT "RGB565" CACHE STRING "Framebuffer format: RGB565 or RGB888")
set_property(CACHE SDL_PLAYER_PIXEL_FORMAT PROPERTY STRINGS RGB565 RGB888)

if(NOT SDL_PLAYER_PIXEL_FORMAT STREQUAL "RGB565" AND
   NOT SDL_PLAYER_PIXEL_FORMAT STREQUAL "RGB888")
    message(FATAL_ERROR "SDL_PLAYER_PIXEL_FORMAT must be RGB565 or RGB888")
endif()

# Prefer an installed SDL2 package (Linux distribution, vcpkg, MSYS2, etc.).
find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static)
    find_package(SDL2 MODULE QUIET)
endif()

set(SDL_PLAYER_SDL_TARGET "")
if(TARGET SDL2::SDL2)
    set(SDL_PLAYER_SDL_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    set(SDL_PLAYER_SDL_TARGET SDL2::SDL2-static)
elseif(SDL2_FOUND)
    # Compatibility for older FindSDL2.cmake modules which expose variables
    # but do not create modern imported targets.
    add_library(sdl2_dependency INTERFACE)
    target_include_directories(sdl2_dependency SYSTEM INTERFACE
                               ${SDL2_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR})
    target_link_libraries(sdl2_dependency INTERFACE
                          ${SDL2_LIBRARIES} ${SDL2_LIBRARY})
    set(SDL_PLAYER_SDL_TARGET sdl2_dependency)
endif()

if(NOT SDL_PLAYER_SDL_TARGET AND SDL_PLAYER_FETCH_SDL2)
    include(FetchContent)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.11
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL2)
    if(TARGET SDL2::SDL2)
        set(SDL_PLAYER_SDL_TARGET SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(SDL_PLAYER_SDL_TARGET SDL2::SDL2-static)
    endif()
endif()

if(NOT SDL_PLAYER_SDL_TARGET)
    message(FATAL_ERROR
        "SDL2 was not found. Install libsdl2-dev (Linux), provide SDL2_DIR "
        "(vcpkg/MSYS2/Windows), or configure with SDL_PLAYER_FETCH_SDL2=ON.")
endif()

function(sdl_player_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(ui_core
    ui/ui_surface.c
    ui/ui_drawer.c
)
target_include_directories(ui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(ui_core PUBLIC c_std_11)
sdl_player_enable_warnings(ui_core)

if(SDL_PLAYER_PIXEL_FORMAT STREQUAL "RGB888")
    target_compile_definitions(ui_core PUBLIC PIXEL_FORMAT_RGB888)
else()
    target_compile_definitions(ui_core PUBLIC PIXEL_FORMAT_RGB565)
endif()

add_library(demo_core
    app/demo.c
    esp32_effects.c
)
target_link_libraries(demo_core PUBLIC ui_core)
if(NOT MSVC)
    target_link_libraries(demo_core PUBLIC m)
endif()
target_compile_features(demo_core PUBLIC c_std_11)
sdl_player_enable_warnings(demo_core)

add_library(sdl_platform platform/sdl/sdl_display.c)
target_link_libraries(sdl_platform PUBLIC ui_core PRIVATE ${SDL_PLAYER_SDL_TARGET})
target_compile_features(sdl_platform PUBLIC c_std_11)
sdl_player_enable_warnings(sdl_platform)

add_executable(sdl-player main.c)
target_link_libraries(sdl-player PRIVATE demo_core sdl_platform)
target_compile_features(sdl-player PRIVATE c_std_11)
sdl_player_enable_warnings(sdl-player)

# Copy the SDL2 runtime DLL next to the executable for MSVC/vcpkg and for the
# FetchContent SDL2 build. No hard-coded x86/x64 paths are required.
if(WIN32 AND SDL_PLAYER_SDL_TARGET STREQUAL "SDL2::SDL2")
    add_custom_command(TARGET sdl-player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2>
                $<TARGET_FILE_DIR:sdl-player>
        COMMENT "Copying SDL2 runtime DLLs"
    )
endif()

if(SDL_PLAYER_BUILD_TESTS)
    enable_testing()
    add_executable(ui-core-tests tests/ui_core_tests.c)
    target_link_libraries(ui-core-tests PRIVATE ui_core)
    target_compile_features(ui-core-tests PRIVATE c_std_11)
    sdl_player_enable_warnings(ui-core-tests)
    add_test(NAME ui-core-tests COMMAND ui-core-tests)
endif()

message(STATUS "sdl-player ${PROJECT_VERSION}")
message(STATUS "SDL2 target: ${SDL_PLAYER_SDL_TARGET}")
message(STATUS "Pixel format: ${SDL_PLAYER_PIXEL_FORMAT}")
