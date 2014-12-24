# - Find libxo
# This module defines the following variables:
#     LIBXO_FOUND         - whether or not we found libxo
#     LIBXO_INCLUDE_DIRS  - include dirs for libxo
#     LIBXO_LIBRARIES     - libs needed to use libxo

find_path(LIBXO_PREFIX include/libxo/xo.h REQUIRED)
find_library(LIBXO_LIBRARY xo REQUIRED)

set(LIBXO_INCLUDE_DIR "${LIBXO_PREFIX}/include"
  CACHE STRING "Include directory for libxo header")
set(LIBXO_INCLUDE_DIRS "${LIBXO_INCLUDE_DIR}")
set(LIBXO_LIBRARIES "${LIBXO_LIBRARY}")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBXO DEFAULT_MSG
  LIBXO_LIBRARY LIBXO_INCLUDE_DIR
)

mark_as_advanced (LIBXO_INCLUDE_DIR LIBXO_LIBRARY)
