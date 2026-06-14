cmake_minimum_required(VERSION 3.15)
project(ge2d VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json so editors (clangd / VS Code) resolve includes
# and stop showing false "file not found" squiggles.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# rad2d — built from its sibling source tree.
#
# It has no installed CMake package, and edits to rad2d should flow straight
# into the engine, so we compile it from source. This also pulls raylib in
# transitively (rad2d links it PUBLIC), so we don't find raylib separately.
# Override with -DRAD2D_DIR=/path if rad2d lives elsewhere.
# ---------------------------------------------------------------------------
set(RAD2D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../animation"
    CACHE PATH "Path to the rad2d library source tree")
add_subdirectory(${RAD2D_DIR} ${CMAKE_BINARY_DIR}/rad2d)

# ---------------------------------------------------------------------------
# The engine library.
#
# CONFIGURE_DEPENDS re-globs on each build, so new .cpp files under these
# folders are picked up automatically — you don't have to edit this file every
# time you add one. (app.cpp is the test executable and is intentionally NOT
# part of the library.)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GE2D_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/entity/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/draw/*.cpp"
)

add_library(ge2d ${GE2D_SOURCES})

# Repo root on the include path so consumers get the umbrella header
# (#include "engine.hpp") and the engine's relative includes resolve.
target_include_directories(ge2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ge2d PUBLIC rad2d::rad2d)

# ---------------------------------------------------------------------------
# The test app — your dogfood game lives in app.cpp.
# Links the engine; the engine drags in rad2d + raylib transitively.
# ---------------------------------------------------------------------------
add_executable(app app.cpp)
target_link_libraries(app PRIVATE ge2d)
