cmake_minimum_required(VERSION 3.10.0)
project(sdl-player VERSION 0.1.0 LANGUAGES C)

# ===================================================================
#  SDL2 Integration
# ===================================================================

# Option A: Use system-installed SDL2 (if available)
# find_package(SDL2 REQUIRED)

# Option B: Use bundled SDL2 library
# We define the paths to the bundled SDL2 headers and libraries.
set(SDL2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2-2.30.4/include")

# Detect platform architecture and pick the correct library path
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL2_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2-2.30.4/lib/x64")
else()
    set(SDL2_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2-2.30.4/lib/x86")
endif()

set(SDL2_LIBRARIES
    "${SDL2_LIB_DIR}/SDL2.lib"
    "${SDL2_LIB_DIR}/SDL2main.lib"
)

# Link SDL2.dll alongside the executable (Windows)
set(SDL2_DLL "${SDL2_LIB_DIR}/SDL2.dll")

# ===================================================================
#  Source Files
# ===================================================================

set(SOURCES
    main.c
    display.c
    timer.c
    esp32_effects.c
)

# ===================================================================
#  Executable Target
# ===================================================================
add_executable(sdl-player ${SOURCES}
    drawer.c
    54A99503218B6635058DAE67B9FF007B_resized.c)

# Include directories
target_include_directories(sdl-player PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link SDL2 libraries
target_link_libraries(sdl-player PRIVATE ${SDL2_LIBRARIES})

# target_compile_options(sdl-player PRIVATE /Za)

# Set subsystem to WINDOWS to hide the console (optional)
# Remove the comment on the next line if you want a GUI-only app (no console window):
# set_target_properties(sdl-player PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

# ===================================================================
#  Post-build: copy SDL2.dll to the output directory
# ===================================================================

add_custom_command(TARGET sdl-player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DLL}"
        "$<TARGET_FILE_DIR:sdl-player>"
    COMMENT "Copying SDL2.dll to output directory"
)

# ===================================================================
#  Feature summary
# ===================================================================

message(STATUS "sdl-player v${PROJECT_VERSION}")
message(STATUS "SDL2 include: ${SDL2_INCLUDE_DIR}")
message(STATUS "SDL2 libs:    ${SDL2_LIBRARIES}")
message(STATUS "SDL2 DLL:     ${SDL2_DLL}")

