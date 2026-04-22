vtk_module_third_party(
  INTERNAL
    LICENSE_FILES
      "vtksz3/copyright-and-BSD-license.txt"
    SPDX_LICENSE_IDENTIFIER
      "SZ3"
    SPDX_COPYRIGHT_TEXT
      "Copyright (c) 2016, UChicago Argonne, LLC"
    SPDX_DOWNLOAD_LOCATION
      "git+https://github.com/szcompressor/SZ3.git@4359047ad03abcbce88a0def45d40d0f3a9c1863"
    VERSION
      "3.3.2-6-g4359047"
    STANDARD_INCLUDE_DIRS
    HEADER_ONLY
  EXTERNAL
    PACKAGE SZ3
    TARGETS SZ3::SZ3
    STANDARD_INCLUDE_DIRS)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/vtk_sz3.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/vtk_sz3.h")

vtk_module_install_headers(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/vtk_sz3.h")
