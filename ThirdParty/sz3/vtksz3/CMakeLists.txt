if (FALSE)
cmake_minimum_required(VERSION 3.18)
endif ()

set(SZ3_DATA_VERSION 3.3.0)
set(PROJECT_NAME SZ3)
set(PROJECT_VERSION 3.3.0)
set(PROJECT_VERSION_MAJOR 3)
set(PROJECT_VERSION_MINOR 3)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION_TWEAK 0)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/SZ3/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/SZ3/version.hpp"
  @ONLY)

vtk_module_include(VTK::sz3
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${_vtk_build_HEADERS_DESTINATION}/vtksz3/include>)

vtk_module_compile_features(VTK::sz3
  INTERFACE cxx_std_17)

find_package(PkgConfig QUIET)

if (MSVC)
  option(SZ3_USE_BUNDLED_ZSTD "prefer the bundled version of Zstd" ON)
else ()
  option(SZ3_USE_BUNDLED_ZSTD "prefer the bundled version of Zstd" OFF)
endif ()

if (PkgConfig_FOUND)
  pkg_search_module(ZSTD IMPORTED_TARGET libzstd)
endif ()

if (ZSTD_FOUND AND NOT SZ3_USE_BUNDLED_ZSTD)
  if (TARGET PkgConfig::ZSTD)
    vtk_module_link(VTK::sz3 INTERFACE PkgConfig::ZSTD)
  endif ()
else ()
  add_subdirectory(tools/zstd)
  if (TARGET zstd)
    vtk_module_link(VTK::sz3 INTERFACE zstd)
  endif ()
endif ()

vtk_module_install_headers(
  SUBDIR      "vtksz3"
  DIRECTORIES "include"
)

