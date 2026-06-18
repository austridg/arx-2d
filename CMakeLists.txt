cmake_minimum_required(VERSION 3.15)
project(arx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json so editors (clangd / VS Code) resolve includes
# and stop showing false "file not found" squiggles.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# rad2d — consumed as an installed package.
#
# arx is itself installed (so other projects can find_package(arx) and
# #include "arx.hpp"), and an installed package cannot carry a source-tree
# subdirectory. rad2d is installed system-wide, so we resolve it with
# find_package; that also pulls raylib in transitively (rad2d links it PUBLIC).
#
# Note: edits to rad2d source no longer flow straight into arx — reinstall
# rad2d after changing it.
# ---------------------------------------------------------------------------
find_package(rad2d REQUIRED)

# ---------------------------------------------------------------------------
# The engine library.
#
# CONFIGURE_DEPENDS re-globs on each build, so new .cpp files under these
# folders are picked up automatically — you don't have to edit this file every
# time you add one. (app.cpp is the test executable and is intentionally NOT
# part of the library.)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ARX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/entity/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/draw/*.cpp"
)

add_library(arx ${ARX_SOURCES})
add_library(arx::arx ALIAS arx)

target_compile_features(arx PUBLIC cxx_std_17)
target_link_libraries(arx PUBLIC rad2d::rad2d)

# Repo root on the include path so consumers get the umbrella header
# (#include "arx.hpp") and the engine's relative includes resolve.
#
# Installed headers live under <prefix>/include/arx (not flat in include/),
# because the engine shares top-level dir names like draw/ with rad2d — a flat
# install would let the two libraries clobber each other's headers. Pointing the
# install include dir at .../include/arx keeps arx's "draw/draw.h", "entity/..."
# etc. resolving to arx's copies while rad2d keeps its own.
target_include_directories(arx PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/arx>
)

# ---------------------------------------------------------------------------
# The test app — your dogfood game lives in app.cpp.
# Links the engine; the engine drags in rad2d + raylib transitively.
# ---------------------------------------------------------------------------
add_executable(app app.cpp)
target_link_libraries(app PRIVATE arx::arx)

# ---------------------------------------------------------------------------
# install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS arx
    EXPORT arxTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Headers keep their directory layout under include/arx/ so the relative
# includes between them (and the umbrella header) still resolve once installed,
# without colliding with rad2d's same-named top-level dirs (e.g. draw/).
install(FILES arx.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arx)
install(DIRECTORY entity/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arx/entity
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY game/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arx/game
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY draw/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arx/draw
    FILES_MATCHING PATTERN "*.h")

# Export targets so downstream projects can find_package(arx).
install(EXPORT arxTargets
    FILE arxTargets.cmake
    NAMESPACE arx::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arx
)

# ---------------------------------------------------------------------------
# package config
#
# install(EXPORT ...) only writes arxTargets.cmake; find_package(arx) also
# needs a Config file (which re-resolves rad2d) and a version file.
# ---------------------------------------------------------------------------
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arxConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/arxConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arx
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/arxConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/arxConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/arxConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arx
)
