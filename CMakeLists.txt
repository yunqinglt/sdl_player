cmake_minimum_required(VERSION 3.21)

project(sdl-player VERSION 0.2.0 LANGUAGES C)

option(SDL_PLAYER_FETCH_SDL2
       "Download and build SDL2 when no system package can be found" ON)
option(SDL_PLAYER_BUILD_TESTS "Build UI and sdl-player tests" ON)
set(SDL_PLAYER_TREELIKE_UI_SOURCE_DIR "" CACHE PATH
    "Use a local treelike_ui checkout instead of fetching it")
set(SDL_PLAYER_TREELIKE_UI_GIT_TAG
    "72e3060d292922e98ed4a3a9d023786801a9d38f" CACHE STRING
    "treelike_ui commit used by FetchContent")
set(SDL_PLAYER_TRUETYPE_FONT_PATH "" CACHE FILEPATH
    "Optional TrueType font copied into the build tree for the UI demo")
set(SDL_PLAYER_PIXEL_FORMAT "RGB565" CACHE STRING "Framebuffer format: RGB565 or RGB888")
set_property(CACHE SDL_PLAYER_PIXEL_FORMAT PROPERTY STRINGS RGB565 RGB888)

if(NOT SDL_PLAYER_PIXEL_FORMAT STREQUAL "RGB565" AND
   NOT SDL_PLAYER_PIXEL_FORMAT STREQUAL "RGB888")
    message(FATAL_ERROR "SDL_PLAYER_PIXEL_FORMAT must be RGB565 or RGB888")
endif()

if(SDL_PLAYER_BUILD_TESTS)
    enable_testing()
endif()

# Keep the UI core as an independent repository. A sibling checkout is used by
# the stm32_in_c superproject; standalone clones fetch the same pinned commit.
set(TREELIKE_UI_BUILD_TESTS ${SDL_PLAYER_BUILD_TESTS} CACHE BOOL
    "Build treelike_ui tests" FORCE)
set(TREELIKE_UI_PIXEL_FORMAT "${SDL_PLAYER_PIXEL_FORMAT}" CACHE STRING
    "Framebuffer format inherited from sdl-player" FORCE)

