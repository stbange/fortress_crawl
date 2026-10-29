cmake_minimum_required(VERSION 3.16)
project(fortress_crawl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(FC_BUILD_TESTS "Build world generation tests and tools" ON)

# --- SDL3: use an installed copy if present, otherwise fetch and build it statically.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  message(STATUS "SDL3 not found on the system, fetching release-3.2.24")
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
