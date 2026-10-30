cmake_minimum_required(VERSION 3.16)
project(fortress_crawl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

if(EMSCRIPTEN)
  option(FC_BUILD_TESTS "Build world generation tests and tools" OFF)
else()
  option(FC_BUILD_TESTS "Build world generation tests and tools" ON)
endif()

# --- SDL3: use an installed copy if present, otherwise fetch and build it statically.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  message(STATUS "SDL3 not found on the system, fetching release-3.2.24")
  if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    find_path(FC_XINPUT2_H X11/extensions/XInput2.h)
    if(NOT FC_XINPUT2_H)
      message(WARNING "XInput2 headers not found: SDL3 will be built without relative mouse mode on X11 "
                      "(WSLg uses X11). The game falls back to a cursor grab, but for proper mouse look install "
                      "libxi-dev (Ubuntu/Debian) and delete the build directory before configuring again.")
    endif()
  endif()
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.24
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

if(MSVC)
  set(FC_WARN /W3 /D_CRT_SECURE_NO_WARNINGS)
else()
  set(FC_WARN -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# --- World library: pure C, no SDL/GL, so tests and tools can use it headless.
add_library(fcworld STATIC
  src/core/noise.c
  src/world/block.c
  src/world/theme.c
  src/world/texgen.c
  src/world/layout.c
  src/world/gen_common.c
  src/world/gen_chunk.c
  src/world/gen_struct.c
  src/world/gen_exterior.c
  src/world/mesher.c)
target_include_directories(fcworld PUBLIC src)
target_compile_options(fcworld PRIVATE ${FC_WARN})
if(NOT MSVC)
  target_link_libraries(fcworld PUBLIC m)
endif()

add_executable(fortress_crawl
  src/main.c
  src/engine/gl.c
  src/engine/shader.c
  src/engine/font.c
  src/engine/jobs.c
  src/game/player.c
  src/game/physics.c
  src/game/skills.c
  src/game/game.c
  src/game/monsters.c
  src/game/combat.c
  src/game/explore.c
  src/render/renderer.c
  src/render/batch.c
  src/world/chunkmap.c)
target_link_libraries(fortress_crawl PRIVATE fcworld SDL3::SDL3)
target_compile_options(fortress_crawl PRIVATE ${FC_WARN})

if(EMSCRIPTEN)
  # Web build: WebGL 2, single thread, page from web/shell.html. Output: index.html + .js + .wasm
  set_target_properties(fortress_crawl PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_options(fortress_crawl PRIVATE
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sSTACK_SIZE=1048576
    -sENVIRONMENT=web --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html)
  set_property(TARGET fortress_crawl APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
endif()

if(FC_BUILD_TESTS)
  add_executable(test_worldgen tests/test_worldgen.c)
  target_link_libraries(test_worldgen PRIVATE fcworld)
  target_compile_options(test_worldgen PRIVATE ${FC_WARN})

  add_executable(worldmap tools/worldmap.c)
  target_link_libraries(worldmap PRIVATE fcworld)
  target_compile_options(worldmap PRIVATE ${FC_WARN})

  enable_testing()
  add_test(NAME worldgen COMMAND test_worldgen)
endif()