set(_SDL_PLAYER_TREELIKE_UI_SOURCE "")
if(SDL_PLAYER_TREELIKE_UI_SOURCE_DIR)
    get_filename_component(_SDL_PLAYER_TREELIKE_UI_SOURCE
        "${SDL_PLAYER_TREELIKE_UI_SOURCE_DIR}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
else()
    get_filename_component(_SDL_PLAYER_TREELIKE_UI_SIBLING
        "${CMAKE_CURRENT_SOURCE_DIR}/../treelike-ui" ABSOLUTE)
    if(EXISTS "${_SDL_PLAYER_TREELIKE_UI_SIBLING}/CMakeLists.txt")
        set(_SDL_PLAYER_TREELIKE_UI_SOURCE
            "${_SDL_PLAYER_TREELIKE_UI_SIBLING}")
    endif()
endif()

if(TARGET treelike_ui::treelike_ui)
    set(_SDL_PLAYER_TREELIKE_UI_MODE "existing target")
elseif(_SDL_PLAYER_TREELIKE_UI_SOURCE)
    if(NOT EXISTS "${_SDL_PLAYER_TREELIKE_UI_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR
            "SDL_PLAYER_TREELIKE_UI_SOURCE_DIR does not contain CMakeLists.txt: "
            "${_SDL_PLAYER_TREELIKE_UI_SOURCE}")
    endif()
    add_subdirectory("${_SDL_PLAYER_TREELIKE_UI_SOURCE}"
                     "${CMAKE_CURRENT_BINARY_DIR}/treelike-ui")
    set(_SDL_PLAYER_TREELIKE_UI_MODE
        "local: ${_SDL_PLAYER_TREELIKE_UI_SOURCE}")
else()
    include(FetchContent)
    FetchContent_Declare(
        treelike_ui
        GIT_REPOSITORY https://github.com/yunqinglt/treelike_ui.git
        GIT_TAG ${SDL_PLAYER_TREELIKE_UI_GIT_TAG}
    )
    FetchContent_MakeAvailable(treelike_ui)
    set(_SDL_PLAYER_TREELIKE_UI_MODE
        "fetched: ${SDL_PLAYER_TREELIKE_UI_GIT_TAG}")
endif()

if(NOT TARGET treelike_ui::treelike_ui)
    message(FATAL_ERROR "treelike_ui::treelike_ui target was not created")
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

set(_SDL_PLAYER_FONT_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets")
file(MAKE_DIRECTORY "${_SDL_PLAYER_FONT_ASSET_DIR}")
set(_SDL_PLAYER_BITMAP_FONT_PATH
    "${_SDL_PLAYER_FONT_ASSET_DIR}/demo_bitmap_5x7.tlf")
set(_SDL_PLAYER_VECTOR_FONT_PATH
    "${_SDL_PLAYER_FONT_ASSET_DIR}/demo_vector_stroke.tlf")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/demo_bitmap_5x7.tlf"
    "${_SDL_PLAYER_BITMAP_FONT_PATH}"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/demo_vector_stroke.tlf"
    "${_SDL_PLAYER_VECTOR_FONT_PATH}"
    COPYONLY
)
set(_SDL_PLAYER_TRUETYPE_FONT_BUILD_PATH "")
if(SDL_PLAYER_TRUETYPE_FONT_PATH)
    get_filename_component(_SDL_PLAYER_TRUETYPE_FONT_SOURCE
        "${SDL_PLAYER_TRUETYPE_FONT_PATH}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_SDL_PLAYER_TRUETYPE_FONT_SOURCE}")
        message(FATAL_ERROR
            "SDL_PLAYER_TRUETYPE_FONT_PATH does not exist: "
            "${_SDL_PLAYER_TRUETYPE_FONT_SOURCE}")
    endif()
    set(_SDL_PLAYER_TRUETYPE_FONT_BUILD_PATH
        "${_SDL_PLAYER_FONT_ASSET_DIR}/demo_truetype.ttf")
    configure_file(
        "${_SDL_PLAYER_TRUETYPE_FONT_SOURCE}"
        "${_SDL_PLAYER_TRUETYPE_FONT_BUILD_PATH}"
        COPYONLY
    )
endif()

add_library(demo_core
    app/demo.c
    esp32_effects.c
)
target_include_directories(demo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(demo_core PUBLIC treelike_ui::treelike_ui)
target_compile_definitions(demo_core PRIVATE
    SDL_PLAYER_BITMAP_FONT_PATH="${_SDL_PLAYER_BITMAP_FONT_PATH}"
    SDL_PLAYER_VECTOR_FONT_PATH="${_SDL_PLAYER_VECTOR_FONT_PATH}"
)
if(_SDL_PLAYER_TRUETYPE_FONT_BUILD_PATH)
    target_compile_definitions(demo_core PRIVATE
        SDL_PLAYER_TRUETYPE_FONT_PATH="${_SDL_PLAYER_TRUETYPE_FONT_BUILD_PATH}"
    )
endif()
if(NOT MSVC)
    target_link_libraries(demo_core PUBLIC m)
endif()
target_compile_features(demo_core PUBLIC c_std_11)
sdl_player_enable_warnings(demo_core)

add_library(sdl_platform platform/sdl/sdl_display.c)
target_include_directories(sdl_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdl_platform PUBLIC treelike_ui::treelike_ui
                                   PRIVATE ${SDL_PLAYER_SDL_TARGET})
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
    get_target_property(_SDL_PLAYER_UI_COMPILE_DEFINITIONS
        treelike_ui::treelike_ui INTERFACE_COMPILE_DEFINITIONS)
    set(_SDL_PLAYER_HAS_TRUETYPE FALSE)
    if("TREELIKE_UI_HAS_TRUETYPE=1" IN_LIST
       _SDL_PLAYER_UI_COMPILE_DEFINITIONS)
        set(_SDL_PLAYER_HAS_TRUETYPE TRUE)
    endif()

    add_test(NAME sdl-player-help COMMAND sdl-player --help)
    set_tests_properties(sdl-player-help PROPERTIES
        PASS_REGULAR_EXPRESSION "ui-tree-debug"
    )

    function(sdl_player_add_headless_test name)
        add_test(NAME ${name} COMMAND sdl-player ${ARGN})
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            TIMEOUT 10
        )
    endfunction()

    sdl_player_add_headless_test(
        sdl-player-ui-tree-snapshot
        --phase 3 --frames 1 --ui-tree-debug
    )
    set_tests_properties(sdl-player-ui-tree-snapshot PROPERTIES
        PASS_REGULAR_EXPRESSION "UI-TREE.*event=snapshot"
    )

    if(_SDL_PLAYER_HAS_TRUETYPE AND
       _SDL_PLAYER_TRUETYPE_FONT_BUILD_PATH)
        sdl_player_add_headless_test(
            sdl-player-truetype-font
            --phase 3 --frames 1
        )
        set_tests_properties(sdl-player-truetype-font PROPERTIES
            PASS_REGULAR_EXPRESSION
                "UI-FONT source=truetype family=Fantasque Sans Mono"
        )
    endif()

    sdl_player_add_headless_test(
        sdl-player-ui-tree-join
        --phase 3 --frames 10 --ui-tree-debug
    )
    set_tests_properties(sdl-player-ui-tree-join PROPERTIES
        PASS_REGULAR_EXPRESSION "UI-TREE.*event=join"
    )

    sdl_player_add_headless_test(
        sdl-player-ui-tree-split
        --phase 3 --frames 10 --ui-tree-debug
    )
    set_tests_properties(sdl-player-ui-tree-split PROPERTIES
        PASS_REGULAR_EXPRESSION "UI-TREE.*event=split"
    )

    sdl_player_add_headless_test(
        sdl-player-ui-tree-debug-disabled
        --phase 3 --frames 1
    )
    set_tests_properties(sdl-player-ui-tree-debug-disabled PROPERTIES
        FAIL_REGULAR_EXPRESSION "UI-TREE"
    )
endif()

message(STATUS "sdl-player ${PROJECT_VERSION}")
message(STATUS "treelike_ui: ${_SDL_PLAYER_TREELIKE_UI_MODE}")
message(STATUS "SDL2 target: ${SDL_PLAYER_SDL_TARGET}")
message(STATUS "Pixel format: ${SDL_PLAYER_PIXEL_FORMAT}")
